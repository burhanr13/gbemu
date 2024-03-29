#ifndef EMULATOR_H
#define EMULATOR_H

#include <SDL2/SDL.h>

#include "cartridge.h"
#include "gb.h"
#include "types.h"

struct emulator {
    SDL_Window* main_window;
    SDL_Renderer* main_renderer;

    SDL_Texture* gb_screen;
    SDL_AudioDeviceID gb_audio;
    SDL_GameController* controller;

    struct gb* gb;
    struct cartridge* cart;

    unsigned long frame;

    bool paused;
    bool muted;

    bool force_dmg;
    Uint32 dmg_colors[4];

    bool speedup;
    int speedup_speed;
    int speed;
};

extern struct emulator gbemu;

bool emulator_init();
void emulator_quit();

void emu_handle_event(SDL_Event e);

void emu_run_frame(bool video, bool audio);

bool emu_load_rom(char* filename);
void emu_reset();

void save_state();
void load_state();

#endif