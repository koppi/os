/**
 * @file hda.c
 * @brief Intel HD Audio driver: controller reset, CORB/RIRB command rings, a
 *        codec walk to a DAC -> output-pin path, blocking one-shot PCM, and a
 *        cyclic buffer for streamed playback on output stream 0.
 *
 * Deliberately minimal: one codec, one DAC, 44.1/48 kHz 16-bit stereo, polled
 * (no interrupts). DMA structures live in identity-mapped .bss / kheap
 * (phys == virt); the register block (BAR0) is mapped 1:1 cache-disabled.
 *
 * Three things make this work on a real laptop rather than only under QEMU,
 * and all three are invisible failures — the codec answers every verb, the
 * stream runs, the DMA position advances, and nothing comes out:
 *
 *   - Intel's NoSnoop bit. With it set the controller fetches the sample
 *     buffer without snooping the caches, so a ring refilled by ordinary
 *     cached writes plays stale memory. Cleared in @ref hda_probe.
 *
 *   - Which pin. Picking the lowest-numbered output-capable pin lands on
 *     SPDIF or on a pin with nothing behind it as often as not. The walk uses
 *     each pin's default configuration instead, and follows its connection
 *     list to the DAC that actually feeds it.
 *
 *   - Amplifier GPIOs. A MacBook's speaker amp hangs off a codec GPIO rather
 *     than the pin's EAPD bit, and powers up off. See @ref apply_codec_quirks.
 */
#include <hda.h>

#include <pci.h>
#include <io.h>
#include <mm.h>
#include <kheap.h>
#include <paging.h>
#include <pit.h>
#include <cmdline.h>
#include <lib/string.h>
#include <log.h>

static void io_wait(void) { inportb(0x80); }

/* ------------------------------------------------------------------ *
 *  Controller registers (BAR0)                                        *
 * ------------------------------------------------------------------ */
#define REG_GCAP      0x00
#define REG_GCTL      0x08
#define REG_STATESTS  0x0E
#define REG_CORBLBASE 0x40
#define REG_CORBUBASE 0x44
#define REG_CORBWP    0x48
#define REG_CORBRP    0x4A
#define REG_CORBCTL   0x4C
#define REG_CORBSIZE  0x4E
#define REG_RIRBLBASE 0x50
#define REG_RIRBUBASE 0x54
#define REG_RIRBWP    0x58
#define REG_RINTCNT   0x5A
#define REG_RIRBCTL   0x5C
#define REG_RIRBSTS   0x5D
#define REG_RIRBSIZE  0x5E

#define GCTL_CRST  (1u << 0)

/* Stream descriptor fields (relative to the descriptor base). */
#define SD_CTL   0x00
#define SD_STS   0x03
#define SD_LPIB  0x04
#define SD_CBL   0x08
#define SD_LVI   0x0C
#define SD_FMT   0x12
#define SD_BDLPL 0x18
#define SD_BDLPU 0x1C
#define SDCTL_SRST (1u << 0)
#define SDCTL_RUN  (1u << 1)

/* Codec verbs. */
#define V_GET_PARAM      0xF00
#define V_GET_CONN_LIST  0xF02
#define V_GET_CFG_DEF    0xF1C
#define V_SET_CONN_SEL   0x701
#define V_SET_STREAM_CHN 0x706
#define V_SET_PIN_CTL    0x707
#define V_SET_POWER      0x705
#define V_SET_EAPD       0x70C
#define V_SET_GPIO_DATA  0x715
#define V_SET_GPIO_MASK  0x716
#define V_SET_GPIO_DIR   0x717
#define V4_SET_FORMAT    0x2      /* 4-bit verb, 16-bit payload */
#define V4_SET_AMP       0x3      /* 4-bit verb, 16-bit payload */

#define P_VENDOR_ID    0x00
#define P_NODE_COUNT   0x04
#define P_FN_TYPE      0x05
#define P_WIDGET_CAP   0x09
#define P_PCM          0x0A
#define P_CONN_LIST_LEN 0x0E
#define P_PIN_CAP      0x0C
#define P_AMP_OUT_CAP  0x12

#define WTYPE(cap)  (((cap) >> 20) & 0xF)
#define WT_DAC 0x0
#define WT_MIX 0x2                /* mixer: sums its inputs */
#define WT_SEL 0x3                /* selector: picks one input */
#define WT_PIN 0x4

/* AC_WCAP bits used here. */
#define WCAP_OUT_AMP   (1u << 2)
#define WCAP_AMP_OVRD  (1u << 3)
#define WCAP_CONN_LIST (1u << 8)
#define WCAP_DIGITAL   (1u << 9)
#define WCAP_POWER     (1u << 10)

/* AC_PINCAP bits used here. */
#define PINCAP_HP_DRV  (1u << 3)
#define PINCAP_OUT     (1u << 4)
#define PINCAP_EAPD    (1u << 16)

/* Pin default configuration (V_GET_CFG_DEF). The BIOS — or, on a Mac, the
 * codec's own config ROM — describes what each pin is physically wired to.
 * Choosing an output by these rather than by "lowest NID that can do output"
 * is what keeps the driver off SPDIF and off pins with nothing behind them. */
#define CFG_PORT_CONN(c) (((c) >> 30) & 0x3)
#define CFG_DEVICE(c)    (((c) >> 20) & 0xF)
#define CONN_NONE     0x1         /* no physical connection: never usable */
#define DEV_LINE_OUT  0x0
#define DEV_SPEAKER   0x1
#define DEV_HP_OUT    0x2

/* Pin widget control (V_SET_PIN_CTL). */
#define PINCTL_OUT_EN (1u << 6)
#define PINCTL_HP_EN  (1u << 7)

/* ------------------------------------------------------------------ *
 *  State                                                              *
 * ------------------------------------------------------------------ */
static uint8_t *mmio;
static uint32_t corb[256] __attribute__((aligned(128)));
static uint64_t rirb[256] __attribute__((aligned(128)));
static struct { uint64_t addr; uint32_t len; uint32_t flags; }
               bdl[4] __attribute__((aligned(128)));

static int      have_hda;
static uint32_t osd_base;
static uint8_t  codec_addr, afg_nid, dac_nid, pin_nid;
static uint32_t codec_vid;
static uint8_t  corb_wp, rirb_rp;

/* Every analog output pin that can reach dac_nid, so a laptop drives its
 * speaker and its headphone jack from the same stream. Without jack detection
 * there is no way to know which one the user is listening to, and feeding both
 * costs nothing: the unused one is simply not connected to anything. */
#define MAX_OUT_PINS 4
static struct out_path {
    uint8_t pin;        /**< The output pin widget. */
    uint8_t pin_sel;    /**< Connection index on @c pin that reaches the DAC. */
    uint8_t mid;        /**< Mixer/selector between pin and DAC, 0 if direct. */
    uint8_t mid_sel;    /**< Connection index on @c mid that reaches the DAC. */
} out_path[MAX_OUT_PINS];
static int n_out_paths;

/* Output-amp the volume keys drive: the node with an adjustable gain/mute amp
 * (DAC first, then the output pin) and its step count from AMP_OUT_CAP. */
static uint8_t  vol_nid;
static uint8_t  vol_steps;

/* Streamed-playback state (see hda_stream_*). */
static int      streaming;
static int      stream_primed;
static uint32_t stream_half_bytes;
static int      stream_playing_half;
static int16_t  stream_ring[HDA_STREAM_HALF_FRAMES * 2 /* ch */ * 2 /* halves */]
                __attribute__((aligned(4096)));

static uint16_t r16(uint32_t o)             { return *(volatile uint16_t *)(mmio + o); }
static uint32_t r32(uint32_t o)             { return *(volatile uint32_t *)(mmio + o); }
static void     w8 (uint32_t o, uint8_t v)  { *(volatile uint8_t  *)(mmio + o) = v; }
static void     w16(uint32_t o, uint16_t v) { *(volatile uint16_t *)(mmio + o) = v; }
static void     w32(uint32_t o, uint32_t v) { *(volatile uint32_t *)(mmio + o) = v; }

/* ------------------------------------------------------------------ *
 *  CORB / RIRB verb transport                                         *
 * ------------------------------------------------------------------ */
static uint32_t verb(uint8_t nid, uint32_t v, uint32_t payload, int is4) {
    uint32_t cmd = ((uint32_t) codec_addr << 28) | ((uint32_t) nid << 20);
    cmd |= is4 ? ((v << 16) | (payload & 0xFFFF))
               : ((v << 8)  | (payload & 0xFF));

    corb_wp = (corb_wp + 1) & 0xFF;
    corb[corb_wp] = cmd;
    w16(REG_CORBWP, corb_wp);

    for (int i = 0; i < 200000; i++) {
        if ((r16(REG_RIRBWP) & 0xFF) != rirb_rp) {
            rirb_rp = (rirb_rp + 1) & 0xFF;
            return (uint32_t) rirb[rirb_rp];
        }
        io_wait();
    }
    return 0xFFFFFFFF;
}

static uint32_t v12(uint8_t nid, uint32_t v, uint32_t p)  { return verb(nid, v, p, 0); }
static uint32_t v4 (uint8_t nid, uint32_t v, uint32_t p)  { return verb(nid, v, p, 1); }
static uint32_t param(uint8_t nid, uint8_t p)             { return v12(nid, V_GET_PARAM, p); }

/* ------------------------------------------------------------------ *
 *  Codec walk                                                         *
 * ------------------------------------------------------------------ */

/** @brief Put a widget into D0, if it has its own power control. */
static void power_up(uint8_t nid) {
    if (param(nid, P_WIDGET_CAP) & WCAP_POWER)
        v12(nid, V_SET_POWER, 0);
}

/**
 * @brief Read a widget's connection list.
 * @param nid Widget to query.
 * @param out Receives the connected NIDs.
 * @param max Capacity of @p out.
 * @return Number of entries written.
 *
 * Range entries (short form, bit 7 set) are taken as literal NIDs with the
 * marker masked off rather than expanded. Laptop output paths list their
 * inputs individually, so the distinction has not come up; a codec that does
 * use ranges would simply have fewer candidates to search.
 */
static int conn_list(uint8_t nid, uint8_t *out, int max) {
    if (!(param(nid, P_WIDGET_CAP) & WCAP_CONN_LIST))
        return 0;
    uint32_t len = param(nid, P_CONN_LIST_LEN);
    int longform = (len >> 7) & 1;
    int n = (int) (len & 0x7F);
    if (n > max)
        n = max;

    int got = 0;
    if (longform) {
        for (int i = 0; i < n && got < n; i += 2) {
            uint32_t r = v12(nid, V_GET_CONN_LIST, i);
            for (int k = 0; k < 2 && got < n; k++)
                out[got++] = (r >> (k * 16)) & 0xFF;
        }
    } else {
        for (int i = 0; i < n && got < n; i += 4) {
            uint32_t r = v12(nid, V_GET_CONN_LIST, i);
            for (int k = 0; k < 4 && got < n; k++)
                out[got++] = (r >> (k * 8)) & 0x7F;
        }
    }
    return got;
}

/** @brief Is @p nid an analog digital-to-analog converter? */
static int is_analog_dac(uint8_t nid) {
    uint32_t cap = param(nid, P_WIDGET_CAP);
    return WTYPE(cap) == WT_DAC && !(cap & WCAP_DIGITAL);
}

/**
 * @brief Find a route from an output pin back to a DAC.
 * @param pin The pin to start from.
 * @param p   Receives the route on success.
 * @return The DAC's NID, or 0 if the pin reaches none.
 *
 * Searches the pin's own connection list first, then one level deeper through
 * a mixer or selector — which covers every laptop codec seen so far. Pairing
 * "first DAC" with "first pin" the way this used to is wrong on any codec
 * where the speaker hangs off a different converter than the headphone jack,
 * the Cirrus parts in a MacBook among them.
 */
static uint8_t route_pin_to_dac(uint8_t pin, struct out_path *p) {
    uint8_t list[16];
    int n = conn_list(pin, list, (int) sizeof list);

    for (int i = 0; i < n; i++)
        if (is_analog_dac(list[i])) {
            p->pin = pin; p->pin_sel = (uint8_t) i; p->mid = 0; p->mid_sel = 0;
            return list[i];
        }

    for (int i = 0; i < n; i++) {
        uint32_t cap = param(list[i], P_WIDGET_CAP);
        if (WTYPE(cap) != WT_MIX && WTYPE(cap) != WT_SEL)
            continue;
        uint8_t sub[16];
        int m = conn_list(list[i], sub, (int) sizeof sub);
        for (int k = 0; k < m; k++)
            if (is_analog_dac(sub[k])) {
                p->pin = pin; p->pin_sel = (uint8_t) i;
                p->mid = list[i]; p->mid_sel = (uint8_t) k;
                return sub[k];
            }
    }
    return 0;
}

/**
 * @brief Rank an output pin by what the codec says it is wired to.
 * @return A score; higher wins, 0 means unusable.
 *
 * The built-in speaker outranks the headphone jack because it is always
 * there — with no jack detection, picking the jack on a machine with nothing
 * plugged into it would be silence.
 */
static int pin_score(uint8_t pin) {
    uint32_t cap = param(pin, P_PIN_CAP);
    if (!(cap & PINCAP_OUT) || (param(pin, P_WIDGET_CAP) & WCAP_DIGITAL))
        return 0;

    uint32_t cfg = v12(pin, V_GET_CFG_DEF, 0);
    if (cfg == 0xFFFFFFFFu || CFG_PORT_CONN(cfg) == CONN_NONE)
        return 0;

    switch (CFG_DEVICE(cfg)) {
    case DEV_SPEAKER:  return 3;
    case DEV_HP_OUT:   return 2;
    case DEV_LINE_OUT: return 1;
    default:           return 0;   /* SPDIF, modem, anything that is an input */
    }
}

/** @brief Log one line per output pin: what it claims to be and where it goes. */
static void dump_out_pins(uint8_t w0, uint8_t wc) {
    for (uint8_t w = w0; w < w0 + wc; w++) {
        uint32_t cap = param(w, P_WIDGET_CAP);
        if (WTYPE(cap) != WT_PIN)
            continue;
        uint32_t pc = param(w, P_PIN_CAP);
        if (!(pc & PINCAP_OUT))
            continue;
        uint32_t cfg = v12(w, V_GET_CFG_DEF, 0);
        klogf(LOG_INFO, "hda: pin %u cfg %08x conn %u dev %u score %d%s\n",
              w, cfg, CFG_PORT_CONN(cfg), CFG_DEVICE(cfg), pin_score(w),
              (cap & WCAP_DIGITAL) ? " digital" : "");
    }
}

/**
 * @brief Find a codec and the best analog output path on it.
 * @return Non-zero once @ref dac_nid and @ref out_path describe a usable path.
 *
 * Picks the highest-scoring pin that routes to a DAC, then adds every other
 * usable pin that reaches the same DAC so speaker and headphones both play.
 */
static int find_output_path(void) {
    for (uint8_t cad = 0; cad < 15; cad++) {
        if (!(r16(REG_STATESTS) & (1u << cad)))
            continue;
        codec_addr = cad;
        rirb_rp = r16(REG_RIRBWP) & 0xFF;

        uint32_t vid = param(0, P_VENDOR_ID);
        if (vid + 1 <= 1)                          /* 0 or 0xFFFFFFFF */
            continue;

        uint32_t root = param(0, P_NODE_COUNT);
        uint8_t fg0 = (root >> 16) & 0xFF, fgn = root & 0xFF;

        for (uint8_t fg = fg0; fg < fg0 + fgn; fg++) {
            if ((param(fg, P_FN_TYPE) & 0x7F) != 0x01)
                continue;
            v12(fg, V_SET_POWER, 0);

            uint32_t wn = param(fg, P_NODE_COUNT);
            uint8_t w0 = (wn >> 16) & 0xFF, wc = wn & 0xFF;

            klogf(LOG_INFO, "hda: codec %u vendor %08x afg %u widgets %u..%u\n",
                  cad, vid, fg, w0, w0 + wc - 1);
            if (cmdline_has("hdadebug"))
                dump_out_pins(w0, wc);

            /* Best pin first. */
            int best = 0;
            uint8_t best_pin = 0;
            struct out_path best_path = { 0, 0, 0, 0 };
            uint8_t best_dac = 0;
            for (uint8_t w = w0; w < w0 + wc; w++) {
                int sc = pin_score(w);
                if (sc <= best)
                    continue;
                struct out_path p;
                uint8_t dac = route_pin_to_dac(w, &p);
                if (!dac)
                    continue;
                best = sc; best_pin = w; best_path = p; best_dac = dac;
            }
            /* Fall back to the old pairing if nothing routed. A pin that is
             * hardwired to its converter publishes no connection list, so the
             * search above finds no route and would otherwise reject a codec
             * that used to work. */
            if (!best) {
                int dac = -1, pin = -1, pin_sc = 0;
                for (uint8_t w = w0; w < w0 + wc; w++) {
                    if (dac < 0 && is_analog_dac(w))
                        dac = w;
                    int sc = pin_score(w);
                    if (sc > pin_sc) { pin_sc = sc; pin = w; }
                }
                if (dac < 0 || pin < 0)
                    continue;
                best = pin_sc;
                best_dac = (uint8_t) dac;
                best_pin = (uint8_t) pin;
                best_path.pin = (uint8_t) pin;
                best_path.pin_sel = 0;
                best_path.mid = 0;
                best_path.mid_sel = 0;
                klogf(LOG_INFO, "hda: no routable pin, assuming DAC %d -> pin %d\n",
                      dac, pin);
            }

            afg_nid = fg;
            dac_nid = best_dac;
            pin_nid = best_pin;
            out_path[0] = best_path;
            n_out_paths = 1;

            /* Everything else that lands on the same DAC. */
            for (uint8_t w = w0; w < w0 + wc && n_out_paths < MAX_OUT_PINS; w++) {
                if (w == best_pin || !pin_score(w))
                    continue;
                struct out_path p;
                if (route_pin_to_dac(w, &p) == dac_nid)
                    out_path[n_out_paths++] = p;
            }
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Turn on the amplifier GPIOs a Mac's Cirrus codec needs.
 *
 * On a MacBook the internal speaker amp is not wired to the pin's EAPD bit but
 * to a codec GPIO, and it comes up off. The codec enumerates and accepts every
 * verb either way, the stream runs, the DMA position advances — and nothing
 * comes out of the speaker. Nothing in the HD Audio spec says which GPIO, so
 * these come from the per-codec tables in Linux's patch_cirrus.c.
 *
 * Sent to the function group, which is where the GPIO block lives.
 */
static void apply_codec_quirks(void) {
    uint32_t gpio;

    switch (codec_vid) {
    case 0x10134208:            /* CS4208: MacBook Air 6,x / MacBook Pro 11,x */
        gpio = 0x01;            /*   GPIO0 = speaker amp                      */
        break;
    case 0x10134206:            /* CS4206/CS4207: earlier MacBooks            */
    case 0x10134207:
        gpio = 0x0A;            /*   GPIO1 = headphone amp, GPIO3 = speaker   */
        break;
    default:
        return;
    }

    v12(afg_nid, V_SET_GPIO_MASK, gpio);
    v12(afg_nid, V_SET_GPIO_DIR,  gpio);
    v12(afg_nid, V_SET_GPIO_DATA, gpio);
    klogf(LOG_INFO, "hda: codec %08x: amp GPIOs 0x%x enabled\n", codec_vid, gpio);
}

/* ------------------------------------------------------------------ *
 *  Playback                                                           *
 * ------------------------------------------------------------------ */
static uint16_t fmt_for_rate(uint32_t rate) {
    return (rate >= 44000 && rate < 48000) ? 0x4011 /* 44.1k */ : 0x0011 /* 48k */;
}

/** @brief Reset output stream 0 (SRST toggle) and wait for each edge. */
static void osd_reset(uint32_t sd) {
    w8(sd + SD_CTL, SDCTL_SRST);
    for (int i = 0; i < 1000 && !(r32(sd + SD_CTL) & SDCTL_SRST); i++) io_wait();
    w8(sd + SD_CTL, 0);
    for (int i = 0; i < 1000 && (r32(sd + SD_CTL) & SDCTL_SRST); i++) io_wait();
}

/** @brief NumSteps of a node's output amp (0 = fixed / no output amp).
 *  Honours the widget's own AMP_OUT_CAP only when it overrides the AFG default
 *  (WCAP_OUT_AMP = out amp present, WCAP_AMP_OVRD = amp caps override). */
static uint8_t amp_out_steps(uint8_t nid) {
    uint32_t wc = param(nid, P_WIDGET_CAP);
    if (!(wc & WCAP_OUT_AMP))
        return 0;
    uint32_t cap = (wc & WCAP_AMP_OVRD) ? param(nid, P_AMP_OUT_CAP)
                                        : param(afg_nid, P_AMP_OUT_CAP);
    return (cap >> 8) & 0x7F;                       /* NumSteps */
}

/**
 * @brief Unmute a node's output amp and open it to full gain.
 *
 * The gain index is taken from the node's own step count rather than a fixed
 * 0x7F: on a codec with fewer steps than that, an out-of-range index is
 * ignored and the amp stays wherever it powered up — usually muted.
 */
static void amp_open_out(uint8_t nid) {
    uint8_t steps = amp_out_steps(nid);
    v4(nid, V4_SET_AMP, 0xB000 | (steps ? steps : 0x7F));
}

/**
 * @brief Point the DAC at stream @p strm / @p fmt and open every output pin.
 *
 * Walks each route found by @ref find_output_path: the intermediate mixer or
 * selector, if there is one, then the pin — selecting the connection that
 * actually reaches the DAC, unmuting the amp at every stage, and enabling the
 * pin's own external amplifier where it has one.
 */
static void codec_bind_output(uint16_t fmt, uint8_t strm) {
    power_up(afg_nid);
    power_up(dac_nid);
    v4 (dac_nid, V4_SET_FORMAT, fmt);
    v12(dac_nid, V_SET_STREAM_CHN, strm << 4);
    amp_open_out(dac_nid);

    for (int i = 0; i < n_out_paths; i++) {
        struct out_path *p = &out_path[i];

        if (p->mid) {
            power_up(p->mid);
            v12(p->mid, V_SET_CONN_SEL, p->mid_sel);
            amp_open_out(p->mid);
        }

        power_up(p->pin);
        v12(p->pin, V_SET_CONN_SEL, p->pin_sel);

        uint32_t pc = param(p->pin, P_PIN_CAP);
        uint8_t ctl = PINCTL_OUT_EN;
        if (pc & PINCAP_HP_DRV)
            ctl |= PINCTL_HP_EN;                   /* headphone jacks need the
                                                    * drive amp as well */
        v12(p->pin, V_SET_PIN_CTL, ctl);
        amp_open_out(p->pin);
        if (pc & PINCAP_EAPD)
            v12(p->pin, V_SET_EAPD, 0x02);         /* external amp / EAPD on */
    }
}

/** @brief Choose the node the volume keys will attenuate: the DAC if its output
 *  amp is adjustable, else the output pin. Called once, after the path is found. */
static void pick_volume_node(void) {
    vol_steps = amp_out_steps(dac_nid);
    vol_nid   = dac_nid;
    if (!vol_steps) {
        vol_steps = amp_out_steps(pin_nid);
        vol_nid   = pin_nid;
    }
}

void hda_set_volume(int pct) {
    if (!have_hda)
        return;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;

    /* SET_AMP payload: bit15 output amp, bit13/12 left/right, bit7 mute,
     * bits6-0 gain index (0 = most attenuated). */
    uint16_t amp = 0xB000;
    if (pct == 0)
        amp |= (1u << 7);                          /* mute */
    else if (vol_steps)
        amp |= (pct * vol_steps + 50) / 100;       /* scaled gain index */
    else
        amp |= 0x7F;                               /* fixed amp: just unmute */

    v4(vol_nid, V4_SET_AMP, amp);
}

void hda_play_pcm(const int16_t *samples, uint32_t nframes, uint32_t rate) {
    if (!have_hda || streaming || !samples || !nframes)
        return;   /* streamed playback owns the one output stream */

    uint32_t bytes = nframes * 4;
    void *buf = kmalloc(bytes < 4096 ? 4096 : bytes);
    if (!buf)
        return;
    memcpy(buf, (void *) samples, bytes);

    uint16_t fmt = fmt_for_rate(rate);
    uint32_t sd = osd_base;
    uint8_t strm = 1;

    osd_reset(sd);

    memset(bdl, 0, sizeof(bdl));
    bdl[0].addr  = (uint32_t) buf;
    bdl[0].len   = bytes;
    bdl[0].flags = 1;                              /* IOC */

    w32(sd + SD_BDLPL, (uint32_t) &bdl[0]);
    w32(sd + SD_BDLPU, 0);
    w32(sd + SD_CBL, bytes);
    w16(sd + SD_LVI, 0);
    w16(sd + SD_FMT, fmt);
    w32(sd + SD_CTL, (r32(sd + SD_CTL) & 0x000FFFFF) | ((uint32_t) strm << 20));

    codec_bind_output(fmt, strm);

    w8(sd + SD_STS, 0x1C);
    w32(sd + SD_CTL, r32(sd + SD_CTL) | SDCTL_RUN);

    pit_busywait_ms((nframes * 1000u) / (rate ? rate : 48000u) + 20);

    w32(sd + SD_CTL, r32(sd + SD_CTL) & ~SDCTL_RUN);
    kfree(buf);
}

void hda_beep(uint32_t freq, uint32_t ms) {
    if (!have_hda || streaming || !freq || !ms)
        return;   /* the MOD stream has the output stream; skip the blip */
    uint32_t rate = 48000;
    uint32_t nframes = (rate * ms) / 1000;
    if (nframes > rate) nframes = rate;            /* cap at 1 s */
    int16_t *s = kmalloc(nframes * 4);
    if (!s)
        return;
    uint32_t half = rate / (freq * 2);
    if (!half) half = 1;
    for (uint32_t i = 0; i < nframes; i++) {
        int16_t v = ((i / half) & 1) ? 8000 : -8000;
        s[i * 2] = s[i * 2 + 1] = v;
    }
    hda_play_pcm(s, nframes, rate);
    kfree(s);
}

/* ------------------------------------------------------------------ *
 *  Streamed playback (cyclic double buffer, polled)                   *
 * ------------------------------------------------------------------ */
#define STREAM_HALF   HDA_STREAM_HALF_FRAMES
#define STREAM_FRAMES (STREAM_HALF * 2)
#define STREAM_BYTES  (STREAM_FRAMES * 4)          /* stereo s16 */

int hda_stream_start(uint32_t rate) {
    if (!have_hda)
        return 0;
    if (streaming)
        return 1;

    uint16_t fmt = fmt_for_rate(rate);
    uint32_t sd  = osd_base;
    uint8_t  strm = 1;

    memset(stream_ring, 0, sizeof(stream_ring));
    stream_half_bytes   = STREAM_HALF * 4;
    stream_playing_half = 0;
    stream_primed       = 0;

    osd_reset(sd);

    /* Two BDL entries over one contiguous ring: the DMA engine loops back to
     * entry 0 after entry 1, so the buffer plays forever while RUN is set. */
    memset(bdl, 0, sizeof(bdl));
    bdl[0].addr  = (uint32_t) &stream_ring[0];
    bdl[0].len   = stream_half_bytes;
    bdl[0].flags = 1;                              /* IOC */
    bdl[1].addr  = (uint32_t) &stream_ring[STREAM_HALF * 2];
    bdl[1].len   = stream_half_bytes;
    bdl[1].flags = 1;                              /* IOC */

    w32(sd + SD_BDLPL, (uint32_t) &bdl[0]);
    w32(sd + SD_BDLPU, 0);
    w32(sd + SD_CBL, STREAM_BYTES);
    w16(sd + SD_LVI, 1);
    w16(sd + SD_FMT, fmt);
    w32(sd + SD_CTL, (r32(sd + SD_CTL) & 0x000FFFFF) | ((uint32_t) strm << 20));

    codec_bind_output(fmt, strm);

    w8(sd + SD_STS, 0x1C);
    w32(sd + SD_CTL, r32(sd + SD_CTL) | SDCTL_RUN);

    streaming = 1;
    return 1;
}

void hda_stream_stop(void) {
    if (!streaming)
        return;
    w32(osd_base + SD_CTL, r32(osd_base + SD_CTL) & ~SDCTL_RUN);
    streaming = 0;
}

void hda_stream_service(void (*fill)(int16_t *dst, uint32_t nframes)) {
    if (!streaming || !fill)
        return;

    /* Link position in buffer -> which half the DMA engine is reading now.
     * Once it has crossed into the other half, the one it left is free to
     * refill. We poll far faster than a half drains, so at most one crossing
     * is pending per call. */
    uint32_t lpib = r32(osd_base + SD_LPIB);
    int cur = (lpib >= stream_half_bytes) ? 1 : 0;

    if (!stream_primed) {
        /* Get real audio into the half the engine has not reached yet, so
         * only the first ~90 ms (one half) plays as silence. */
        int ahead = !cur;
        fill(&stream_ring[ahead * STREAM_HALF * 2], STREAM_HALF);
        stream_playing_half = cur;
        stream_primed = 1;
        return;
    }

    if (cur != stream_playing_half) {
        fill(&stream_ring[stream_playing_half * STREAM_HALF * 2], STREAM_HALF);
        stream_playing_half = cur;
    }
}

int hda_present(void) { return have_hda; }

int hda_is_apple_cirrus(void) {
    return have_hda && (codec_vid == 0x10134208 ||
                        codec_vid == 0x10134206 ||
                        codec_vid == 0x10134207);
}

/* ------------------------------------------------------------------ *
 *  Bring-up                                                           *
 * ------------------------------------------------------------------ */
void hda_probe(struct pci_device *dev) {
    if (have_hda || cmdline_has("nosound") || cmdline_has("nohda"))
        return;   /* a laptop has two HDA controllers (analog + HDMI); one is enough */

    pci_device_t *d = (pci_device_t *) dev;

    /* Intel integrated-HDMI ("iHD") controllers are digital-only. A laptop
     * carries one alongside the PCH analog controller; if we take the first
     * (usually the HDMI one, e.g. Haswell 0x0a0c on a MacBook Air), there is
     * no audible output. Skip them and let the analog controller bind. */
    static const uint16_t hdmi_only[] = {
        0x0a0c, 0x0c0c, 0x0d0c,      /* Haswell / Broadwell iHD */
        0x160c, 0x170c,              /* Skylake / Kaby Lake iHD */
        0x280c, 0x281c               /* Alder Lake iHD */
    };
    if (d->vendor == 0x8086)
        for (unsigned i = 0; i < sizeof(hdmi_only) / sizeof(hdmi_only[0]); i++)
            if (d->device == hdmi_only[i]) {
                klogf(LOG_INFO, "hda: skipping digital-only HDMI controller 8086:%04x\n",
                      d->device);
                return;
            }
    uint32_t bar = 0, span = 0;
    for (int i = 0; i < 6; i++)
        if (!d->bar[i].is_io && d->bar[i].addr) {
            bar  = d->bar[i].addr;
            span = d->bar[i].size;
            break;
        }
    if (!bar) {
        klogf(LOG_WARNING, "hda: no MMIO BAR\n");
        return;
    }
    if (span < 0x4000)
        span = 0x4000;                             /* spec register space */

    pci_enable(d, PCI_CMD_MEM | PCI_CMD_MASTER);

    /* Intel PCH controllers come out of reset with two settings that break
     * DMA playback from a normally-cached buffer:
     *
     *   TCSEL (0x44) selects the PCI traffic class for stream DMA. Anything
     *   but class 0 is not guaranteed to be snooped on the way to memory.
     *
     *   DEVC (0x78) bit 11 is NoSnoop. With it set the controller reads the
     *   sample buffer without probing the CPU caches, so it fetches whatever
     *   RAM held before the last writeback. The MOD player refills its ring
     *   through ordinary cached writes, which is exactly the case that
     *   breaks: the stream runs, LPIB advances, and the codec plays stale
     *   memory — silence, or a fragment of the first buffer over and over.
     *
     * Linux does the same two writes in its Intel init path. */
    if (d->vendor == 0x8086) {
        uint32_t tc = pci_cfg_read32(d, 0x44);
        if (tc & 0x07)
            pci_cfg_write32(d, 0x44, tc & ~0x07u);
        uint16_t devc = pci_cfg_read16(d, 0x78);
        if (devc & (1u << 11)) {
            pci_cfg_write16(d, 0x78, devc & ~(uint16_t) (1u << 11));
            klogf(LOG_INFO, "hda: Intel NoSnoop cleared (DEVC was %04x)\n", devc);
        }
    }

    /* Map the whole register block, not just the first page: the output
     * stream descriptors sit past 0x80 and a controller with many streams
     * pushes them further out. Shared into every address space so a console
     * `beep` reaches the registers from a user process's directory too. */
    for (uint32_t o = 0; o < span; o += PAGE_SIZE)
        if (!vmm_map_phys(get_kern_directory(), bar + o, bar + o,
                          PAGE_PRESENT | PAGE_RW | PAGE_PCD | PAGE_PWT)) {
            klogf(LOG_WARNING, "hda: cannot map MMIO at 0x%x, skipping\n", bar + o);
            return;
        }
    vmm_share_kernel_range(bar, span);
    mmio = (uint8_t *) bar;

    w32(REG_GCTL, r32(REG_GCTL) & ~GCTL_CRST);
    for (int i = 0; i < 1000 && (r32(REG_GCTL) & GCTL_CRST); i++) io_wait();
    w32(REG_GCTL, r32(REG_GCTL) | GCTL_CRST);
    for (int i = 0; i < 1000 && !(r32(REG_GCTL) & GCTL_CRST); i++) io_wait();
    pit_busywait_ms(1);

    if (!(r16(REG_STATESTS) & 0x7FFF)) {
        klogf(LOG_INFO, "hda: no codecs\n");
        return;
    }

    w8(REG_CORBCTL, 0);
    w8(REG_RIRBCTL, 0);
    w8(REG_CORBSIZE, 0x02);
    w8(REG_RIRBSIZE, 0x02);
    w32(REG_CORBLBASE, (uint32_t) &corb[0]);  w32(REG_CORBUBASE, 0);
    w32(REG_RIRBLBASE, (uint32_t) &rirb[0]);  w32(REG_RIRBUBASE, 0);

    w16(REG_CORBRP, 0x8000);
    for (int i = 0; i < 1000 && !(r16(REG_CORBRP) & 0x8000); i++) io_wait();
    w16(REG_CORBRP, 0);
    for (int i = 0; i < 1000 && (r16(REG_CORBRP) & 0x8000); i++) io_wait();
    w16(REG_CORBWP, 0);
    corb_wp = 0;

    w16(REG_RIRBWP, 0x8000);
    w16(REG_RINTCNT, 0xFF);
    rirb_rp = 0;

    w8(REG_CORBCTL, 0x02);
    w8(REG_RIRBCTL, 0x02);

    uint16_t gcap = r16(REG_GCAP);
    osd_base = 0x80 + ((gcap >> 8) & 0xF) * 0x20;

    if (!find_output_path()) {
        klogf(LOG_WARNING, "hda: no DAC/output-pin path\n");
        return;
    }

    codec_vid = param(0, P_VENDOR_ID);
    apply_codec_quirks();
    pick_volume_node();

    have_hda = 1;
    klogf(LOG_INFO, "hda: codec %u (%08x)  DAC %u -> pin %u  %d output(s)  "
          "osd 0x%x  vol nid %u/%u steps\n",
          codec_addr, codec_vid, dac_nid, pin_nid, n_out_paths,
          osd_base, vol_nid, vol_steps);
    for (int i = 0; i < n_out_paths; i++)
        klogf(LOG_INFO, "hda:   out %d: pin %u sel %u%s\n", i,
              out_path[i].pin, out_path[i].pin_sel,
              out_path[i].mid ? " (via mixer)" : "");
}
