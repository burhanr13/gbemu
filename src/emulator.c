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

    printf("Loaded cartridge info: mapper %d rom banks %d ram banks %d\n",
           cart->mapper, cart->rom_banks, cart->ram_banks);

    struct gb* gb = calloc(1, sizeof(*gb));
    gb->cart = cart;
    init_cpu(gb, &gb->cpu);

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }
        if (gb->cpu.cycles == 0) {
            fprintf(stderr,
                    "A: %02X F: %02X B: %02X C: %02X D: %02X E: %02X H: %02X "
                    "L: %02X SP: %04X PC: 00:%04X (%02X %02X %02X %02X)\n",
                    gb->cpu.A, gb->cpu.F, gb->cpu.B, gb->cpu.C, gb->cpu.D,
                    gb->cpu.E, gb->cpu.H, gb->cpu.L, gb->cpu.SP, gb->cpu.PC,
                    read8(gb, gb->cpu.PC), read8(gb, gb->cpu.PC + 1),
                    read8(gb, gb->cpu.PC + 2), read8(gb, gb->cpu.PC + 3));
        }
        cpu_clock(&gb->cpu);
    }

    free(gb);
    cart_destroy(cart);

    SDL_Quit();
    return 0;
}
