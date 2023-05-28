#include "apu.h"

#include <SDL2/SDL.h>

#include "gb.h"

u8 duty_cycles[] = {0b11111110, 0b01111110, 0b01111000, 0b10000001};

void init_apu(struct gb* master, struct gb_apu* apu) {
    apu->master = master;
    apu->apu_div = 0;
}

u8 get_sample_ch1(struct gb_apu* apu) {
    u8 sample = (duty_cycles[2] & (1 << (apu->ch1_duty_index & 7))) ? 1 : 0;
    sample *= 10;
    return sample;
}

void apu_clock(struct gb_apu* apu) {
    apu->ch1_counter += 2;
    if (apu->ch1_counter == 2048) {
        apu->ch1_counter = apu->ch1_wavelen;
        apu->ch1_duty_index++;
    }
    // same for other channels

    if (apu->master->div % SAMPLE_RATE == 0) {
        u8 ch1_sample = get_sample_ch1(apu);

        u8 l_sample = 0, r_sample = 0;
        l_sample += ch1_sample;
        r_sample = ch1_sample;
        apu->sample_buf[apu->sample_ind++] = l_sample;
        apu->sample_buf[apu->sample_ind++] = r_sample;
        if(apu->sample_ind == SAMPLE_BUF_LEN) {
            SDL_QueueAudio(apu->audio_id, apu->sample_buf, SAMPLE_BUF_LEN);
            apu->sample_ind = 0;
        }
    }
    if (apu->master->div % APU_DIV_RATE == 0) apu->apu_div++;

    if (apu->apu_div % 2 == 0) {
        // tick length timer
    }
    if (apu->apu_div % 4 == 0) {
        // tick ch1 sweep
    }
    if (apu->apu_div % 8 == 0) {
        // tick envelopes
    }
}
