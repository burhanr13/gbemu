#include "ppu.h"

#include <SDL2/SDL.h>

#include "gb.h"

Uint32 colors[] = {0x00ffffff, 0x0000d000, 0x00008000, 0x00000000};

void init_ppu(struct gb* master, struct gb_ppu* ppu) {
    ppu->master = master;
    ppu->cycle = 0;
    ppu->scanline = 0;
}

static void load_bg_tile(struct gb_ppu* ppu) {
    u16 tilemap_offset = TILEMAP_SIZE * ppu->tileY + ppu->tileX;
    u16 tilemap_start;
    if (ppu->rendering_window && ppu->screenX >= (ppu->master->io[WX] - 7)) {
        tilemap_start =
            (ppu->master->io[LCDC] & LCDC_WINDOW_TILE_AREA) ? 0x1c00 : 0x1800;
    } else {
        tilemap_start =
            (ppu->master->io[LCDC] & LCDC_BG_MAP_AREA) ? 0x1c00 : 0x1800;
    }
    u8 tile_index = ppu->master->vram[0][tilemap_start + tilemap_offset];
    u16 tile_addr;
    if (ppu->master->io[LCDC] & LCDC_BG_TILE_AREA) {
        tile_addr = tile_index << 4;
    } else {
        tile_addr = 0x1000 + ((s8) tile_index << 4);
    }
    ppu->bg_tile_b0 = ppu->master->vram[0][tile_addr + 2 * ppu->fineY];
    ppu->bg_tile_b1 = ppu->master->vram[0][tile_addr + 2 * ppu->fineY + 1];
}

static void load_obj_tile(struct gb_ppu* ppu) {
    for (int i = 0; i < ppu->obj_ct; i++) {
        if (ppu->screenX != ppu->master->oam[ppu->objs[i] + 1] - 8) continue;
        int rel_y = ppu->scanline - ppu->master->oam[ppu->objs[i]] + 16;
        u8 tile_index = ppu->master->oam[ppu->objs[i] + 2];
        u8 obj_attr = ppu->master->oam[ppu->objs[i] + 3];
        if (ppu->master->io[LCDC] & LCDC_OBJ_SIZE) {
            if (obj_attr & OBJ_YFLIP) rel_y = 16 - rel_y;
            tile_index &= ~1;
        } else {
            if (obj_attr & OBJ_YFLIP) rel_y = 8 - rel_y;
        }
        rel_y &= 0xf;
        u8 obj_b0 = ppu->master->vram[0][(tile_index << 4) + 2 * rel_y];
        u8 obj_b1 = ppu->master->vram[0][(tile_index << 4) + 2 * rel_y + 1];
        u8 mask = ~(ppu->obj_tile_b0 | ppu->obj_tile_b1);
        ppu->obj_tile_b0 |= obj_b0 & mask;
        ppu->obj_tile_b1 |= obj_b1 & mask;
        if (obj_attr & OBJ_PAL) ppu->obj_tile_pal &= mask;
        if (obj_attr & OBJ_BGOVER) ppu->obj_tile_bgover &= mask;
    }
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
        if (ppu->cycle < MODE2_LEN) {
            if (ppu->cycle == 0) {
                ppu->master->io[STAT] &= ~STAT_MODE;
                ppu->master->io[STAT] |= 2;
                if (ppu->master->io[STAT] & STAT_I_OAM)
                    ppu->master->io[IF] |= I_STAT;

                if (ppu->scanline == 0) ppu->rendering_window = false;
                if (ppu->scanline == ppu->master->io[WY] &&
                    (ppu->master->io[LCDC] & LCDC_WINDOW_ENABLE)) {
                    ppu->rendering_window = true;
                    ppu->windowline = 0;
                }

                ppu->obj_ct = 0;
                ppu->obj_tile_b0 = 0;
                ppu->obj_tile_b1 = 0;
                ppu->obj_tile_pal = 0;
                ppu->obj_tile_bgover = 0;
            }

            if (!ppu->master->dma_active && !(ppu->cycle & 1) &&
                ppu->obj_ct < 10) {
                u8 obj_y = ppu->master->oam[2 * ppu->cycle];
                int rel_y = ppu->scanline - obj_y + 16;
                if (rel_y >= 0 &&
                    rel_y <
                        ((ppu->master->io[LCDC] & LCDC_OBJ_SIZE) ? 16 : 8)) {
                    ppu->objs[ppu->obj_ct++] = ppu->master->oam[2 * ppu->cycle];
                }
            }
        } else if (ppu->screenX < GB_SCREEN_W) {
            if (ppu->screenX == 0) {
                ppu->master->io[STAT] &= ~STAT_MODE;
                ppu->master->io[STAT] |= 3;

                if (ppu->rendering_window && ppu->master->io[WX] < 7) {
                    ppu->tileX = 0;
                    ppu->fineX = 6 - ppu->master->io[WX];
                    ppu->tileY = (ppu->windowline >> 3) & (TILEMAP_SIZE - 1);
                    ppu->fineY = ppu->windowline & 0b111;
                } else {
                    u8 curY = ppu->master->io[SCY] + ppu->scanline;
                    ppu->tileY = (curY >> 3) & (TILEMAP_SIZE - 1);
                    ppu->fineY = curY & 0b111;
                    ppu->tileX = ppu->master->io[SCX] >> 3;
                    ppu->fineX = ppu->master->io[SCX] & 0b111;
                }
                load_bg_tile(ppu);
                ppu->bg_tile_b0 <<= ppu->fineX;
                ppu->bg_tile_b1 <<= ppu->fineX;
            } else if (ppu->fineX == 0) {
                load_bg_tile(ppu);
            }

            int color = 0;
            if (ppu->master->io[LCDC] & LCDC_BG_ENABLE) {
                if (ppu->screenX == ppu->master->io[WX] - 7 &&
                    ppu->rendering_window) {
                    ppu->tileX = 0;
                    ppu->fineX = 0;
                    ppu->tileY = (ppu->windowline >> 3) & (TILEMAP_SIZE - 1);
                    ppu->fineY = ppu->windowline & 0b111;
                    load_bg_tile(ppu);
                }

                int bg_index = 0;
                if (ppu->bg_tile_b0 & 0x80) bg_index |= 0b01;
                if (ppu->bg_tile_b1 & 0x80) bg_index |= 0b10;
                color = (ppu->master->io[BGP] >> (2 * bg_index)) & 0b11;
            }
            if (!ppu->master->dma_active &&
                (ppu->master->io[LCDC] & LCDC_OBJ_ENABLE)) {
                load_obj_tile(ppu);

                int obj_index = 0;
                if (ppu->obj_tile_b0 & 0x80) obj_index |= 0b01;
                if (ppu->obj_tile_b1 & 0x80) obj_index |= 0b10;

                if ((color == 0 || !(ppu->obj_tile_bgover & 0x80)) &&
                    obj_index) {
                    if (ppu->obj_tile_pal & 0x80) {
                        color =
                            (ppu->master->io[OBP1] >> (2 * obj_index)) & 0b11;
                    } else {
                        color =
                            (ppu->master->io[OBP0] >> (2 * obj_index)) & 0b11;
                    }
                }
            }

            ppu->screen[ppu->scanline * (ppu->pitch / 4) + ppu->screenX] =
                colors[color];

            ppu->bg_tile_b0 <<= 1;
            ppu->bg_tile_b1 <<= 1;
            ppu->fineX++;
            if (ppu->fineX == 8) {
                ppu->tileX = (ppu->tileX + 1) & (TILEMAP_SIZE - 1);
                ppu->fineX = 0;
            }
            ppu->screenX++;
            ppu->obj_tile_b0 <<= 1;
            ppu->obj_tile_b1 <<= 1;
            ppu->obj_tile_pal <<= 1;
            ppu->obj_tile_bgover <<= 1;
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
        ppu->windowline++;
        if (ppu->scanline == SCANLINES_PER_FRAME) {
            ppu->scanline = 0;
            ppu->frame_complete = true;
        }
        ppu->master->io[LY] = ppu->scanline;
    }
}