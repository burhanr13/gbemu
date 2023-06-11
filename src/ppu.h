#ifndef PPU_H
#define PPU_H

#include <SDL2/SDL.h>

#include "types.h"

#define GB_SCREEN_W 160
#define GB_SCREEN_H 144
#define CYCLES_PER_SCANLINE 456
#define SCANLINES_PER_FRAME 154
#define MODE2_LEN 80

#define TILEMAP_SIZE 32

enum {
    LCDC_BG_ENABLE = 1 << 0,
    LCDC_OBJ_ENABLE = 1 << 1,
    LCDC_OBJ_SIZE = 1 << 2,
    LCDC_BG_MAP_AREA = 1 << 3,
    LCDC_BG_TILE_AREA = 1 << 4,
    LCDC_WINDOW_ENABLE = 1 << 5,
    LCDC_WINDOW_TILE_AREA = 1 << 6,
    LCDC_ENABLE = 1 << 7
};

enum {
    STAT_MODE = 0b11, // 0: hblank 1: vblank 2: searching oam 3: drawing
    STAT_LYCEQ = 1 << 2,
    STAT_I_HBLANK = 1 << 3,
    STAT_I_VBLANK = 1 << 4,
    STAT_I_OAM = 1 << 5,
    STAT_I_LYCEQ = 1 << 6
};

enum {
    BG_CPAL = 0b111,
    BG_BANK = 1 << 3,
    BG_XFLIP = 1 << 5,
    BG_YFLIP = 1 << 6,
    BG_BGOVER = 1 << 7
};

enum {
    OBJ_PAL = (1 << 4),
    OBJ_XFLIP = (1 << 5),
    OBJ_YFLIP = (1 << 6),
    OBJ_BGOVER = (1 << 7)
};

struct gb;

struct gb_ppu {
    struct gb* master;

    Uint32* screen;
    int pitch;

    u8 bg_tile_b0;
    u8 bg_tile_b1;
    
    u8 bg_tile_bgover;

    u8 tileX;
    u8 tileY;
    u8 fineX;
    u8 fineY;
    int screenX;
    bool rendering_window;
    int windowline;

    u8 obj_tile_b0;
    u8 obj_tile_b1;
    u8 obj_tile_pal;
    u8 obj_tile_bgover;

    u8 objs[10];
    u8 obj_ct;

    int cycle;
    int scanline;
    bool frame_complete;
};

void ppu_clock(struct gb_ppu* ppu);

#endif