#include "apu.h"

#include "emulator.h"
#include "gb.h"

u8 duty_cycles[] = {0b11111110, 0b01111110, 0b01111000, 0b10000001};

u8 get_sample_ch1(struct gb_apu* apu) {
    return (duty_cycles[(apu->master->io[NR11] & NRX1_DUTY) >> 6] &
            (1 << (apu->ch1_duty_index & 7)))
               ? apu->ch1_volume
               : 0;
}

u8 get_sample_ch2(struct gb_apu* apu) {
    return (duty_cycles[(apu->master->io[NR21] & NRX1_DUTY) >> 6] &
            (1 << (apu->ch2_duty_index & 7)))
               ? apu->ch2_volume
               : 0;
}

u8 get_sample_ch3(struct gb_apu* apu) {
    u8 wvram_index = (apu->ch3_sample_index & 0b00011110) >> 1;
    u8 sample = (apu->master->io + WAVERAM)[wvram_index];
    if (apu->ch3_sample_index & 1) {
        sample &= 0x0f;
    } else {
        sample &= 0xf0;
        sample >>= 4;
    }
    u8 out_level = (apu->master->io[NR32] & 0b01100000) >> 5;
    if (out_level) {
        return sample >> (out_level - 1);
    } else {
        return 0;
    }
}

u8 get_sample_ch4(struct gb_apu* apu) {
    return (apu->ch4_lfsr & 1) ? apu->ch4_volume : 0;
}

void apu_clock(struct gb_apu* apu) {
    if (!(apu->master->io[NR52] & 0b10000000)) {
        apu->master->io[NR52] = 0;
        apu->apu_div = 0;
        apu->sample_ind = 0;
        apu->global_counter = 0;
        return;
    }

    apu->master->io[NR52] = 0b10000000 | (apu->ch1_enable ? 0b0001 : 0) |
                            (apu->ch2_enable ? 0b0010 : 0) |
                            (apu->ch3_enable ? 0b0100 : 0) |
                            (apu->ch4_enable ? 0b1000 : 0);

    if (gbemu.cycle % gbemu.speed == 0) {
        apu->global_counter++;

        if (apu->global_counter % 2 == 0) {
            apu->ch3_counter++;
            if (apu->ch3_counter == 2048) {
                apu->ch3_counter = apu->ch3_wavelen;
                apu->ch3_sample_index++;
            }
        }

        if (apu->global_counter % 4 == 0) {
            apu->ch1_counter++;
            if (apu->ch1_counter == 2048) {
                apu->ch1_counter = apu->ch1_wavelen;
                apu->ch1_duty_index++;
            }

            apu->ch2_counter++;
            if (apu->ch2_counter == 2048) {
                apu->ch2_counter = apu->ch2_wavelen;
                apu->ch2_duty_index++;
            }
        }

        if (apu->global_counter % 8 == 0) {
            apu->ch4_counter++;
            int rate = 2 << ((apu->master->io[NR43] & NR43_SHIFT) >> 4);
            if (apu->master->io[NR43] & NR43_DIV) {
                rate *= apu->master->io[NR43] & NR43_DIV;
            }
            if (apu->ch4_counter >= rate) {
                apu->ch4_counter = 0;
                u16 bit = (~(apu->ch4_lfsr ^ (apu->ch4_lfsr >> 1))) & 1;
                apu->ch4_lfsr = (apu->ch4_lfsr & ~(1 << 15)) | (bit << 15);
                if (apu->master->io[NR43] & NR43_WIDTH) {
                    apu->ch4_lfsr = (apu->ch4_lfsr & ~(1 << 7)) | (bit << 7);
                }
                apu->ch4_lfsr >>= 1;
            }
        }

        if (apu->global_counter % SAMPLE_RATE == 0) {
            u8 ch1_sample = apu->ch1_enable ? get_sample_ch1(apu) : 0;
            u8 ch2_sample = apu->ch2_enable ? get_sample_ch2(apu) : 0;
            u8 ch3_sample = apu->ch3_enable ? get_sample_ch3(apu) : 0;
            u8 ch4_sample = apu->ch4_enable ? get_sample_ch4(apu) : 0;
            apu->master->io[PCM12] = ch1_sample | (ch2_sample << 4);
            apu->master->io[PCM34] = ch3_sample | (ch4_sample << 4);

            u8 l_sample = 0, r_sample = 0;
            if (apu->master->io[NR51] & (1 << 0)) r_sample += ch1_sample;
            if (apu->master->io[NR51] & (1 << 1)) r_sample += ch2_sample;
            if (apu->master->io[NR51] & (1 << 2)) r_sample += ch3_sample;
            if (apu->master->io[NR51] & (1 << 3)) r_sample += ch4_sample;
            if (apu->master->io[NR51] & (1 << 4)) l_sample += ch1_sample;
            if (apu->master->io[NR51] & (1 << 5)) l_sample += ch2_sample;
            if (apu->master->io[NR51] & (1 << 6)) l_sample += ch3_sample;
            if (apu->master->io[NR51] & (1 << 7)) l_sample += ch4_sample;

            apu->sample_buf[apu->sample_ind++] =
                (float) l_sample / 500 *
                (((apu->master->io[NR50] & 0b01110000) >> 4) + 1);
            apu->sample_buf[apu->sample_ind++] =
                (float) r_sample / 500 *
                ((apu->master->io[NR50] & 0b00000111) + 1);
            if (apu->sample_ind == SAMPLE_BUF_LEN) {
                apu->samples_full = true;
                apu->sample_ind = 0;
            }
        }
    }

    u16 effective_apu_div_rate = APU_DIV_RATE;
    if (apu->master->io[KEY1] & (1 << 7)) effective_apu_div_rate <<= 1;
    if (apu->master->div % effective_apu_div_rate == 0) {
        apu->apu_div++;

        if (apu->apu_div % 2 == 0) {
            if (apu->master->io[NR14] & NRX4_LEN_ENABLE) {
                apu->ch1_len_counter++;
                if (apu->ch1_len_counter == 64) {
                    apu->ch1_len_counter = 0;
                    apu->ch1_enable = false;
                }
            }

            if (apu->master->io[NR24] & NRX4_LEN_ENABLE) {
                apu->ch2_len_counter++;
                if (apu->ch2_len_counter == 64) {
                    apu->ch2_len_counter = 0;
                    apu->ch2_enable = false;
                }
            }

            if (apu->master->io[NR34] & NRX4_LEN_ENABLE) {
                apu->ch3_len_counter++;
                if (apu->ch3_len_counter == 0) {
                    apu->ch3_enable = false;
                }
            }

            if (apu->master->io[NR44] & NRX4_LEN_ENABLE) {
                apu->ch4_len_counter++;
                if (apu->ch4_len_counter == 64) {
                    apu->ch4_len_counter = 0;
                    apu->ch4_enable = false;
                }
            }
        }
        if (apu->apu_div % 4 == 0) {
            apu->ch1_sweep_counter++;
            if (apu->ch1_sweep_pace &&
                apu->ch1_sweep_counter == apu->ch1_sweep_pace) {
                apu->ch1_sweep_counter = 0;
                apu->ch1_sweep_pace = (apu->master->io[NR10] & NR10_PACE) >> 4;
                u16 del_wvlen =
                    apu->ch1_wavelen >> (apu->master->io[NR10] & NR10_SLOP);
                u16 new_wvlen = apu->ch1_wavelen;
                if (apu->master->io[NR10] & NR10_DIR) {
                    new_wvlen -= del_wvlen;
                } else {
                    new_wvlen += del_wvlen;
                    if (new_wvlen > 2047) apu->ch1_enable = false;
                }
                if (apu->master->io[NR10] & NR10_SLOP)
                    apu->ch1_wavelen = new_wvlen;
            }
        }
        if (apu->apu_div % 8 == 0) {
            apu->ch1_env_counter++;
            if (apu->ch1_env_pace &&
                apu->ch1_env_counter == apu->ch1_env_pace) {
                apu->ch1_env_counter = 0;
                if (apu->ch1_env_dir) {
                    apu->ch1_volume++;
                    if (apu->ch1_volume == 0x10) apu->ch1_volume = 0xf;
                } else {
                    apu->ch1_volume--;
                    if (apu->ch1_volume == 0xff) apu->ch1_volume = 0x0;
                }
            }

            apu->ch2_env_counter++;
            if (apu->ch2_env_pace &&
                apu->ch2_env_counter == apu->ch2_env_pace) {
                apu->ch2_env_counter = 0;
                if (apu->ch2_env_dir) {
                    apu->ch2_volume++;
                    if (apu->ch2_volume == 0x10) apu->ch2_volume = 0xf;
                } else {
                    apu->ch2_volume--;
                    if (apu->ch2_volume == 0xff) apu->ch2_volume = 0x0;
                }
            }

            apu->ch4_env_counter++;
            if (apu->ch4_env_pace &&
                apu->ch4_env_counter == apu->ch4_env_pace) {
                apu->ch4_env_counter = 0;
                if (apu->ch4_env_dir) {
                    apu->ch4_volume++;
                    if (apu->ch4_volume == 0x10) apu->ch4_volume = 0xf;
                } else {
                    apu->ch4_volume--;
                    if (apu->ch4_volume == 0xff) apu->ch4_volume = 0x0;
                }
            }
        }
    }
}
