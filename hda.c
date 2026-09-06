/**
 * @file hda.c
 * @brief Intel HD Audio driver: controller reset, CORB/RIRB command rings,
 *        a codec walk to a DAC -> output-pin path, and blocking one-shot PCM
 *        playback on output stream 0.
 *
 * Deliberately minimal: one codec, one output path, 48 kHz / 16-bit / stereo,
 * polled (no interrupts). Enough for the console "beep" and short clips. DMA
 * structures live in identity-mapped .bss / kheap (phys == virt); the register
 * block (BAR0) is mapped 1:1 cache-disabled.
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
#define V_SET_CONN_SEL   0x701
#define V_SET_STREAM_CHN 0x706
#define V_SET_PIN_CTL    0x707
#define V_SET_POWER      0x705
#define V_SET_EAPD       0x70C
#define V4_SET_FORMAT    0x2      /* 4-bit verb, 16-bit payload */
#define V4_SET_AMP       0x3      /* 4-bit verb, 16-bit payload */

#define P_VENDOR_ID   0x00
#define P_NODE_COUNT  0x04
#define P_FN_TYPE     0x05
#define P_WIDGET_CAP  0x09
#define P_PIN_CAP     0x0C
#define P_AMP_OUT_CAP 0x12

#define WTYPE(cap)  (((cap) >> 20) & 0xF)
#define WT_DAC 0x0
#define WT_PIN 0x4

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
static uint8_t  corb_wp, rirb_rp;

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
static int find_output_path(void) {
    for (uint8_t cad = 0; cad < 15; cad++) {
        if (!(r16(REG_STATESTS) & (1u << cad)))
            continue;
        codec_addr = cad;
        rirb_rp = r16(REG_RIRBWP) & 0xFF;

        if (param(0, P_VENDOR_ID) + 1 <= 1)        /* 0 or 0xFFFFFFFF */
            continue;

        uint32_t root = param(0, P_NODE_COUNT);
        uint8_t fg0 = (root >> 16) & 0xFF, fgn = root & 0xFF;

        for (uint8_t fg = fg0; fg < fg0 + fgn; fg++) {
            if ((param(fg, P_FN_TYPE) & 0x7F) != 0x01)
                continue;
            v12(fg, V_SET_POWER, 0);

            uint32_t wn = param(fg, P_NODE_COUNT);
            uint8_t w0 = (wn >> 16) & 0xFF, wc = wn & 0xFF;

            int dac = -1, pin = -1;
            for (uint8_t w = w0; w < w0 + wc; w++) {
                uint32_t cap = param(w, P_WIDGET_CAP);
                if (WTYPE(cap) == WT_DAC && dac < 0)
                    dac = w;
                else if (WTYPE(cap) == WT_PIN && pin < 0 &&
                         (param(w, P_PIN_CAP) & (1u << 4)))
                    pin = w;
            }
            if (dac >= 0 && pin >= 0) {
                afg_nid = fg;
                dac_nid = dac;
                pin_nid = pin;
                return 1;
            }
        }
    }
    return 0;
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

/** @brief Point the DAC at stream @p strm / @p fmt and open the output pin. */
static void codec_bind_output(uint16_t fmt, uint8_t strm) {
    v4 (dac_nid, V4_SET_FORMAT, fmt);
    v12(dac_nid, V_SET_STREAM_CHN, strm << 4);
    v4 (dac_nid, V4_SET_AMP, 0xB000 | 0x7F);       /* out amp, L+R, gain 0x7F */
    v12(pin_nid, V_SET_CONN_SEL, 0);
    v12(pin_nid, V_SET_PIN_CTL, 0x40);             /* output enable */
    v4 (pin_nid, V4_SET_AMP, 0xB000 | 0x7F);
    v12(pin_nid, V_SET_EAPD, 0x02);                /* external amp / EAPD on */
}

/** @brief NumSteps of a node's output amp (0 = fixed / no output amp).
 *  Honours the widget's own AMP_OUT_CAP only when it overrides the AFG default
 *  (WIDGET_CAP bit 2 = out amp present, bit 3 = amp caps override). */
static uint8_t amp_out_steps(uint8_t nid) {
    uint32_t wc = param(nid, P_WIDGET_CAP);
    if (!(wc & (1u << 2)))
        return 0;
    uint32_t cap = (wc & (1u << 3)) ? param(nid, P_AMP_OUT_CAP)
                                    : param(afg_nid, P_AMP_OUT_CAP);
    return (cap >> 8) & 0x7F;                       /* NumSteps */
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

/* ------------------------------------------------------------------ *
 *  Bring-up                                                           *
 * ------------------------------------------------------------------ */
void hda_probe(struct pci_device *dev) {
    if (have_hda || cmdline_has("nosound") || cmdline_has("nohda"))
        return;   /* a laptop has two HDA controllers (analog + HDMI); one is enough */

    pci_device_t *d = (pci_device_t *) dev;
    uint32_t bar = 0, span = 0x4000;
    for (int i = 0; i < 6; i++)
        if (!d->bar[i].is_io && d->bar[i].addr) {
            bar = d->bar[i].addr;
            if (d->bar[i].size) span = d->bar[i].size;
            break;
        }
    if (!bar) {
        klogf(LOG_WARNING, "hda: no MMIO BAR\n");
        return;
    }

    pci_enable(d, PCI_CMD_MEM | PCI_CMD_MASTER);
    for (uint32_t o = 0; o < span; o += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), bar + o, bar + o,
                     PAGE_PRESENT | PAGE_RW | PAGE_PCD | PAGE_PWT);
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

    pick_volume_node();

    have_hda = 1;
    klogf(LOG_INFO, "hda: codec %u  DAC %u -> pin %u  osd 0x%x  vol nid %u/%u steps\n",
          codec_addr, dac_nid, pin_nid, osd_base, vol_nid, vol_steps);
}
