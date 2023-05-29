#include "apu.h"

#include "gb.h"

u8 duty_cycles[] = {0b11111110, 0b01111110, 0b01111000, 0b10000001};

void init_apu(struct gb* master, struct gb_apu* apu) {
    apu->master = master;
    apu->apu_div = 0;
}

u8 get_sample_ch1(struct gb_apu* apu) {
    u8 sample = (duty_cycles[(apu->master->io[NR11] & NRX1_DUTY) >> 6] &
                 (1 << (apu->ch1_duty_index & 7)))
                    ? 1 //apu->ch1_volume
                    : 0;
    return sample;
}

u8 get_sample_ch2(struct gb_apu* apu) {
    u8 sample = (duty_cycles[(apu->master->io[NR21] & NRX1_DUTY) >> 6] &
                 (1 << (apu->ch2_duty_index & 7)))
                    ? 1 // apu->ch2_volume
                    : 0;
    return sample;
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
    }
    // same for other channels

    if (apu->master->div % SAMPLE_RATE == 0) {
        u8 ch1_sample = apu->ch1_enable ? get_sample_ch1(apu) : 0;
        u8 ch2_sample = apu->ch2_enable ? get_sample_ch2(apu) : 0;

        u8 l_sample = 0, r_sample = 0;
        l_sample += ch1_sample;
        l_sample += ch2_sample;
        r_sample += ch1_sample;
        r_sample += ch2_sample;

        apu->sample_buf[apu->sample_ind++] = l_sample * 5;
        apu->sample_buf[apu->sample_ind++] = r_sample * 5;
        if (apu->sample_ind == SAMPLE_BUF_LEN) {
            apu->samples_full = true;
            apu->sample_ind = 0;
        }
    }
    if (apu->master->div % APU_DIV_RATE == 0) apu->apu_div++;

    if (apu->apu_div % 2 == 0) {
        apu->ch1_len++;
        if (apu->ch1_len == 64) {
            apu->ch1_len--;
            if (apu->master->io[NR14] & NRX4_LEN_ENABLE)
                apu->ch1_enable = false;
        }

        apu->ch2_len++;
        if (apu->ch2_len == 64) {
            apu->ch2_len--;
            if (apu->master->io[NR24] & NRX4_LEN_ENABLE)
                apu->ch2_enable = false;
        }
        // tick length timer
    }
    if (apu->apu_div % 4 == 0) {
        // tick ch1 sweep
    }
    if (apu->apu_div % 8 == 0) {
        apu->ch1_vol_counter++;
        if (apu->ch1_vol_counter == (apu->master->io[NR12] & NRX2_PACE)) {
            apu->ch1_vol_counter = 0;
            if (apu->master->io[NR12] & NRX2_DIR) {
                apu->ch1_volume++;
                if (apu->ch1_volume == 0x10) apu->ch1_volume = 0xf;
            } else {
                apu->ch1_volume--;
                if (apu->ch1_volume == 0xff) apu->ch1_volume = 0x0;
            }
        }

        apu->ch2_vol_counter++;
        if (apu->ch2_vol_counter == (apu->master->io[NR22] & NRX2_PACE)) {
            apu->ch2_vol_counter = 0;
            if (apu->master->io[NR22] & NRX2_DIR) {
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
