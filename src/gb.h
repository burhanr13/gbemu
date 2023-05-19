#ifndef GB_H
#define GB_H

#include "cartridge.h"
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
    JOYP = 0x00, // joypad
    SB = 0x01,   // serial bus
    SC = 0x02,   // serial control
    DIV = 0x04,  // divider
    TIMA = 0x05, // timer
    TMA = 0x06,  // timer modulo
    TAC = 0x07, // timer control
    IF = 0x0f,   // interrupt flag
    // 0x10 - 0x3f : sound
    // 0x40 - 0x4b : video
    // 0x4d - 0x77 : cgb
};

struct gb {
    struct sm83 cpu;

    struct cartridge* cart;

    u8 vram[1][VRAM_BANK_SIZE];
    u8 wram[2][WRAM_BANK_SIZE];

    u8 oam[OAM_SIZE];

    u8 io[IO_SIZE];

    u8 hram[HRAM_SIZE];

    u8 IE;

    u8 joypad;
};

u8 read8(struct gb* bus, u16 addr);
u16 read16(struct gb* bus, u16 addr);
void write8(struct gb* bus, u16 addr, u8 data);
void write16(struct gb* bus, u16 addr, u16 data);

void clock_timers(struct gb* gb, long cycle);

#endif