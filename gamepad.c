/**
 * @file gamepad.c
 * @brief USB game controllers: HID report-descriptor parser and report decoder.
 * @see gamepad.h
 */
#include <gamepad.h>
#include <log.h>
#include <lib/string.h>

/** Fields a pad's reports can hold that this driver reads. A pad has a dozen
 *  to forty (buttons are one field each); the rest of the descriptor -- vendor
 *  data, motion sensors, LEDs -- is walked past, not kept. */
#define GP_MAX_FIELDS 64

/** Report IDs whose running bit position the parser tracks at once. */
#define GP_MAX_REPORTS 8

/** Depth of the descriptor's Push / Pop global-state stack. */
#define GP_PUSH_DEPTH 4

/** Usages a single Input item can name before the extras are dropped. */
#define GP_MAX_USAGES 32

/** Elements of one Input item the parser looks at (its Report Count is
 *  whatever the device claims, up to 2^32). */
#define GP_MAX_ELEMENTS 128

/** @name Field kinds */
///@{
#define FK_BUTTON 1   /**< id = button index (0 = button 1) */
#define FK_AXIS   2   /**< id = AXU_* until roles are assigned, then GR_* */
#define FK_HAT    3   /**< hat switch, folded into the four dpad bits */
#define FK_DPAD   4   /**< a D-pad direction as a button; id = a GAMEPAD_DPAD_ bit */
///@}

/** Axis usages as the descriptor names them. */
enum { AXU_X, AXU_Y, AXU_Z, AXU_RX, AXU_RY, AXU_RZ, AXU_BRAKE, AXU_ACCEL, AXU_COUNT };

/** What an axis is used for, once the set the pad has is known (see assign_roles). */
enum { GR_NONE, GR_LX, GR_LY, GR_RX, GR_RY, GR_LT, GR_RT, GR_TRIGGERS };

/** One value a report carries: where it is and how to read it. */
typedef struct {
    uint8_t  kind;
    uint8_t  id;
    uint8_t  report_id;
    uint8_t  size;        /**< bits */
    uint8_t  is_signed;   /**< logical minimum below zero: sign-extend */
    uint16_t bit;         /**< offset in the report, after the report ID byte */
    int32_t  lmin, lmax;  /**< logical range */
} gp_field_t;

typedef struct {
    int      used;
    uint8_t  xinput;
    uint8_t  uses_id;     /**< reports carry a leading report ID byte */
    uint8_t  nfields;
    uint8_t  nbuttons;    /**< highest button number named */
    uint8_t  axis_role[AXU_COUNT];
    uint8_t  has_hat, has_dpad;
    uint16_t vendor, product;
    gp_field_t f[GP_MAX_FIELDS];

    /* Seqlock: odd while a report is being applied. Written by the USB thread,
     * read by any CPU through gamepad_get(). */
    volatile uint32_t seq;
    gamepad_state_t   st;
    uint8_t  hat_dirs;    /**< dpad bits from the hat */
    uint8_t  btn_dirs;    /**< dpad bits from D-pad "buttons" */
} pad_t;

static pad_t pads[GAMEPAD_MAX];
static uint8_t rdesc_buf[GAMEPAD_RDESC_MAX];
static int watch;

uint8_t *gamepad_rdesc_buf(void) {
    return rdesc_buf;
}

/* ------------------------------------------------------------------ *
 *  Report descriptor parser                                           *
 * ------------------------------------------------------------------ */

/** Global items; Push saves them, Pop restores. */
typedef struct {
    uint16_t usage_page;
    int32_t  lmin, lmax;
    uint32_t size, count;
    uint8_t  report_id;
} gp_global_t;

/** Parser state across one descriptor. */
typedef struct {
    gp_global_t g;
    gp_global_t stack[GP_PUSH_DEPTH];
    int         sp;

    /* Local items, cleared after every Main item. A usage carries its page in
     * the high half, so one value says both. */
    uint32_t    usage[GP_MAX_USAGES];
    int         nusage;
    uint32_t    umin, umax;
    int         has_range;

    int         depth;        /**< collection nesting */
    int         in_pad;       /**< inside a joystick / game pad application */
    int         found_app;

    uint8_t     rid[GP_MAX_REPORTS];     /**< report IDs seen */
    uint16_t    pos[GP_MAX_REPORTS];     /**< running input bit position per ID */
    int         nrid;
} gp_parse_t;

static int32_t item_signed(const uint8_t *d, int size) {
    switch(size) {
        case 1:  return (int8_t) d[0];
        case 2:  return (int16_t) (d[0] | (d[1] << 8));
        case 4:  return (int32_t) ((uint32_t) d[0] | ((uint32_t) d[1] << 8) |
                                   ((uint32_t) d[2] << 16) | ((uint32_t) d[3] << 24));
        default: return 0;
    }
}

static uint32_t item_unsigned(const uint8_t *d, int size) {
    switch(size) {
        case 1:  return d[0];
        case 2:  return (uint32_t) (d[0] | (d[1] << 8));
        case 4:  return (uint32_t) item_signed(d, 4);
        default: return 0;
    }
}

/** @brief The running bit position for report @p id, allocating it on first use. */
static uint16_t *report_pos(gp_parse_t *ps, uint8_t id) {
    for(int i = 0; i < ps->nrid; i++)
        if(ps->rid[i] == id)
            return &ps->pos[i];
    if(ps->nrid == GP_MAX_REPORTS)
        return 0;
    ps->rid[ps->nrid] = id;
    ps->pos[ps->nrid] = 0;
    return &ps->pos[ps->nrid++];
}

/** @brief The usage the @p i'th element of the current Input item stands for. */
static uint32_t usage_for(const gp_parse_t *ps, uint32_t i) {
    if(ps->has_range) {
        uint32_t u = ps->umin + i;
        return u > ps->umax ? ps->umax : u;
    }
    if(ps->nusage == 0)
        return 0;
    return ps->usage[i < (uint32_t) ps->nusage ? i : (uint32_t) ps->nusage - 1];
}

/**
 * @brief Record the field for one element of an Input item, if it is one a
 *        pad is read from.
 */
static void add_field(pad_t *p, const gp_parse_t *ps, uint32_t usage, uint32_t bit) {
    uint16_t page = (uint16_t) (usage >> 16);
    uint16_t id   = (uint16_t) (usage & 0xFFFF);
    uint8_t  kind = 0, fid = 0;

    /* Nothing to read in no bits, and nothing a pad has needs more than 32. */
    if(ps->g.size == 0 || ps->g.size > 32)
        return;

    if(page == 0x09) {                       /* Button */
        if(id < 1 || id > 32)
            return;
        kind = FK_BUTTON;
        fid  = (uint8_t) (id - 1);
        if(id > p->nbuttons)
            p->nbuttons = (uint8_t) id;
    } else if(page == 0x01) {                /* Generic Desktop */
        switch(id) {
            case 0x30: kind = FK_AXIS; fid = AXU_X;  break;
            case 0x31: kind = FK_AXIS; fid = AXU_Y;  break;
            case 0x32: kind = FK_AXIS; fid = AXU_Z;  break;
            case 0x33: kind = FK_AXIS; fid = AXU_RX; break;
            case 0x34: kind = FK_AXIS; fid = AXU_RY; break;
            case 0x35: kind = FK_AXIS; fid = AXU_RZ; break;
            case 0x39: kind = FK_HAT;  p->has_hat = 1; break;
            case 0x90: kind = FK_DPAD; fid = GAMEPAD_DPAD_UP;    p->has_dpad = 1; break;
            case 0x91: kind = FK_DPAD; fid = GAMEPAD_DPAD_DOWN;  p->has_dpad = 1; break;
            case 0x92: kind = FK_DPAD; fid = GAMEPAD_DPAD_RIGHT; p->has_dpad = 1; break;
            case 0x93: kind = FK_DPAD; fid = GAMEPAD_DPAD_LEFT;  p->has_dpad = 1; break;
            default: return;
        }
    } else if(page == 0x02) {                /* Simulation Controls */
        if(id == 0xC4)      { kind = FK_AXIS; fid = AXU_ACCEL; }  /* right trigger */
        else if(id == 0xC5) { kind = FK_AXIS; fid = AXU_BRAKE; }  /* left trigger  */
        else return;
    } else {
        return;
    }

    if(p->nfields >= GP_MAX_FIELDS)
        return;
    gp_field_t *f = &p->f[p->nfields++];
    f->kind      = kind;
    f->id        = fid;
    f->report_id = ps->g.report_id;
    f->size      = (uint8_t) ps->g.size;
    f->is_signed = ps->g.lmin < 0;
    f->bit       = (uint16_t) bit;
    f->lmin      = ps->g.lmin;
    f->lmax      = ps->g.lmax;
}

/** @brief Does this (page << 16 | usage) head a collection a pad is read from? */
static int is_pad_usage(uint32_t u) {
    return u == 0x00010004 ||    /* Joystick */
           u == 0x00010005 ||    /* Game Pad */
           u == 0x00010008;      /* Multi-axis Controller */
}

/**
 * @brief Decide what each axis the pad has is for.
 *
 * X and Y are always the left stick. The right stick is the Rx/Ry pair, or the
 * Z/Rz pair, whichever the pad has -- and when it has both, Z/Rz is the stick
 * and Rx/Ry are the triggers: a DualShock 4 numbers it that way, while an Xbox
 * pad in DirectInput mode has Rx/Ry for the stick and a single Z for both
 * triggers (left pushes it one way, right the other). Accelerator and Brake,
 * the simulation controls a Bluetooth Xbox pad uses, are the triggers outright.
 *
 * These are conventions, not facts: the descriptor names an axis, not what it
 * is bolted to. They cover the pads there are; a pad that does otherwise has
 * the wrong stick moving, and `pad` on the console shows which.
 */
static void assign_roles(pad_t *p) {
    int has[AXU_COUNT] = {0};
    for(int i = 0; i < p->nfields; i++)
        if(p->f[i].kind == FK_AXIS)
            has[p->f[i].id] = 1;

    uint8_t *r = p->axis_role;
    memset(r, GR_NONE, AXU_COUNT);
    r[AXU_X] = GR_LX;
    r[AXU_Y] = GR_LY;

    int zrz   = has[AXU_Z]  && has[AXU_RZ];
    int rxry  = has[AXU_RX] && has[AXU_RY];
    if(zrz) {
        r[AXU_Z]  = GR_RX;
        r[AXU_RZ] = GR_RY;
        if(rxry) {
            r[AXU_RX] = GR_LT;
            r[AXU_RY] = GR_RT;
        }
    } else if(rxry) {
        r[AXU_RX] = GR_RX;
        r[AXU_RY] = GR_RY;
        if(has[AXU_Z])
            r[AXU_Z] = GR_TRIGGERS;
    }
    r[AXU_BRAKE] = GR_LT;
    r[AXU_ACCEL] = GR_RT;

    /* From here on a field's id is its role. */
    for(int i = 0; i < p->nfields; i++)
        if(p->f[i].kind == FK_AXIS)
            p->f[i].id = r[p->f[i].id];
}

/**
 * @brief Walk @p d and fill in @p p with the fields of a joystick / game pad.
 * @return 1 if the descriptor holds one, 0 if not.
 */
static int parse_descriptor(pad_t *p, const uint8_t *d, int len) {
    gp_parse_t ps;
    memset(&ps, 0, sizeof(ps));
    ps.g.report_id = 0;

    for(int i = 0; i < len; ) {
        uint8_t pre = d[i];
        if(pre == 0xFE) {                           /* long item: skip it */
            if(i + 1 >= len)
                break;
            i += 3 + d[i + 1];
            continue;
        }
        int size = pre & 3;
        if(size == 3)
            size = 4;
        if(i + 1 + size > len)
            break;
        const uint8_t *data = d + i + 1;
        int type = (pre >> 2) & 3;
        int tag  = pre >> 4;
        i += 1 + size;

        if(type == 1) {                             /* Global */
            switch(tag) {
                case 0:  ps.g.usage_page = (uint16_t) item_unsigned(data, size); break;
                case 1:  ps.g.lmin = item_signed(data, size); break;
                case 2:  /* Linux's rule: the maximum is unsigned unless the
                          * minimum is negative, so 0..255 in one byte is not
                          * read as 0..-1. */
                         ps.g.lmax = ps.g.lmin < 0 ? item_signed(data, size)
                                                   : (int32_t) item_unsigned(data, size);
                         break;
                case 7:  ps.g.size  = item_unsigned(data, size); break;
                case 8:  ps.g.report_id = (uint8_t) item_unsigned(data, size);
                         p->uses_id = 1;
                         break;
                case 9:  ps.g.count = item_unsigned(data, size); break;
                case 10: if(ps.sp < GP_PUSH_DEPTH) ps.stack[ps.sp++] = ps.g; break;
                case 11: if(ps.sp > 0) ps.g = ps.stack[--ps.sp]; break;
                default: break;                     /* physical range, unit */
            }
        } else if(type == 2) {                      /* Local */
            uint32_t v = item_unsigned(data, size);
            if(size <= 2)
                v |= (uint32_t) ps.g.usage_page << 16;
            switch(tag) {
                case 0:  if(ps.nusage < GP_MAX_USAGES) ps.usage[ps.nusage++] = v; break;
                case 1:  ps.umin = v; ps.has_range = 1; break;
                case 2:  ps.umax = v; break;
                default: break;                     /* designators, strings, delimiter */
            }
        } else if(type == 0) {                      /* Main */
            uint32_t flags = item_unsigned(data, size);
            switch(tag) {
                case 10:                            /* Collection */
                    ps.depth++;
                    if(ps.depth == 1) {
                        ps.in_pad = flags == 1 && ps.nusage > 0 &&
                                    is_pad_usage(ps.usage[0]);
                        if(ps.in_pad)
                            ps.found_app = 1;
                    }
                    break;
                case 12:                            /* End Collection */
                    if(ps.depth > 0 && --ps.depth == 0)
                        ps.in_pad = 0;
                    break;
                case 8: {                           /* Input */
                    uint16_t *pos = report_pos(&ps, ps.g.report_id);
                    if(!pos)
                        break;
                    if(ps.in_pad && !(flags & 1) && (flags & 2)) {
                        /* Data, variable: one field per element. Constant
                         * padding and arrays (a keyboard's key list) are not
                         * what a pad is read from. The count is the device's
                         * word, up to 2^32: no real pad has more than a few
                         * dozen controls in one item, so look at no more than
                         * that rather than spin on a descriptor that says
                         * four billion. */
                        uint32_t n = ps.g.count > GP_MAX_ELEMENTS ? GP_MAX_ELEMENTS
                                                                  : ps.g.count;
                        for(uint32_t k = 0; k < n && p->nfields < GP_MAX_FIELDS; k++)
                            add_field(p, &ps, usage_for(&ps, k), *pos + k * ps.g.size);
                    }
                    *pos = (uint16_t) (*pos + ps.g.size * ps.g.count);
                    break;
                }
                default:                            /* Output, Feature */
                    break;
            }
            ps.nusage = 0;
            ps.has_range = 0;
            ps.umin = ps.umax = 0;
        }
    }

    if(!ps.found_app || p->nfields == 0)
        return 0;

    int naxes = 0;
    for(int k = 0; k < p->nfields; k++)
        if(p->f[k].kind == FK_AXIS)
            naxes++;
    if(naxes + p->has_hat + p->has_dpad + (p->nbuttons >= 2) < 1)
        return 0;

    assign_roles(p);
    return 1;
}

/* ------------------------------------------------------------------ *
 *  Slots                                                              *
 * ------------------------------------------------------------------ */

/** @brief A free slot, cleared, or NULL. */
static pad_t *alloc_pad(void) {
    for(int i = 0; i < GAMEPAD_MAX; i++)
        if(!pads[i].used) {
            memset(&pads[i], 0, sizeof(pads[i]));
            return &pads[i];
        }
    return 0;
}

int gamepad_attach(uint16_t vendor, uint16_t product, const uint8_t *desc, int len) {
    pad_t *p = alloc_pad();
    if(!p) {
        klogf(LOG_WARNING, "gamepad: no free slot for %04x:%04x\n", vendor, product);
        return -1;
    }
    p->vendor  = vendor;
    p->product = product;

    if(!parse_descriptor(p, desc, len))
        return -1;

    __sync_synchronize();
    p->used = 1;
    int idx = (int) (p - pads);

    int axes = 0;
    for(int i = 0; i < p->nfields; i++)
        if(p->f[i].kind == FK_AXIS && p->f[i].id != GR_NONE)
            axes++;
    klogf(LOG_INFO, "gamepad: %04x:%04x is pad %d: %d axes, %s, %d buttons "
          "(%d descriptor bytes%s)\n", vendor, product, idx, axes,
          p->has_hat ? "hat" : p->has_dpad ? "d-pad" : "no d-pad",
          p->nbuttons, len, p->uses_id ? ", report IDs" : "");
    return idx;
}

int gamepad_attach_xinput(uint16_t vendor, uint16_t product) {
    pad_t *p = alloc_pad();
    if(!p) {
        klogf(LOG_WARNING, "gamepad: no free slot for %04x:%04x\n", vendor, product);
        return -1;
    }
    p->vendor   = vendor;
    p->product  = product;
    p->xinput   = 1;
    p->nbuttons = 13;
    p->has_dpad = 1;
    __sync_synchronize();
    p->used = 1;
    int idx = (int) (p - pads);
    klogf(LOG_INFO, "gamepad: %04x:%04x is pad %d (Xbox 360 controller)\n",
          vendor, product, idx);
    return idx;
}

void gamepad_detach(int pad) {
    if(pad < 0 || pad >= GAMEPAD_MAX || !pads[pad].used)
        return;
    pad_t *p = &pads[pad];
    klogf(LOG_INFO, "gamepad: pad %d (%04x:%04x) gone\n", pad, p->vendor, p->product);
    p->seq++;
    __sync_synchronize();
    p->used = 0;
    memset(&p->st, 0, sizeof(p->st));
    __sync_synchronize();
    p->seq++;
}

/* ------------------------------------------------------------------ *
 *  Reports                                                            *
 * ------------------------------------------------------------------ */

/** @brief @p size bits of report @p r starting at bit @p bit. */
static uint32_t get_bits(const uint8_t *r, int bit, int size) {
    uint32_t v = 0;
    for(int i = 0; i < size; i++) {
        int b = bit + i;
        if(r[b >> 3] & (1u << (b & 7)))
            v |= 1u << i;
    }
    return v;
}

/** @brief Scale @p v from [lmin, lmax] to 0..65535 (never overflows 32 bits). */
static uint32_t scale16(int32_t v, int32_t lmin, int32_t lmax) {
    if(lmax <= lmin)
        return 0;
    if(v < lmin) v = lmin;
    if(v > lmax) v = lmax;
    uint32_t range = (uint32_t) lmax - (uint32_t) lmin;
    uint32_t x     = (uint32_t) v - (uint32_t) lmin;
    while(range > 0xFFFF) {                 /* a 32-bit axis: keep the top bits */
        range >>= 1;
        x >>= 1;
    }
    return x * 65535u / range;
}

static int16_t axis_value(int32_t v, const gp_field_t *f) {
    return (int16_t) ((int32_t) scale16(v, f->lmin, f->lmax) - 32768);
}

static uint8_t trigger_value(int32_t v, const gp_field_t *f) {
    return (uint8_t) (scale16(v, f->lmin, f->lmax) >> 8);
}

/** @brief The dpad bits for a hat switch reading @p v (0 = north, clockwise). */
static uint8_t hat_dirs(int32_t v, const gp_field_t *f) {
    int32_t count = f->lmax - f->lmin + 1;
    int32_t d = v - f->lmin;
    if(d < 0 || d >= count)
        return 0;                           /* the null state: centred */
    if(count == 4)
        d *= 2;                             /* N E S W -> the eight-way numbering */
    else if(d > 7)
        return 0;

    static const uint8_t dirs[8] = {
        GAMEPAD_DPAD_UP,
        GAMEPAD_DPAD_UP | GAMEPAD_DPAD_RIGHT,
        GAMEPAD_DPAD_RIGHT,
        GAMEPAD_DPAD_DOWN | GAMEPAD_DPAD_RIGHT,
        GAMEPAD_DPAD_DOWN,
        GAMEPAD_DPAD_DOWN | GAMEPAD_DPAD_LEFT,
        GAMEPAD_DPAD_LEFT,
        GAMEPAD_DPAD_UP | GAMEPAD_DPAD_LEFT,
    };
    return dirs[d];
}

/** @brief Replace pad @p p's published state, under the seqlock. */
static void publish(pad_t *p, const gamepad_state_t *s) {
    p->seq++;
    __sync_synchronize();
    p->st = *s;
    __sync_synchronize();
    p->seq++;
}

static void decode_hid(pad_t *p, const uint8_t *rpt, int len) {
    int rid = 0;
    if(p->uses_id) {
        rid = rpt[0];
        rpt++;
        len--;
    }

    gamepad_state_t s = p->st;
    uint8_t hat = p->hat_dirs, bdir = p->btn_dirs;

    for(int i = 0; i < p->nfields; i++) {
        const gp_field_t *f = &p->f[i];
        if(f->report_id != rid || f->bit + f->size > len * 8)
            continue;

        uint32_t raw = get_bits(rpt, f->bit, f->size);
        int32_t  v   = (int32_t) raw;
        if(f->is_signed && f->size < 32 && (raw & (1u << (f->size - 1))))
            v = (int32_t) (raw | (~0u << f->size));

        switch(f->kind) {
            case FK_BUTTON:
                if(raw) s.buttons |= 1u << f->id;
                else    s.buttons &= ~(1u << f->id);
                break;
            case FK_DPAD:
                if(raw) bdir |= f->id;
                else    bdir &= (uint8_t) ~f->id;
                break;
            case FK_HAT:
                hat = hat_dirs(v, f);
                break;
            case FK_AXIS:
                switch(f->id) {
                    case GR_LX: s.lx = axis_value(v, f); break;
                    case GR_LY: s.ly = axis_value(v, f); break;
                    case GR_RX: s.rx = axis_value(v, f); break;
                    case GR_RY: s.ry = axis_value(v, f); break;
                    case GR_LT: s.lt = trigger_value(v, f); break;
                    case GR_RT: s.rt = trigger_value(v, f); break;
                    case GR_TRIGGERS: {
                        /* One axis for both: rest in the middle, left trigger
                         * pushes it up, right trigger down. */
                        int32_t a = axis_value(v, f);
                        if(a < -32767)              /* -32768 negates to 32768, which */
                            a = -32767;             /* would shift out to 256 and wrap to 0 */
                        s.lt = a > 0 ? (uint8_t) (a >> 7) : 0;
                        s.rt = a < 0 ? (uint8_t) ((-a) >> 7) : 0;
                        break;
                    }
                    default: break;
                }
                break;
            default:
                break;
        }
    }

    p->hat_dirs = hat;
    p->btn_dirs = bdir;
    s.dpad = hat | bdir;
    publish(p, &s);
}

/** @brief A little-endian int16 at @p b, flipped to the +y-down convention. */
static int16_t xinput_y(const uint8_t *b) {
    int32_t v = (int16_t) (b[0] | (b[1] << 8));
    return (int16_t) (v == -32768 ? 32767 : -v);
}

static int16_t xinput_x(const uint8_t *b) {
    return (int16_t) (b[0] | (b[1] << 8));
}

static void decode_xinput(pad_t *p, const uint8_t *r, int len) {
    /* [0] = 0x00 (input), [1] = 0x14 (length), then the two button bytes, the
     * triggers and four little-endian stick axes. Other message types (0x08 is
     * a wireless receiver announcing a pad) carry no input. */
    if(len < 14 || r[0] != 0x00)
        return;
    uint8_t lo = r[2], hi = r[3];

    gamepad_state_t s;
    memset(&s, 0, sizeof(s));
    uint32_t b = 0;
    if(hi & 0x10) b |= 1u << 0;     /* A */
    if(hi & 0x20) b |= 1u << 1;     /* B */
    if(hi & 0x40) b |= 1u << 2;     /* X */
    if(hi & 0x80) b |= 1u << 3;     /* Y */
    if(hi & 0x01) b |= 1u << 4;     /* LB */
    if(hi & 0x02) b |= 1u << 5;     /* RB */
    if(r[4] >= 64) b |= 1u << 6;    /* left trigger, as a button */
    if(r[5] >= 64) b |= 1u << 7;    /* right trigger, as a button */
    if(lo & 0x20) b |= 1u << 8;     /* Back */
    if(lo & 0x10) b |= 1u << 9;     /* Start */
    if(lo & 0x40) b |= 1u << 10;    /* left stick click */
    if(lo & 0x80) b |= 1u << 11;    /* right stick click */
    if(hi & 0x04) b |= 1u << 12;    /* Guide */
    s.buttons = b;

    if(lo & 0x01) s.dpad |= GAMEPAD_DPAD_UP;
    if(lo & 0x02) s.dpad |= GAMEPAD_DPAD_DOWN;
    if(lo & 0x04) s.dpad |= GAMEPAD_DPAD_LEFT;
    if(lo & 0x08) s.dpad |= GAMEPAD_DPAD_RIGHT;

    s.lt = r[4];
    s.rt = r[5];
    s.lx = xinput_x(r + 6);
    s.ly = xinput_y(r + 8);
    s.rx = xinput_x(r + 10);
    s.ry = xinput_y(r + 12);
    publish(p, &s);
}

void gamepad_report(int pad, const uint8_t *rpt, int len) {
    if(pad < 0 || pad >= GAMEPAD_MAX || !pads[pad].used || len < 1)
        return;
    pad_t *p = &pads[pad];

    if(watch) {
        char hex[3 * 24 + 1];
        int o = 0;
        for(int i = 0; i < len && i < 24 && o < (int) sizeof(hex) - 3; i++)
            o += snprintf(hex + o, sizeof(hex) - o, "%02x ", rpt[i]);
        klogf(LOG_INFO, "pad %d: len %d: %s%s\n", pad, len, hex, len > 24 ? "..." : "");
    }

    if(p->xinput)
        decode_xinput(p, rpt, len);
    else
        decode_hid(p, rpt, len);
}

/* ------------------------------------------------------------------ *
 *  Queries                                                            *
 * ------------------------------------------------------------------ */

int gamepad_get(int index, gamepad_state_t *out) {
    if(index < 0 || index >= GAMEPAD_MAX)
        return 0;
    pad_t *p = &pads[index];
    for(int tries = 0; tries < 100; tries++) {
        uint32_t s1 = p->seq;
        if(s1 & 1)
            continue;                       /* a report is being applied */
        __sync_synchronize();
        int used = p->used;
        gamepad_state_t copy = p->st;
        __sync_synchronize();
        if(p->seq != s1)
            continue;
        if(!used)
            return 0;
        *out = copy;
        return 1;
    }
    return 0;
}

int gamepad_count(void) {
    int n = 0;
    for(int i = 0; i < GAMEPAD_MAX; i++)
        n += pads[i].used;
    return n;
}

int gamepad_describe(int index, char *buf, int size) {
    if(index < 0 || index >= GAMEPAD_MAX || !pads[index].used)
        return 0;
    const pad_t *p = &pads[index];

    if(p->xinput) {
        snprintf(buf, size, "pad %d: %04x:%04x Xbox 360 controller, 13 buttons",
                 index, p->vendor, p->product);
        return 1;
    }

    static const char *const role_name[] = {
        "-", "lx", "ly", "rx", "ry", "lt", "rt", "lt/rt"
    };
    char axes[48];
    int o = 0;
    axes[0] = 0;
    for(int r = GR_LX; r <= GR_TRIGGERS; r++) {
        for(int i = 0; i < p->nfields; i++)
            if(p->f[i].kind == FK_AXIS && p->f[i].id == r) {
                o += snprintf(axes + o, sizeof(axes) - o, "%s%s", o ? " " : "",
                              role_name[r]);
                break;
            }
    }
    snprintf(buf, size, "pad %d: %04x:%04x %d buttons, axes [%s], %s%s",
             index, p->vendor, p->product, p->nbuttons, axes,
             p->has_hat ? "hat" : p->has_dpad ? "d-pad buttons" : "no d-pad",
             p->uses_id ? ", report IDs" : "");
    return 1;
}

void gamepad_set_watch(int on) {
    watch = on;
}
