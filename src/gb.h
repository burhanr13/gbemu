#ifndef GB_H
#define GB_H

#include "cartridge.h"
#include "sm83.h"
#include "types.h"

#define VRAM_BANK_SIZE 0x2000 // 8k
#define WRAM_BANK_SIZE 0x1000 // 4k

#define OAM_SIZE 0xa0
#define HRAM_SIZE 0x7f

struct gb {
    struct sm83 cpu;

    struct cartridge* cart;

    u8 vram[1][VRAM_BANK_SIZE];
    u8 wram[2][WRAM_BANK_SIZE];

    u8 oam[OAM_SIZE];

    u8 hram[HRAM_SIZE];

    u8 IME;
    u8 IE;
    u8 IF;
};

u8 read8(struct gb* bus, u16 addr);
u16 read16(struct gb* bus, u16 addr);
void write8(struct gb* bus, u16 addr, u8 data);
void write16(struct gb* bus, u16 addr, u16 data);

#endif