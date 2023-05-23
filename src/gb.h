#ifndef GB_H
#define GB_H

#include "cartridge.h"
#include "ppu.h"
#include "sm83.h"
#include "types.h"

#define VRAM_BANK_SIZE 0x2000 // 8k
#define WRAM_BANK_SIZE 0x1000 // 4k

#define OAM_SIZE 0xa0
#define IO_SIZE  0x80
#define HRAM_SIZE 0x7f

enum {
    I_VBLANK = 1 << 0,
    I_STAT = 1 << 1,
    I_TIMER = 1 << 2,
    I_SERIAL = 1 << 3,
    I_JOYPAD = 1 << 4
};

enum {
    JP_R_A = (1 << 0),
    JP_L_B = (1 << 1),
    JP_U_SL = (1 << 2),
    JP_D_ST = (1 << 3),
    JP_DIR = (1 << 4),
    JP_ACT = (1 << 5)
};

enum {
    JOYP = 0x00, // joypad
    SB = 0x01,   // serial bus
    SC = 0x02,   // serial control
    DIV = 0x04,  // divider
    TIMA = 0x05, // timer
    TMA = 0x06,  // timer modulo
    TAC = 0x07,  // timer control
    IF = 0x0f,   // interrupt flag
    // 0x10 - 0x3f : sound
    // 0x40 - 0x4b : video
    LCDC = 0x40, // lcd control
    STAT = 0x41, // lcd status
    SCY = 0x42,  // scroll y
    SCX = 0x43,  // scroll x
    LY = 0x44,   // lcd y
    LYC = 0x45,  // lcd y compare
    BGP = 0x47,  // bg palette
    WY = 0x4a,   // window y
    WX = 0x4b,   // window x + 7
    // 0x4d - 0x77 : cgb
};

struct gb {
    struct sm83 cpu;
    struct gb_ppu ppu;

    struct cartridge* cart;

    u8 vram[1][VRAM_BANK_SIZE];
    u8 wram[2][WRAM_BANK_SIZE];

    u8 oam[OAM_SIZE];

    u8 io[IO_SIZE];

    u8 hram[HRAM_SIZE];

    u8 IE;

    u8 jp_dir;
    u8 jp_action;
};

u8 read8(struct gb* bus, u16 addr);
u16 read16(struct gb* bus, u16 addr);
void write8(struct gb* bus, u16 addr, u8 data);
void write16(struct gb* bus, u16 addr, u16 data);

void clock_timers(struct gb* gb, long cycle);
void update_joyp(struct gb* gb);
void gb_handle_event(struct gb* gb, SDL_Event* e);

#endif