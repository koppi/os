#include <sound.h>
#include <io.h>
#include <lib/string.h>
#include <idt.h>
#include <log.h>

#include <hxcmod.h>
#include <modfile.h>

#define MIXER_IRQ       0x5
#define MIXER_IRQ_DATA  0x2

// SB16 ports
#define DSP_MIXER       0x224
#define DSP_MIXER_DATA  0x225
#define DSP_RESET       0x226
#define DSP_READ        0x22A
#define DSP_WRITE       0x22C
#define DSP_READ_STATUS 0x22E
#define DSP_ACK_8       DSP_READ_STATUS
#define DSP_ACK_16      0x22F

// TODO: ???
#define DSP_PROG_16     0xB0
#define DSP_PROG_8      0xC0
#define DSP_AUTO_INIT   0x06
#define DSP_PLAY        0x00
#define DSP_RECORD      0x08
#define DSP_MONO        0x00
#define DSP_STEREO      0x20
#define DSP_UNSIGNED    0x00
#define DSP_SIGNED      0x10

#define DMA_CHANNEL_16  5
#define DMA_FLIP_FLOP   0xD8
#define DMA_BASE_ADDR   0xC4
#define DMA_COUNT       0xC6

// commands for DSP_WRITE
#define DSP_SET_TIME    0x40
#define DSP_SET_RATE    0x41
#define DSP_ON          0xD1
#define DSP_OFF         0xD3
#define DSP_OFF_8       0xD0
#define DSP_ON_8        0xD4
#define DSP_OFF_16      0xD5
#define DSP_ON_16       0xD6
#define DSP_VERSION     0xE1

// commands for DSP_MIXER
#define DSP_VOLUME  0x22
#define DSP_IRQ     0x80

#define SAMPLE_RATE     44100
#define BUFFER_MS       50

#define BUFFER_SIZE 2048*20 //((size_t) (SAMPLE_RATE * (BUFFER_MS / 1000.0)))
static __attribute__ ((aligned (4096))) short int buffer[BUFFER_SIZE];
static uint8_t buffer_flip = 0;

static modcontext modctx;

static uint8_t sound_muted = 1;

static void fill(short int *buf, size_t len) {
    //printf("sound: fill len: %d\n", len);
    hxcmod_fillbuffer(&modctx, buf, len, NULL);
}

void sound_toggle() {
    if (sound_muted) {
        sound_muted = 0;
    } else {
        sound_muted = 1;
    }
}

static void dsp_write(uint8_t b) {
    while (inportb(DSP_WRITE) & 0x80);
    outportb(DSP_WRITE, b);
}

/*
static void dsp_read(uint8_t b) {
    while (inportb(DSP_READ_STATUS) & 0x80);
    outportb(DSP_READ, b);
}*/

static uint8_t dsp_detect_timeout(uint8_t* b) {
    for (size_t i = 0; i < 1000000; i++) {
        if (inportb(DSP_READ_STATUS) & 0x80) {
            *b = inportb(DSP_READ);
            return 1;
        }
    }
    
    return 0;
}

static uint8_t reset() {
    uint8_t status = 0;
    
    outportb(DSP_RESET, 1);

    // TODO: maybe not necessary
    // ~3 microseconds?
    for (size_t i = 0; i < 1000000; i++);

    outportb(DSP_RESET, 0);

    if (!dsp_detect_timeout(&status) || status != 0xAA) {
        goto fail;
    }

    outportb(DSP_WRITE, DSP_VERSION);
    uint8_t major = inportb(DSP_READ),
       minor = inportb(DSP_READ);

    if (major < 4) {
        status = (major << 4) | minor;
        goto fail;
    }

    return 0;
fail:
    klogf(LOG_ERR, "Failed to reset SB16: %d\n", 128);
    return 1;
}

static void set_sample_rate(uint16_t hz) {
    dsp_write(DSP_SET_RATE);
    dsp_write((uint8_t) ((hz >> 8) & 0xFF));
    dsp_write((uint8_t) (hz & 0xFF));
}

static void transfer(void *buf, uint32_t len) {
    uint8_t mode = 0x48;

    // disable DMA channel
    outportb(DSP_ON_8, 4 + (DMA_CHANNEL_16 % 4));

    // clear byte-poiner flip-flop
    outportb(DMA_FLIP_FLOP, 0);

    // write DMA mode for transfer
    outportb(DSP_ON_16, (DMA_CHANNEL_16 % 4) | mode | (1 << 4));

    // write buffer offset (div 2 for 16-bit)
    unsigned short offset = (((uintptr_t) buf) / 2) & 0xFFFF;
    outportb(DMA_BASE_ADDR, (uint8_t) ((offset >> 0) & 0xFF));
    outportb(DMA_BASE_ADDR, (uint8_t) ((offset >> 8) & 0xFF));

    // write transfer length
    outportb(DMA_COUNT, (uint8_t) (((len - 1) >> 0) & 0xFF));
    outportb(DMA_COUNT, (uint8_t) (((len - 1) >> 8) & 0xFF));

    // write buffer page
    outportb(0x8B, ((uintptr_t) buf) >> 16);

    // enable DMA channel
    outportb(DSP_ON_8, DMA_CHANNEL_16 % 4);
}

void sb16_irq_handler() {
    buffer_flip = !buffer_flip;

    if (!sound_muted) {
        fill(
            &buffer[buffer_flip ? 0 : (BUFFER_SIZE / 2)],
            (BUFFER_SIZE / 2));
    } else {
        memset(buffer, 0, BUFFER_SIZE*sizeof(short int));
    }

    inportb(DSP_READ_STATUS);
    inportb(DSP_ACK_16);
}

extern void sound_int();

static void configure() {
    install_ir(32 + MIXER_IRQ, 0x80 | 0x0E, 0x8, &sound_int);

    outportb(DSP_MIXER, DSP_IRQ);
    outportb(DSP_MIXER_DATA, MIXER_IRQ_DATA);
}

void sound_init() {
    //klogf(LOG_DEBUG, "sound_init()\n");
    
    hxcmod_init(&modctx);
    hxcmod_load(&modctx, modfile_0, modfile_0_size);

    install_ir(32 + MIXER_IRQ, 0x80 | 0x0E, 0x8, &sound_int);
    if (reset() == 0)
    {
        configure();

        transfer(buffer, BUFFER_SIZE);
        set_sample_rate(SAMPLE_RATE);

        uint16_t sample_count = (BUFFER_SIZE / 2) - 1;
        dsp_write(DSP_PLAY | DSP_PROG_16 | DSP_AUTO_INIT);
        dsp_write(DSP_SIGNED | DSP_MONO);
        dsp_write((uint8_t) ((sample_count >> 0) & 0xFF));
        dsp_write((uint8_t) ((sample_count >> 8) & 0xFF));

        dsp_write(DSP_ON);
        dsp_write(DSP_ON_16);

        printf("playing module '%s'.\n", modctx.song.title);
        klogf(LOG_DEBUG, "sound_init() ok.\n");
    }
}
