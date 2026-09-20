/*
 * OPL backend for koppi-os.
 *
 * Stands in for chocolate-doom's opl.c (the driver-selection layer) and
 * opl_sdl.c (its software backend) together, because neither has anything to
 * choose between here: there is no hardware OPL to talk to on any machine
 * this runs on, so emulation is the only backend, and with one backend the
 * driver table is an indirection to nowhere. What is left is the part that
 * matters -- Nuked OPL3 plus the sequencer's callback queue -- wired to this
 * system's clock.
 *
 * That clock is the difference from upstream. opl_sdl.c is a post-mix hook:
 * SDL_mixer calls it, and it renders and advances time in response. Here the
 * game's own mixer is in charge, so OPL_Koppi_Render is a pull, and musical
 * time advances only as samples are consumed. Nothing is scheduled off a
 * wall clock, so music and sound effects cannot drift apart however the
 * frame rate wanders.
 *
 * The other simplification is locking: upstream needs two mutexes because
 * SDL runs the audio callback on its own thread. Everything here -- the
 * mixer, the callbacks it runs, and every I_*Song call from the game loop --
 * happens on the one thread that is the Doom process, so OPL_Lock and
 * OPL_Unlock have nothing to do.
 *
 * OPL_InitRegisters is chocolate-doom's, verbatim, including its writes to
 * registers that do not exist: that is what Doom's own OPL code did, and the
 * emulator is here to be bug-compatible with it.
 */
#include <stdio.h>
#include <string.h>

#include "opl.h"
#include "opl3.h"
#include "opl_queue.h"
#include "opl_koppi.h"

/* Set by i_oplmusic.c through OPL_SetSampleRate before OPL_Init. */
unsigned int opl_sample_rate = 22050;

/* DBOPL does not keep the chip's two timers, so the backend does. Doom only
 * reads them during its AdLib detection, which does not run here, but
 * OPL_InitRegisters writes them and the registers must not reach the
 * synthesiser as note data. */
typedef struct {
    unsigned int rate;        /* timer ticks per second */
    unsigned int enabled;
    unsigned int value;       /* last value written */
    uint64_t     expire_time;
} opl_timer_t;

static opl3_chip  chip;
static opl_callback_queue_t *queue;
static uint64_t   current_time;      /* microseconds of rendered audio */
static uint64_t   pause_offset;      /* microseconds spent paused */
static int        opl_paused;
static int        register_num;
static int        opl3mode;
static int        initialised;

static opl_timer_t timer1 = { 12500, 0, 0, 0 };
static opl_timer_t timer2 = {  3125, 0, 0, 0 };

/* ------------------------------------------------------------------ *
 *  Time                                                               *
 * ------------------------------------------------------------------ */

/** Advance by @p nsamples and run every callback that has come due. */
static void advance_time(unsigned int nsamples) {
    uint64_t us = ((uint64_t) nsamples * OPL_SECOND) / opl_sample_rate;

    current_time += us;
    if (opl_paused)
        pause_offset += us;

    while (!OPL_Queue_IsEmpty(queue)
        && current_time >= OPL_Queue_Peek(queue) + pause_offset) {
        opl_callback_t callback;
        void *data;
        if (!OPL_Queue_Pop(queue, &callback, &data))
            break;
        callback(data);
    }
}

void OPL_Koppi_Render(int16_t *dst, unsigned int nsamples) {
    if (!initialised) {
        memset(dst, 0, (size_t) nsamples * 2 * sizeof(int16_t));
        return;
    }

    unsigned int filled = 0;
    unsigned int idle = 0;

    while (filled < nsamples) {
        uint64_t n;

        /* Render up to the next callback and no further, so an event lands
         * on the sample it was scheduled for rather than at the start of
         * whatever block happens to contain it. */
        if (opl_paused || OPL_Queue_IsEmpty(queue)) {
            n = nsamples - filled;
        } else {
            uint64_t next = OPL_Queue_Peek(queue) + pause_offset;
            uint64_t ahead = next > current_time ? next - current_time : 0;
            n = (ahead * opl_sample_rate + OPL_SECOND - 1) / OPL_SECOND;
            if (n > nsamples - filled)
                n = nsamples - filled;
        }

        if (n > 0) {
            OPL3_GenerateStream(&chip, dst + filled * 2, (uint32_t) n);
            filled += (unsigned int) n;
            idle = 0;
        } else if (++idle > 4096) {
            /* A zero-length step is normal -- simultaneous MIDI events are
             * scheduled at the same instant and advance_time runs them all.
             * A song that somehow never got past one would hang the game
             * with the screen grabbed and no way to interrupt it, so give
             * up on exact placement and force the clock forward. */
            OPL3_GenerateStream(&chip, dst + filled * 2, 1);
            filled++;
            idle = 0;
        }

        advance_time((unsigned int) n);
    }
}

/* ------------------------------------------------------------------ *
 *  Registers                                                          *
 * ------------------------------------------------------------------ */

static void timer_calculate_end(opl_timer_t *timer) {
    if (timer->enabled) {
        int tics = 0x100 - (int) timer->value;
        timer->expire_time = current_time
                           + ((uint64_t) tics * OPL_SECOND) / timer->rate;
    }
}

static void write_register(unsigned int reg_num, unsigned int value) {
    switch (reg_num) {
    case OPL_REG_TIMER1:
        timer1.value = value;
        timer_calculate_end(&timer1);
        break;

    case OPL_REG_TIMER2:
        timer2.value = value;
        timer_calculate_end(&timer2);
        break;

    case OPL_REG_TIMER_CTRL:
        if (value & 0x80) {
            timer1.enabled = 0;
            timer2.enabled = 0;
        } else {
            if ((value & 0x40) == 0) {
                timer1.enabled = (value & 0x01) != 0;
                timer_calculate_end(&timer1);
            }
            if ((value & 0x20) == 0) {
                timer2.enabled = (value & 0x02) != 0;
                timer_calculate_end(&timer2);
            }
        }
        break;

    case OPL_REG_NEW:
        opl3mode = value & 0x01;
        __attribute__((fallthrough));   /* the synthesiser needs it too */

    default:
        OPL3_WriteRegBuffered(&chip, (uint16_t) reg_num, (uint8_t) value);
        break;
    }
}

void OPL_WritePort(opl_port_t port, unsigned int value) {
    if (port == OPL_REGISTER_PORT)
        register_num = (int) value;
    else if (port == OPL_REGISTER_PORT_OPL3)
        register_num = (int) value | 0x100;
    else if (port == OPL_DATA_PORT)
        write_register((unsigned int) register_num, value);
}

unsigned int OPL_ReadPort(opl_port_t port) {
    unsigned int result = 0;

    if (port == OPL_REGISTER_PORT_OPL3)
        return 0xff;

    if (timer1.enabled && current_time > timer1.expire_time)
        result |= 0x80 | 0x40;
    if (timer2.enabled && current_time > timer2.expire_time)
        result |= 0x80 | 0x20;

    return result;
}

unsigned int OPL_ReadStatus(void) {
    return OPL_ReadPort(OPL_REGISTER_PORT);
}

/*
 * Upstream routes this through the ports and pads it with the register
 * reads a real chip needs between writes. There is no chip to wait for, so
 * the value goes straight in.
 */
void OPL_WriteRegister(int reg, int value) {
    write_register((unsigned int) reg, (unsigned int) value);
}

/*
 * chocolate-doom's opl.c, unchanged. The two loops that run past the end of
 * the operator range, and the <= that takes them one register too far, are
 * what Doom's own OPL code did; the comment there is "this is what Doom
 * does ...".
 */
void OPL_InitRegisters(int opl3) {
    int r;

    for (r = OPL_REGS_LEVEL; r <= OPL_REGS_LEVEL + OPL_NUM_OPERATORS; ++r)
        OPL_WriteRegister(r, 0x3f);

    for (r = OPL_REGS_ATTACK; r <= OPL_REGS_WAVEFORM + OPL_NUM_OPERATORS; ++r)
        OPL_WriteRegister(r, 0x00);

    for (r = 1; r < OPL_REGS_LEVEL; ++r)
        OPL_WriteRegister(r, 0x00);

    /* Reset both timers and enable interrupts. */
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x60);
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x80);

    /* "Allow FM chips to control the waveform of each operator". */
    OPL_WriteRegister(OPL_REG_WAVEFORM_ENABLE, 0x20);

    if (opl3) {
        OPL_WriteRegister(OPL_REG_NEW, 0x01);

        for (r = OPL_REGS_LEVEL; r <= OPL_REGS_LEVEL + OPL_NUM_OPERATORS; ++r)
            OPL_WriteRegister(r | 0x100, 0x3f);

        for (r = OPL_REGS_ATTACK; r <= OPL_REGS_WAVEFORM + OPL_NUM_OPERATORS; ++r)
            OPL_WriteRegister(r | 0x100, 0x00);

        for (r = 1; r < OPL_REGS_LEVEL; ++r)
            OPL_WriteRegister(r | 0x100, 0x00);
    }

    /* Keyboard split point on (?) */
    OPL_WriteRegister(OPL_REG_FM_MODE, 0x40);

    if (opl3)
        OPL_WriteRegister(OPL_REG_NEW, 0x01);
}

/* ------------------------------------------------------------------ *
 *  Lifecycle                                                          *
 * ------------------------------------------------------------------ */

void OPL_SetSampleRate(unsigned int rate) {
    if (rate)
        opl_sample_rate = rate;
}

opl_init_result_t OPL_Init(unsigned int port_base) {
    (void) port_base;            /* no I/O ports: this is emulation only */

    if (initialised)
        return OPL_INIT_OPL3;

    queue = OPL_Queue_Create();
    if (queue == NULL)
        return OPL_INIT_NONE;

    OPL3_Reset(&chip, opl_sample_rate);
    current_time = 0;
    pause_offset = 0;
    opl_paused   = 0;
    opl3mode     = 0;
    register_num = 0;
    initialised  = 1;

    /* Nuked emulates an OPL3, and saying so is what lets a player ask for
     * OPL3 mode with DMXOPTION. Doom's own music was written for an OPL2
     * and that stays the default. */
    return OPL_INIT_OPL3;
}

opl_init_result_t OPL_Detect(void) {
    return initialised ? OPL_INIT_OPL3 : OPL_INIT_NONE;
}

void OPL_Shutdown(void) {
    if (!initialised)
        return;
    initialised = 0;
    OPL_Queue_Destroy(queue);
    queue = NULL;
}

/* ------------------------------------------------------------------ *
 *  Callbacks                                                          *
 * ------------------------------------------------------------------ */

void OPL_SetCallback(uint64_t us, opl_callback_t callback, void *data) {
    if (initialised)
        OPL_Queue_Push(queue, callback, data, current_time - pause_offset + us);
}

void OPL_ClearCallbacks(void) {
    if (initialised)
        OPL_Queue_Clear(queue);
}

void OPL_AdjustCallbacks(float factor) {
    if (initialised)
        OPL_Queue_AdjustCallbacks(queue, current_time, factor);
}

/* One thread does all of this; see the note at the top of the file. */
void OPL_Lock(void)   { }
void OPL_Unlock(void) { }

void OPL_SetPaused(int paused) {
    opl_paused = paused;
}

/*
 * Upstream blocks until the audio thread has rendered this far. There is no
 * audio thread: rendering happens on this one, so waiting here would wait
 * for something only this thread can do. Nothing in the music module calls
 * it.
 */
void OPL_Delay(uint64_t us) {
    (void) us;
}
