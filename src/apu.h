#ifndef APU_H
#define APU_H

#include "types.h"

#define APU_DIV_RATE 8192

#define SAMPLE_FREQ 44100
#define SAMPLE_RATE ((1 << 22) / SAMPLE_FREQ)
#define SAMPLE_BUF_LEN 1024

enum { NRX1_LEN = 0b00111111, NRX1_DUTY = 0b11000000 };

enum { NRX2_PACE = 0b00000111, NRX2_DIR = 1 << 3, NRX2_VOL = 0b11110000 };

enum {
    NRX4_WVLEN_HI = 0b00000111,
    NRX4_LEN_ENABLE = 1 << 6,
    NRX4_TRIGGER = 1 << 7
};

enum { NR10_SLOP = 0b00000111, NR10_DIR = 1 << 3, NR10_PACE = 0b01110000 };

enum { NR43_DIV = 0b00000111, NR43_WIDTH = 1 << 3, NR43_SHIFT = 0b11110000 };

struct gb;

struct gb_apu {
    struct gb* master;

    u16 apu_div;

    float sample_buf[SAMPLE_BUF_LEN];
    int sample_ind;
    bool samples_full;

    long global_counter;

    bool ch1_enable;
    u16 ch1_counter;
    u16 ch1_wavelen;
    u8 ch1_duty_index;
    u8 ch1_env_counter;
    u8 ch1_env_pace;
    bool ch1_env_dir;
    u8 ch1_volume;
    u8 ch1_len_counter;
    u8 ch1_sweep_pace;
    u8 ch1_sweep_counter;

    bool ch2_enable;
    u16 ch2_counter;
    u16 ch2_wavelen;
    u8 ch2_duty_index;
    u8 ch2_env_counter;
    u8 ch2_env_pace;
    bool ch2_env_dir;
    u8 ch2_volume;
    u8 ch2_len_counter;

    bool ch3_enable;
    u16 ch3_counter;
    u16 ch3_wavelen;
    u8 ch3_sample_index;
    u8 ch3_len_counter;

    bool ch4_enable;
    int ch4_counter;
    u16 ch4_lfsr;
    u8 ch4_env_counter;
    u8 ch4_env_pace;
    bool ch4_env_dir;
    u8 ch4_volume;
    u8 ch4_len_counter;
};

void apu_clock(struct gb_apu* apu);

#endif