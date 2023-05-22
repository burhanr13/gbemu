#include "ppu.h"

#include <SDL2/SDL.h>

#include "gb.h"

Uint32 colors[] = {0x00000000, 0x00008000, 0x0000d000, 0x00ffffff};

void init_ppu(struct gb* master, struct gb_ppu* ppu) {
    ppu->master = master;
    ppu->cycle = 0;
    ppu->scanline = 0;
}

static void load_bg_tile(struct gb_ppu* ppu) {
    u16 tilemap_offset = TILEMAP_SIZE * ppu->tileY + ppu->tileX;
    u16 tilemap_start =
        (ppu->master->io[LCDC] & LCDC_BG_MAP_AREA) ? 0x1c00 : 0x1800;
    u8 tile_index = ppu->master->vram[0][tilemap_start + tilemap_offset];
    u16 tile_addr;
    if (ppu->master->io[LCDC] & LCDC_BG_TILE_AREA) {
        tile_addr = tile_index << 4;
    } else {
        s8 signed_index = tile_index;
        tile_addr = 0x1000 + ((s16) signed_index << 4);
    }
    ppu->bg_tile_b0 = ppu->master->vram[0][tile_addr] + 2 * ppu->fineY;
    ppu->bg_tile_b1 = ppu->master->vram[0][tile_addr] + 1 + 2 * ppu->fineY;
}

void ppu_clock(struct gb_ppu* ppu) {
    if (ppu->master->io[LYC] == ppu->master->io[LY]) {
        if (!(ppu->master->io[STAT] & STAT_LCYEQ) &&
            (ppu->master->io[STAT] & STAT_I_LCYEQ)) {
            ppu->master->io[IF] |= I_STAT;
        }
        ppu->master->io[STAT] |= STAT_LCYEQ;
    }
    if (!(ppu->master->io[LCDC] & LCDC_ENABLE)) return;
    if (ppu->scanline < GB_SCREEN_H) {
        if (ppu->cycle == 0) {
            ppu->master->io[STAT] &= ~STAT_MODE;
            ppu->master->io[STAT] |= 2;
            if (ppu->master->io[STAT] & STAT_I_OAM)
                ppu->master->io[IF] |= I_STAT;
            // search OAM for sprites
        } else if (ppu->cycle < MODE2_LEN) {
            // do nothing
        } else if (ppu->screenX < GB_SCREEN_W) {
            if (ppu->screenX == 0) {
                ppu->master->io[STAT] &= ~STAT_MODE;
                ppu->master->io[STAT] |= 3;

                u8 curY = ppu->master->io[SCY] + ppu->scanline;
                ppu->tileY = curY >> 3;
                ppu->fineY = curY & 0b111;
                ppu->tileX = ppu->master->io[SCX] >> 3;
                ppu->fineX = ppu->master->io[SCX] & 0b111;
                load_bg_tile(ppu);
                ppu->bg_tile_b0 <<= ppu->fineX;
                ppu->bg_tile_b1 <<= ppu->fineX;
            } else if (ppu->fineX == 0) {
                load_bg_tile(ppu);
            }

            if (ppu->master->io[LCDC] & LCDC_BG_ENABLE) {
                int bg_index = 0;
                if (ppu->bg_tile_b0 & 0x80) bg_index |= 0b01;
                if (ppu->bg_tile_b1 & 0x80) bg_index |= 0b10;
                ppu->screen[ppu->scanline * (ppu->pitch / 4) + ppu->screenX] =
                    colors[bg_index];
            }

            ppu->bg_tile_b0 <<= 1;
            ppu->bg_tile_b1 <<= 1;
            ppu->fineX++;
            if (ppu->fineX == 8) {
                ppu->tileX = (ppu->tileX + 1) & (TILEMAP_SIZE - 1);
                ppu->fineX = 0;
            }
            ppu->screenX++;
        } else if (ppu->screenX == GB_SCREEN_W) {
            ppu->screenX = 0;
            ppu->master->io[STAT] &= ~STAT_MODE;
            ppu->master->io[STAT] |= 1;
            if (ppu->master->io[STAT] & STAT_I_HBLANK)
                ppu->master->io[IF] |= I_STAT;
        }
    } else if (ppu->scanline == GB_SCREEN_H && ppu->cycle == 0) {
        ppu->master->io[IF] |= I_VBLANK;
        ppu->master->io[STAT] &= ~STAT_MODE;
        if (ppu->master->io[STAT] & STAT_I_VBLANK)
            ppu->master->io[IF] |= I_STAT;
    }
    ppu->cycle++;
    if (ppu->cycle == CYCLES_PER_SCANLINE) {
        ppu->cycle = 0;
        ppu->scanline++;
        if (ppu->scanline == SCANLINES_PER_FRAME) {
            ppu->scanline = 0;
            ppu->frame_complete = 1;
        }
        ppu->master->io[LY] = ppu->scanline;
    }
}