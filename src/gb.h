#ifndef GB_H
#define GB_H

#include "apu.h"
#include "cartridge.h"
#include "ppu.h"
#include "sm83.h"
#include "types.h"

#define VRAM_BANK_SIZE 0x2000 // 8k
#define WRAM_BANK_SIZE 0x1000 // 4k

#define OAM_SIZE 0xa0
#define IO_SIZE 0x80
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
    NR10 = 0x10, // ch1 sweep
    NR11 = 0x11, // ch1 length timer/duty cycle
    NR12 = 0x12, // ch1 volume/envelope
    NR13 = 0x13, // ch1 wavelen low
    NR14 = 0x14, // ch1 wavelen hi/control
    // 0x40 - 0x4b : video
    LCDC = 0x40, // lcd control
    STAT = 0x41, // lcd status
    SCY = 0x42,  // scroll y
    SCX = 0x43,  // scroll x
    LY = 0x44,   // lcd y
    LYC = 0x45,  // lcd y compare
    DMA = 0x46,  // oam dma start
    BGP = 0x47,  // bg palette
    OBP0 = 0x48, // obj palette 0
    OBP1 = 0x49, // obj palette 1
    WY = 0x4a,   // window y
    WX = 0x4b,   // window x + 7
    // 0x4d - 0x77 : cgb
};

struct gb {
    struct sm83 cpu;
    struct gb_ppu ppu;
    struct gb_apu apu;

    struct cartridge* cart;

    u8 vram[1][VRAM_BANK_SIZE];
    u8 wram[2][WRAM_BANK_SIZE];

    u8 oam[OAM_SIZE];

    u8 io[IO_SIZE];

    u8 hram[HRAM_SIZE];

    u8 IE;

    u16 div;

    bool prev_timer_inc;
    bool timer_overflow;

    bool prev_stat_int;

    u8 jp_dir;
    u8 jp_action;

    bool dma_active;
    u8 dma_index;
    int dma_cycles;
};

u8 read8(struct gb* bus, u16 addr);
u16 read16(struct gb* bus, u16 addr);
void write8(struct gb* bus, u16 addr, u8 data);
void write16(struct gb* bus, u16 addr, u16 data);

void gb_handle_event(struct gb* gb, SDL_Event* e);

void check_stat_irq(struct gb* gb);
void clock_timers(struct gb* gb);
void update_joyp(struct gb* gb);
void run_dma(struct gb* gb);

void tick_gb(struct gb* gb);

#endif