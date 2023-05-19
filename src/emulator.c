#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "cartridge.h"
#include "gb.h"
#include "sm83.h"

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    if (argc < 2) {
        printf("pass rom file name as argument\n");
        return -1;
    }

    struct cartridge* cart = cart_create(argv[1]);
    if (!cart) {
        printf("error loading rom\n");
        return -1;
    }

    printf("Loaded cartridge info-- mapper: %d, rom banks: %d, ram banks: %d\n",
           cart->mapper, cart->rom_banks, cart->ram_banks);

    struct gb* gb = calloc(1, sizeof(*gb));
    gb->cart = cart;
    init_cpu(gb, &gb->cpu);

    long cycle = 0;

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }

        clock_timers(gb, cycle);
        cpu_clock(&gb->cpu);
        cycle++;
    }
    printf("\n%ld cycles ran\n", cycle);

    free(gb);
    cart_destroy(cart);

    SDL_Quit();
    return 0;
}
