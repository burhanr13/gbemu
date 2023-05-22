#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

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

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_CreateWindowAndRenderer(4 * GB_SCREEN_W, 4 * GB_SCREEN_H, 0, &window,
                                &renderer);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             GB_SCREEN_W, GB_SCREEN_H);

    printf("Loaded cartridge info-- mapper: %d, rom banks: %d, ram banks: %d\n",
           cart->mapper, cart->rom_banks, cart->ram_banks);

    struct gb* gb = calloc(1, sizeof(*gb));
    gb->cart = cart;
    init_cpu(gb, &gb->cpu);
    init_ppu(gb, &gb->ppu);

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
        }

        SDL_LockTexture(texture, NULL, (void**) &gb->ppu.screen,
                        &gb->ppu.pitch);
        while (!gb->ppu.frame_complete) {
            clock_timers(gb, cycle);
            ppu_clock(&gb->ppu);
            cpu_clock(&gb->cpu);
            cycle++;
        }
        gb->ppu.frame_complete = 0;
        SDL_UnlockTexture(texture);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        frame++;

        long wait_time = (1000 * frame) / 60 + start_time - SDL_GetTicks64();
        if(wait_time > 0) SDL_Delay(wait_time);
    }
    printf("\n%ld cycles, %ld frames\n", cycle, frame);

    free(gb);
    cart_destroy(cart);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
