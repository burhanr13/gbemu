#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "apu.h"
#include "cartridge.h"
#include "gb.h"
#include "ppu.h"
#include "sm83.h"

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

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_CreateWindowAndRenderer(1200, 700, SDL_WINDOW_RESIZABLE, &window,
                                &renderer);
    SDL_SetWindowTitle(window, "gbemu");
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             GB_SCREEN_W, GB_SCREEN_H);

    SDL_AudioSpec audio_spec = {.freq = SAMPLE_FREQ,
                                .format = AUDIO_F32,
                                .channels = 2,
                                .samples = SAMPLE_BUF_LEN / 2};
    SDL_AudioDeviceID audio_id =
        SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    SDL_PauseAudioDevice(audio_id, 0);

    SDL_GameController* controller = NULL;
    if (SDL_NumJoysticks() > 0) {
        controller = SDL_GameControllerOpen(0);
    }

    struct gb* gb = malloc(sizeof *gb);
    reset_gb(gb, cart);

    long cycle = 0;
    long frame = 0;
    double fps = 0.0;

    bool running = true;
    while (running) {
        if (gb->cpu.ill) {
            printf("illegal instruction reached -- terminating\n");
            break;
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            gb_handle_event(gb, &e);
        }

        SDL_LockTexture(texture, NULL, (void**) &gb->ppu.screen,
                        &gb->ppu.pitch);
        while (!gb->ppu.frame_complete) {
            tick_gb(gb);
            if (gb->apu.samples_full) {
                SDL_QueueAudio(audio_id, gb->apu.sample_buf,
                               (sizeof(float)) * SAMPLE_BUF_LEN);
                gb->apu.samples_full = false;
            }
            cycle++;
        }
        gb->ppu.frame_complete = false;
        SDL_UnlockTexture(texture);

        SDL_RenderClear(renderer);
        int windowW, windowH;
        SDL_GetWindowSize(window, &windowW, &windowH);
        SDL_Rect dst;
        if (windowW > windowH) {
            dst.h = windowH;
            dst.y = 0;
            dst.w = dst.h * GB_SCREEN_W / GB_SCREEN_H;
            dst.x = (windowW - dst.w) / 2;
        } else {
            dst.w = windowW;
            dst.x = 0;
            dst.h = dst.w * GB_SCREEN_H / GB_SCREEN_W;
            dst.y = (windowH - dst.h) / 2;
        }
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_RenderPresent(renderer);
        frame++;

        if (gb->io[NR52]) {
            while (SDL_GetQueuedAudioSize(audio_id) > 4 * SAMPLE_BUF_LEN)
                SDL_Delay(1);
        } else {
            SDL_Delay(16);
        }
        fps = 1000.0 * frame / SDL_GetTicks64();
    }
    printf("cycles: %ld, frames: %ld, fps: %lf\n", cycle, frame, fps);

    free(gb);
    cart_destroy(cart);

    SDL_GameControllerClose(controller);

    SDL_CloseAudioDevice(audio_id);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
