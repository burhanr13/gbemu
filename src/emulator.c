#include "emulator.h"

#include <SDL2/SDL.h>
#include <zlib.h>

#include "gb.h"
#include "sm83.h"

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

    gbemu.dmg_colors[0] = 0x00ffffff;
    gbemu.dmg_colors[1] = 0x0000e000;
    gbemu.dmg_colors[2] = 0x0009000;
    gbemu.dmg_colors[3] = 0x00000000;

    gbemu.speed = 1;
    gbemu.speedup_speed = 5;

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
            case SDLK_t:
                gbemu.force_dmg = !gbemu.force_dmg;
            case SDLK_r:
                emu_reset();
                break;
            case SDLK_p:
                gbemu.paused = !gbemu.paused;
                break;
            case SDLK_TAB:
                gbemu.speedup = !gbemu.speedup;
                if (gbemu.speedup) {
                    gbemu.speed = gbemu.speedup_speed;
                } else {
                    gbemu.speed = 1;
                }
                break;
            case SDLK_m:
                gbemu.muted = !gbemu.muted;
                break;
            case SDLK_9:
                save_state();
                break;
            case SDLK_0:
                load_state();
                break;
            default:
                break;
        }
    }
}

void update_texture() {
    Uint32* pixels;
    int pitch;
    SDL_LockTexture(gbemu.gb_screen, NULL, (void**) &pixels, &pitch);
    memcpy(pixels, gbemu.gb->ppu.screen, sizeof gbemu.gb->ppu.screen);
    SDL_UnlockTexture(gbemu.gb_screen);
}

void emu_run_frame(bool video, bool audio) {
    if (!(gbemu.gb->io[LCDC] & LCDC_ENABLE)) {
        for (int i = 0; i < 10000; i++) {
            cpu_clock(&gbemu.gb->cpu);
            if (gbemu.gb->apu.samples_full) {
                if (audio)
                    SDL_QueueAudio(
                        gbemu.gb_audio,
                        gbemu.gb->apu
                            .sample_buf[(gbemu.gb->apu.buf_ind - 1) % 2],
                        sizeof gbemu.gb->apu.sample_buf);
                gbemu.gb->apu.samples_full = false;
            }
            if (gbemu.gb->io[LCDC] & LCDC_ENABLE) break;
        }
        return;
    }

    while (!gbemu.gb->ppu.frame_complete) {
        cpu_clock(&gbemu.gb->cpu);
        if (gbemu.gb->apu.samples_full) {
            if (audio)
                SDL_QueueAudio(
                    gbemu.gb_audio,
                    gbemu.gb->apu.sample_buf[(gbemu.gb->apu.buf_ind - 1) % 2],
                    sizeof gbemu.gb->apu.sample_buf);
            gbemu.gb->apu.samples_full = false;
        }
        if (!(gbemu.gb->io[LCDC] & LCDC_ENABLE)) break;
    }
    gbemu.gb->ppu.frame_complete = false;
    if (video) update_texture();
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
    gbemu.frame = 0;
    gbemu.paused = false;
}

/*
save state format:
16byte cartridge header
contents of gb struct
cartridge state
cartridge ram (if any)
cartridge rtc (if any)
and this is all compressed in gzip format
*/

void save_state() {
    gzFile sst_file = gzopen(gbemu.cart->sst_filename, "wb");
    gzfwrite(gbemu.cart->title, sizeof gbemu.cart->title, 1, sst_file);

    gbemu.gb->cart = NULL;
    gbemu.gb->cpu.master = NULL;
    gbemu.gb->ppu.master = NULL;
    gbemu.gb->apu.master = NULL;
    gzfwrite(gbemu.gb, sizeof *gbemu.gb, 1, sst_file);
    gbemu.gb->cart = gbemu.cart;
    gbemu.gb->cpu.master = gbemu.gb;
    gbemu.gb->ppu.master = gbemu.gb;
    gbemu.gb->apu.master = gbemu.gb;

    gzfwrite(&gbemu.cart->st, sizeof gbemu.cart->st, 1, sst_file);
    if (gbemu.cart->ram_banks)
        gzfwrite(gbemu.cart->ram, SRAM_BANK_SIZE, gbemu.cart->ram_banks,
                 sst_file);
    if (gbemu.cart->has_rtc)
        gzfwrite(gbemu.cart->rtc, sizeof *gbemu.cart->rtc, 1, sst_file);

    gzclose(sst_file);
}

void load_state() {
    gzFile sst_file = gzopen(gbemu.cart->sst_filename, "rb");
    if (!sst_file) {
        gzclose(sst_file);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "gbemu",
                                 "No Save State!", gbemu.main_window);
        return;
    }

    u8 title[0x10];
    gzfread(title, sizeof gbemu.cart->title, 1, sst_file);
    if (memcmp(title, gbemu.cart->title, sizeof title)) {
        gzclose(sst_file);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "gbemu",
                                 "Invalid Save State!", gbemu.main_window);
        return;
    }

    gzfread(gbemu.gb, sizeof *gbemu.gb, 1, sst_file);
    gbemu.gb->cart = gbemu.cart;
    gbemu.gb->cpu.master = gbemu.gb;
    gbemu.gb->ppu.master = gbemu.gb;
    gbemu.gb->apu.master = gbemu.gb;

    gzfread(&gbemu.cart->st, sizeof gbemu.cart->st, 1, sst_file);
    if (gbemu.cart->ram_banks)
        gzfread(gbemu.cart->ram, SRAM_BANK_SIZE, gbemu.cart->ram_banks,
                sst_file);
    if (gbemu.cart->has_rtc)
        gzfread(gbemu.cart->rtc, sizeof *gbemu.cart->rtc, 1, sst_file);

    gzclose(sst_file);

    update_texture();
}