#include "apu.h"

#include "gb.h"

u8 duty_cycles[] = {0b11111110, 0b01111110, 0b01111000, 0b10000001};

void init_apu(struct gb* master, struct gb_apu* apu) {
    apu->master = master;
    apu->apu_div = 0;
}

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

void apu_clock(struct gb_apu* apu) {
    if (apu->master->div % 4 == 0) {
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

        apu->ch3_counter++;
        if (apu->ch3_counter == 2048) {
            apu->ch3_counter = apu->ch3_wavelen;
            apu->ch3_sample_index++;
        }
    }
    // same for other channels

    if (apu->master->div % SAMPLE_RATE == 0) {
        u8 ch1_sample = apu->ch1_enable ? get_sample_ch1(apu) : 0;
        u8 ch2_sample = apu->ch2_enable ? get_sample_ch2(apu) : 0;
        u8 ch3_sample = apu->ch3_enable ? get_sample_ch3(apu) : 0;

        u8 l_sample = 0, r_sample = 0;
        l_sample += ch1_sample;
        l_sample += ch2_sample;
        l_sample += ch3_sample;
        r_sample += ch1_sample;
        r_sample += ch2_sample;
        r_sample += ch3_sample;

        apu->sample_buf[apu->sample_ind++] = l_sample * 2;
        apu->sample_buf[apu->sample_ind++] = r_sample * 2;
        if (apu->sample_ind == SAMPLE_BUF_LEN) {
            apu->samples_full = true;
            apu->sample_ind = 0;
        }
    }

    if (apu->master->div % APU_DIV_RATE == 0) {
        apu->apu_div++;

        if (apu->apu_div % 2 == 0) {
            apu->ch1_len_counter++;
            if (apu->ch1_len_counter == 64) {
                apu->ch1_len_counter--;
                if (apu->master->io[NR14] & NRX4_LEN_ENABLE)
                    apu->ch1_enable = false;
            }

            apu->ch2_len_counter++;
            if (apu->ch2_len_counter == 64) {
                apu->ch2_len_counter--;
                if (apu->master->io[NR24] & NRX4_LEN_ENABLE)
                    apu->ch2_enable = false;
            }

            apu->ch3_len_counter++;
            if (apu->ch3_len_counter == 0) {
                apu->ch3_len_counter--;
                if (apu->master->io[NR34] & NRX4_LEN_ENABLE)
                    apu->ch3_enable = false;
            }
            // tick length timer
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
            // tick envelopes
        }
    }
}
