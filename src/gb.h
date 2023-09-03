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
#define CRAM_SIZE (8 * 4 * 2)
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

enum { CPS_ADDR = 0b00111111, CPS_INC = 1 << 7 };

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
    NR10 = 0x10,    // ch1 sweep
    NR11 = 0x11,    // ch1 length timer/duty cycle
    NR12 = 0x12,    // ch1 volume/envelope
    NR13 = 0x13,    // ch1 wavelen low
    NR14 = 0x14,    // ch1 wavelen hi/control
    NR21 = 0x16,    // ch2 length timer/duty cycle
    NR22 = 0x17,    // ch2 volume/envelope
    NR23 = 0x18,    // ch2 wavelen low
    NR24 = 0x19,    // ch2 wavelen hi/control
    NR30 = 0x1a,    // ch3 dac enable
    NR31 = 0x1b,    // ch3 length timer
    NR32 = 0x1c,    // ch3 output level
    NR33 = 0x1d,    // ch3 wavelen low
    NR34 = 0x1e,    // ch3 wavelen hi/control
    NR41 = 0x20,    // ch4 length timer
    NR42 = 0x21,    // ch4 volume/envelope
    NR43 = 0x22,    // ch4 frequency/randomness
    NR44 = 0x23,    // ch4 control
    NR50 = 0x24,    // master volume
    NR51 = 0x25,    // sound panning
    NR52 = 0x26,    // sound on/off
    WAVERAM = 0x30, // ch3 wave ram 0x30-0x3f
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
    KEY1 = 0x4d, // prepare speed switch
    VBK = 0x4f, // vram bank
    HDMA1 = 0x51, // vram dma source hi
    HDMA2 = 0x52, // vram dma source lo
    HDMA3 = 0x53, // vram dma dest hi
    HDMA4 = 0x54, // vram dma dest lo
    HDMA5 = 0x55, // vram dma length/mode/start
    RP = 0x56, // IR communication
    BCPS = 0x68, // bg color palette spec
    BCPD = 0x69, // bg color palette data
    OCPS = 0x6a, // obj color palette spec
    OCPD = 0x6b, // obj color palette data
    OPRI = 0x6c, // obj priority mode
    SVBK = 0x70, // wram bank
    PCM12 = 0x76, // ch1,2 output
    PCM34 = 0x77 // ch3,4 output
};

struct gb {
    struct sm83 cpu;
    struct gb_ppu ppu;
    struct gb_apu apu;

    struct cartridge* cart;

    bool cgb_mode;

    u8 vram[2][VRAM_BANK_SIZE];
    u8 wram[8][WRAM_BANK_SIZE];

    u8 oam[OAM_SIZE];

    u8 io[IO_SIZE];

    u8 bg_cram[CRAM_SIZE];
    u8 obj_cram[CRAM_SIZE];

    u8 hram[HRAM_SIZE];

    u8 IE;

    u16 div;

    bool prev_timer_inc;
    bool timer_overflow;

    bool prev_stat_int;

    u8 jp_dir;
    u8 jp_action;

    bool dma_start;
    bool dma_active;
    u8 dma_index;

    u16 hdma_src;
    u16 hdma_dest;
    bool hdma_active;
    bool hdma_hblank;
    u8 hdma_block;
    u8 hdma_index;
};

u8 read8(struct gb* bus, u16 addr);
void write8(struct gb* bus, u16 addr, u8 data);

void gb_handle_event(struct gb* gb, SDL_Event* e);

void check_stat_irq(struct gb* gb);
void clock_timers(struct gb* gb);
void update_joyp(struct gb* gb);
void run_dma(struct gb* gb);
void run_hdma(struct gb* gb);

void gb_m_cycle(struct gb* gb);

void reset_gb(struct gb* gb, struct cartridge* cart);

#endif