#ifndef APU_H
#define APU_H

#include <SDL2/SDL.h>

#include "types.h"

#define APU_DIV_RATE 8192

#define SAMPLE_FREQ 44100
#define SAMPLE_RATE (CPU_CLOCK_SPEED / SAMPLE_FREQ)
#define SAMPLE_BUF_LEN 1024

enum {
    NRX4_WVLEN_HI = 0b00000111,
    NRX4_LEN_ENABLE = 1 << 6,
    NRX4_TRIGGER = 1 << 7
};

struct gb;

struct gb_apu {
    struct gb* master;

    u16 apu_div;

    u8* sample_buf;
    int sample_ind;
    SDL_AudioDeviceID audio_id;
    bool samples_full;

    u16 ch1_counter;
    u16 ch1_wavelen;
    u8 ch1_duty_index;

    u16 ch2_counter;
    u16 ch2_wavelen;
    u8 ch2_duty_index;
};

void init_apu(struct gb* master, struct gb_apu* apu);
void apu_clock(struct gb_apu* apu);

#endif