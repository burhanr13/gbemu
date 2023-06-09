#include "emulator.h"

#include <SDL2/SDL.h>

#include "gb.h"

struct emulator gbemu;

bool emulator_init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) <
        0) {
        return false;
    }

    if (SDL_CreateWindowAndRenderer(1200, 700, SDL_WINDOW_RESIZABLE,
                                    &gbemu.main_window,
                                    &gbemu.main_renderer) < 0) {
        return false;
    }
    SDL_SetWindowTitle(gbemu.main_window, "gbemu");
    SDL_RenderPresent(gbemu.main_renderer);

    gbemu.gb_screen = SDL_CreateTexture(
        gbemu.main_renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_W, GB_SCREEN_H);

    SDL_AudioSpec audio_spec = {.freq = SAMPLE_FREQ,
                                .format = AUDIO_F32,
                                .channels = 2,
                                .samples = SAMPLE_BUF_LEN / 2};
    gbemu.gb_audio = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    if (gbemu.gb_audio == 0) {
        return false;
    }
    SDL_PauseAudioDevice(gbemu.gb_audio, 0);

    if (SDL_NumJoysticks() > 0) {
        gbemu.controller = SDL_GameControllerOpen(0);
    }

    gbemu.gb = malloc(sizeof *gbemu.gb);

    gbemu.paused = true;

    return true;
}

void emulator_quit() {
    free(gbemu.gb);
    cart_destroy(gbemu.cart);

    SDL_GameControllerClose(gbemu.controller);

    SDL_CloseAudioDevice(gbemu.gb_audio);

    SDL_DestroyRenderer(gbemu.main_renderer);
    SDL_DestroyWindow(gbemu.main_window);
    SDL_Quit();
}

void emu_handle_event(SDL_Event e) {
    gb_handle_event(gbemu.gb, &e);

    if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.sym) {
            case SDLK_r:
                emu_reset();
                break;
            case SDLK_p:
                gbemu.paused = !gbemu.paused;
                break;
            default:
                break;
        }
    }
}

void emu_run_frame(bool audio) {
    SDL_LockTexture(gbemu.gb_screen, NULL, (void**) &gbemu.gb->ppu.screen,
                    &gbemu.gb->ppu.pitch);
    while (!gbemu.gb->ppu.frame_complete) {
        tick_gb(gbemu.gb);
        if (gbemu.gb->apu.samples_full) {
            if (audio)
                SDL_QueueAudio(gbemu.gb_audio, gbemu.gb->apu.sample_buf,
                               sizeof gbemu.gb->apu.sample_buf);
            gbemu.gb->apu.samples_full = false;
        }
        gbemu.cycle++;
    }
    gbemu.gb->ppu.frame_complete = false;
    SDL_UnlockTexture(gbemu.gb_screen);
    gbemu.frame++;
}

bool emu_load_rom(char* filename) {
    gbemu.cart = cart_create(filename);
    if (!gbemu.cart) {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR, "gbemu",
            "Error loading rom! File does not exist or is invalid format.",
            gbemu.main_window);
        return false;
    }
    emu_reset();
    return true;
}

void emu_reset() {
    reset_gb(gbemu.gb, gbemu.cart);
    gbemu.cycle = 0;
    gbemu.frame = 0;
    gbemu.paused = false;
}