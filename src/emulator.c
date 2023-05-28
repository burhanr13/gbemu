#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "apu.h"
#include "cartridge.h"
#include "gb.h"
#include "ppu.h"
#include "sm83.h"

#define FPS 60

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

    SDL_AudioSpec audio_spec = {.freq = SAMPLE_FREQ,
                                .format = AUDIO_U8,
                                .channels = 2,
                                .samples = SAMPLE_BUF_LEN / 2};
    SDL_AudioDeviceID audio_id =
        SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    SDL_PauseAudioDevice(audio_id, 0);

    struct gb* gb = calloc(1, sizeof(*gb));
    gb->cart = cart;
    init_cpu(gb, &gb->cpu);
    init_ppu(gb, &gb->ppu);
    init_apu(gb, &gb->apu);
    gb->apu.audio_id = audio_id;

    long cycle = 0;
    long frame = 0;

    Uint64 start_time = SDL_GetTicks64();

    int running = 1;
    while (running) {
        if (gb->cpu.ill) {
            printf("illegal instruction reached -- terminating\n");
            break;
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            gb_handle_event(gb, &e);
        }

        SDL_LockTexture(texture, NULL, (void**) &gb->ppu.screen,
                        &gb->ppu.pitch);
        while (!gb->ppu.frame_complete) {
            tick_gb(gb);
            cycle++;
        }
        gb->ppu.frame_complete = false;
        SDL_UnlockTexture(texture);

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        frame++;

        long wait_time = (1000 * frame) / FPS + start_time -
        SDL_GetTicks64(); if(wait_time > 0) SDL_Delay(wait_time);
        //while (SDL_GetQueuedAudioSize(audio_id))
        //    ;
    }

    free(gb);
    cart_destroy(cart);

    SDL_CloseAudioDevice(audio_id);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
