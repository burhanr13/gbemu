#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "apu.h"
#include "cartridge.h"
#include "gb.h"
#include "ppu.h"
#include "sm83.h"

#define FPS 60

void run_emulator_audio(void* data, Uint8* stream, int len) {
    struct gb* gb = data;
    gb->apu.sample_buf = stream;
    while (!gb->apu.samples_full) {
        tick_gb(gb);
        if (gb->ppu.frame_complete) {
            gb->ppu.frame_complete = false;
            SDL_UnlockTexture(gb->ppu.texture);
            SDL_RenderClear(gb->ppu.renderer);
            SDL_RenderCopy(gb->ppu.renderer, gb->ppu.texture, NULL, NULL);
            SDL_RenderPresent(gb->ppu.renderer);
            SDL_LockTexture(gb->ppu.texture, NULL, (void**) &gb->ppu.screen,
                            &gb->ppu.pitch);
        }
    }
    gb->apu.samples_full = false;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("pass rom file name as argument\n");
        return -1;
    }

    struct cartridge* cart = cart_create(argv[1]);
    if (!cart) {
        printf("error loading rom\n");
        return -1;
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_CreateWindowAndRenderer(4 * GB_SCREEN_W, 4 * GB_SCREEN_H, 0, &window,
                                &renderer);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             GB_SCREEN_W, GB_SCREEN_H);

    struct gb* gb = calloc(1, sizeof(*gb));
    gb->cart = cart;
    init_cpu(gb, &gb->cpu);
    init_ppu(gb, &gb->ppu);
    gb->ppu.renderer = renderer;
    gb->ppu.texture = texture;
    SDL_LockTexture(texture, NULL, (void**) &gb->ppu.screen, &gb->ppu.pitch);

    init_apu(gb, &gb->apu);
    SDL_AudioSpec audio_spec = {.freq = SAMPLE_FREQ,
                                .format = AUDIO_U8,
                                .channels = 2,
                                .samples = SAMPLE_BUF_LEN / 2,
                                .callback = run_emulator_audio,
                                .userdata = gb};
    SDL_AudioDeviceID audio_id =
        SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    SDL_PauseAudioDevice(audio_id, 0);
    gb->apu.audio_id = audio_id;

    Uint64 last_poll_time = SDL_GetTicks64();

    bool running = true;
    while (running) {
        if (gb->cpu.ill) {
            printf("illegal instruction reached -- terminating\n");
            running = false;
            break;
        }

        SDL_Event e;

        Uint64 time = SDL_GetTicks64();
        if (time - last_poll_time >= 50) {
            last_poll_time = time;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) running = false;
                gb_handle_event(gb, &e);
            }
        }
        
    }

    SDL_CloseAudioDevice(audio_id);

    free(gb);
    cart_destroy(cart);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
