#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "apu.h"
#include "cartridge.h"
#include "emulator.h"
#include "gb.h"
#include "ppu.h"
#include "sm83.h"

void center_screen_in_window(SDL_Rect* dst) {
    int windowW, windowH;
    SDL_GetWindowSize(gbemu.main_window, &windowW, &windowH);
    if (windowW > windowH) {
        dst->h = windowH;
        dst->y = 0;
        dst->w = dst->h * GB_SCREEN_W / GB_SCREEN_H;
        dst->x = (windowW - dst->w) / 2;
    } else {
        dst->w = windowW;
        dst->x = 0;
        dst->h = dst->w * GB_SCREEN_H / GB_SCREEN_W;
        dst->y = (windowH - dst->h) / 2;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("pass rom file name as argument\n");
        return -1;
    }

    if (!emulator_init()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "gbemu",
                                 "Initialization Error.", NULL);
        return -1;
    }

    if (!emu_load_rom(argv[1])) {
        return -1;
    }

    bool running = true;
    while (running) {
        if (gbemu.gb->cpu.ill) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "gbemu",
                                     "Illegal Opcode reached. Terminating.",
                                     gbemu.main_window);
            break;
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            emu_handle_event(e);
        }

        if (!gbemu.paused) {
            for (int i = 0; i < gbemu.speed - 1; i++) {
                emu_run_frame(false, !gbemu.muted);
            }
            emu_run_frame(true, !gbemu.muted);
        }

        SDL_RenderClear(gbemu.main_renderer);
        SDL_Rect dst;
        center_screen_in_window(&dst);
        SDL_RenderCopy(gbemu.main_renderer, gbemu.gb_screen, NULL, &dst);
        SDL_RenderPresent(gbemu.main_renderer);

        if (!gbemu.paused && !gbemu.muted && gbemu.gb->io[NR52]) {
            while (SDL_GetQueuedAudioSize(gbemu.gb_audio) > 8 * SAMPLE_BUF_LEN)
                SDL_Delay(1);
        } else {
            SDL_Delay(10);
        }
    }

    emulator_quit();
    return 0;
}
