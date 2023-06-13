#include "ppu.h"

#include <SDL2/SDL.h>

#include "emulator.h"
#include "gb.h"

static u8 reverse_byte(u8 b) {
    b = (b & 0xf0) >> 4 | (b & 0x0f) << 4;
    b = (b & 0xcc) >> 2 | (b & 0x33) << 2;
    b = (b & 0xaa) >> 1 | (b & 0x55) << 1;
    return b;
}

static void load_bg_tile(struct gb_ppu* ppu) {
    u16 tilemap_offset;
    u16 tilemap_start;
    if ((ppu->master->io[LCDC] & LCDC_WINDOW_ENABLE) && ppu->rendering_window &&
        ppu->screenX >= (ppu->master->io[WX] - 7)) {
        tilemap_offset =
            TILEMAP_SIZE * ppu->tileY + ((ppu->tileX) & (TILEMAP_SIZE - 1));
        tilemap_start =
            (ppu->master->io[LCDC] & LCDC_WINDOW_TILE_AREA) ? 0x1c00 : 0x1800;
    } else {
        tilemap_offset =
            TILEMAP_SIZE * ppu->tileY +
            ((ppu->tileX + (ppu->master->io[SCX] >> 3)) & (TILEMAP_SIZE - 1));
        tilemap_start =
            (ppu->master->io[LCDC] & LCDC_BG_MAP_AREA) ? 0x1c00 : 0x1800;
    }
    u8 tile_index = ppu->master->vram[0][tilemap_start + tilemap_offset];
    u8 tile_attr = ppu->master->vram[1][tilemap_start + tilemap_offset];
    u16 tile_addr;
    if (ppu->master->io[LCDC] & LCDC_BG_TILE_AREA) {
        tile_addr = tile_index << 4;
    } else {
        tile_addr = 0x1000 + ((s8) tile_index << 4);
    }
    u8 bank = (tile_attr & BG_BANK) ? 1 : 0;
    u8 off_y = ppu->fineY;
    if (tile_attr & BG_YFLIP) off_y = 7 - off_y;
    ppu->bg_tile_b0 = ppu->master->vram[bank][tile_addr + 2 * off_y];
    ppu->bg_tile_b1 = ppu->master->vram[bank][tile_addr + 2 * off_y + 1];
    if (tile_attr & BG_XFLIP) {
        ppu->bg_tile_b0 = reverse_byte(ppu->bg_tile_b0);
        ppu->bg_tile_b1 = reverse_byte(ppu->bg_tile_b1);
    }
    if (tile_attr & BG_BGOVER) ppu->bg_tile_bgover = ~0;
    if ((tile_attr & BG_CPAL) & 0b001) ppu->bg_tile_cpal_b0 = ~0;
    if ((tile_attr & BG_CPAL) & 0b010) ppu->bg_tile_cpal_b1 = ~0;
    if ((tile_attr & BG_CPAL) & 0b100) ppu->bg_tile_cpal_b2 = ~0;
}

static void load_obj_tile(struct gb_ppu* ppu) {
    for (int i = 0; i < ppu->obj_ct; i++) {
        if (ppu->screenX != ppu->master->oam[ppu->objs[i] + 1] - 8) continue;
        int rel_y = ppu->scanline - ppu->master->oam[ppu->objs[i]] + 16;
        u8 tile_index = ppu->master->oam[ppu->objs[i] + 2];
        u8 obj_attr = ppu->master->oam[ppu->objs[i] + 3];
        if (ppu->master->io[LCDC] & LCDC_OBJ_SIZE) {
            if (obj_attr & OBJ_YFLIP) rel_y = 15 - rel_y;
            tile_index &= ~1;
        } else {
            if (obj_attr & OBJ_YFLIP) rel_y = 7 - rel_y;
        }
        u8 bank = (obj_attr & OBJ_BANK) ? 1 : 0;
        u8 obj_b0 = ppu->master->vram[bank][(tile_index << 4) + 2 * rel_y];
        u8 obj_b1 = ppu->master->vram[bank][(tile_index << 4) + 2 * rel_y + 1];
        if (obj_attr & OBJ_XFLIP) {
            obj_b0 = reverse_byte(obj_b0);
            obj_b1 = reverse_byte(obj_b1);
        }

        u8 mask = 0;
        if (ppu->master->cgb_mode) {
            for (int j = 0; j < 8; j++) {
                if (ppu->obj_oam_inds[(ppu->obj_oam_head + j) % 8] >
                    ppu->objs[i]) {
                    mask |= (1 << (7 - j));
                }
            }
        }
        mask |= ~(ppu->obj_tile_b0 | ppu->obj_tile_b1);
        mask &= obj_b0 | obj_b1;

        ppu->obj_tile_b0 &= ~mask;
        ppu->obj_tile_b1 &= ~mask;
        ppu->obj_tile_b0 |= obj_b0 & mask;
        ppu->obj_tile_b1 |= obj_b1 & mask;
        ppu->obj_tile_pal &= ~mask;
        ppu->obj_tile_bgover &= ~mask;
        ppu->obj_tile_cpal_b0 &= ~mask;
        ppu->obj_tile_cpal_b1 &= ~mask;
        ppu->obj_tile_cpal_b2 &= ~mask;
        if (obj_attr & OBJ_PAL) ppu->obj_tile_pal |= mask;
        if (obj_attr & OBJ_BGOVER) ppu->obj_tile_bgover |= mask;
        if ((obj_attr & OBJ_CPAL) & 0b001) ppu->obj_tile_cpal_b0 |= mask;
        if ((obj_attr & OBJ_CPAL) & 0b010) ppu->obj_tile_cpal_b1 |= mask;
        if ((obj_attr & OBJ_CPAL) & 0b100) ppu->obj_tile_cpal_b2 |= mask;
        for (int j = 0; j < 8; j++) {
            if (mask & (1 << (7 - j))) {
                ppu->obj_oam_inds[(ppu->obj_oam_head + j) % 8] = ppu->objs[i];
            }
        }
    }
}

Uint32 convert_cgb_color(u16 cgb_color) {
    u8 r = (cgb_color >> 0) & 0x1f;
    r = (r << 3) | (r & 0b111);
    u8 g = (cgb_color >> 5) & 0x1f;
    g = (g << 3) | (g & 0b111);
    u8 b = (cgb_color >> 10) & 0x1f;
    b = (b << 3) | (b & 0b111);
    return (r << 16) | (g << 8) | (b << 0);
}

void ppu_clock(struct gb_ppu* ppu) {
    if (!(ppu->master->io[LCDC] & LCDC_ENABLE)) {
        ppu->cycle = 0;
        ppu->scanline = 0;
        ppu->master->io[LY] = 0;
        ppu->master->io[STAT] &= ~STAT_MODE;
        return;
    }
    if (ppu->scanline < GB_SCREEN_H) {
        if (ppu->cycle < MODE2_LEN) {
            if (ppu->cycle == 0) {
                ppu->master->io[STAT] &= ~STAT_MODE;
                ppu->master->io[STAT] |= 2;

                if (ppu->scanline == 0) ppu->rendering_window = false;
                if (ppu->scanline == ppu->master->io[WY]) {
                    ppu->windowline = 0;
                    ppu->rendering_window = true;
                }

                ppu->screenX = -8;
                ppu->obj_ct = 0;
            }

            if (!ppu->master->dma_active && !(ppu->cycle & 1) &&
                ppu->obj_ct < 10) {
                u8 obj_y = ppu->master->oam[2 * ppu->cycle];
                int rel_y = ppu->scanline - obj_y + 16;
                if (rel_y >= 0 &&
                    rel_y <
                        ((ppu->master->io[LCDC] & LCDC_OBJ_SIZE) ? 16 : 8)) {
                    ppu->objs[ppu->obj_ct++] = 2 * ppu->cycle;
                }
            }
        } else if (ppu->screenX < GB_SCREEN_W) {
            if (ppu->screenX == -8) {
                ppu->master->io[STAT] &= ~STAT_MODE;
                ppu->master->io[STAT] |= 3;

                u8 curY = ppu->master->io[SCY] + ppu->scanline;
                ppu->tileY = (curY >> 3) & (TILEMAP_SIZE - 1);
                ppu->fineY = curY & 0b111;
                ppu->tileX = 0xff;
                ppu->fineX = ppu->master->io[SCX] & 0b111;

                load_bg_tile(ppu);
                ppu->bg_tile_b0 <<= ppu->fineX;
                ppu->bg_tile_b1 <<= ppu->fineX;

                ppu->obj_tile_b0 = 0;
                ppu->obj_tile_b1 = 0;
                ppu->obj_tile_pal = 0;
                ppu->obj_tile_bgover = 0;
                ppu->obj_tile_cpal_b0 = 0;
                ppu->obj_tile_cpal_b1 = 0;
                ppu->obj_tile_cpal_b2 = 0;
                memset(ppu->obj_oam_inds, 0xff, sizeof ppu->obj_oam_inds);
                ppu->obj_oam_head = 0;
            } else if (ppu->fineX == 0) {
                load_bg_tile(ppu);
            }

            u8 bg_index = 0;
            Uint32 color = 0x00ffffff;
            if (ppu->master->cgb_mode ||
                (ppu->master->io[LCDC] & LCDC_BG_ENABLE)) {
                if ((ppu->master->io[LCDC] & LCDC_WINDOW_ENABLE) &&
                    ppu->screenX == ppu->master->io[WX] - 7 &&
                    ppu->rendering_window) {
                    ppu->tileX = 0;
                    ppu->fineX = 0;
                    ppu->tileY = (ppu->windowline >> 3) & (TILEMAP_SIZE - 1);
                    ppu->fineY = ppu->windowline & 0b111;
                    ppu->windowline++;
                    load_bg_tile(ppu);
                }

                if (ppu->bg_tile_b0 & 0x80) bg_index |= 0b01;
                if (ppu->bg_tile_b1 & 0x80) bg_index |= 0b10;
                if (ppu->master->cgb_mode) {
                    u8 pal = 0;
                    if (ppu->bg_tile_cpal_b0 & 0x80) pal |= 0b001;
                    if (ppu->bg_tile_cpal_b1 & 0x80) pal |= 0b010;
                    if (ppu->bg_tile_cpal_b2 & 0x80) pal |= 0b100;
                    u16 cgb_color = 0;
                    cgb_color |= ppu->master->bg_cram[pal * 8 + bg_index * 2];
                    cgb_color |=
                        ppu->master->bg_cram[pal * 8 + bg_index * 2 + 1] << 8;
                    color = convert_cgb_color(cgb_color);
                } else {
                    color = gbemu.dmg_colors[(ppu->master->io[BGP] >>
                                              (2 * bg_index)) &
                                             0b11];
                }
            }
            if (!ppu->master->dma_active &&
                (ppu->master->io[LCDC] & LCDC_OBJ_ENABLE)) {
                load_obj_tile(ppu);

                u8 obj_index = 0;
                if (ppu->obj_tile_b0 & 0x80) obj_index |= 0b01;
                if (ppu->obj_tile_b1 & 0x80) obj_index |= 0b10;

                if (obj_index && (bg_index == 0 ||
                                  (ppu->master->cgb_mode &&
                                   !(ppu->master->io[LCDC] & LCDC_BG_ENABLE)) ||
                                  !((ppu->bg_tile_bgover & 0x80) ||
                                    (ppu->obj_tile_bgover & 0x80)))) {
                    if (ppu->master->cgb_mode) {
                        u8 pal = 0;
                        if (ppu->obj_tile_cpal_b0 & 0x80) pal |= 0b001;
                        if (ppu->obj_tile_cpal_b1 & 0x80) pal |= 0b010;
                        if (ppu->obj_tile_cpal_b2 & 0x80) pal |= 0b100;
                        u16 cgb_color = 0;
                        cgb_color |=
                            ppu->master->obj_cram[pal * 8 + obj_index * 2];
                        cgb_color |=
                            ppu->master->obj_cram[pal * 8 + obj_index * 2 + 1]
                            << 8;
                        color = convert_cgb_color(cgb_color);
                    } else {
                        if (ppu->obj_tile_pal & 0x80) {
                            color = gbemu.dmg_colors[(ppu->master->io[OBP1] >>
                                                      (2 * obj_index)) &
                                                     0b11];
                        } else {
                            color = gbemu.dmg_colors[(ppu->master->io[OBP0] >>
                                                      (2 * obj_index)) &
                                                     0b11];
                        }
                    }
                }
            }

            if (ppu->screen && ppu->screenX >= 0) {
                ppu->screen[ppu->scanline * (ppu->pitch / 4) + ppu->screenX] =
                    color;
            }

            ppu->bg_tile_b0 <<= 1;
            ppu->bg_tile_b1 <<= 1;
            ppu->bg_tile_bgover <<= 1;
            ppu->bg_tile_cpal_b0 <<= 1;
            ppu->bg_tile_cpal_b1 <<= 1;
            ppu->bg_tile_cpal_b2 <<= 1;
            ppu->fineX++;
            if (ppu->fineX == 8) {
                ppu->tileX++;
                ppu->fineX = 0;
            }
            ppu->screenX++;
            ppu->obj_tile_b0 <<= 1;
            ppu->obj_tile_b1 <<= 1;
            ppu->obj_tile_pal <<= 1;
            ppu->obj_tile_bgover <<= 1;
            ppu->obj_tile_cpal_b0 <<= 1;
            ppu->obj_tile_cpal_b1 <<= 1;
            ppu->obj_tile_cpal_b2 <<= 1;
            ppu->obj_oam_inds[ppu->obj_oam_head++] = 0xff;
            ppu->obj_oam_head &= 7;
        } else if (ppu->screenX == GB_SCREEN_W) {
            ppu->master->io[STAT] &= ~STAT_MODE;
            if (ppu->master->hdma_active && ppu->master->hdma_hblank)
                ppu->master->hdma_index = 0x10;
        }
    } else if (ppu->scanline == GB_SCREEN_H && ppu->cycle == 0) {
        ppu->master->io[IF] |= I_VBLANK;
        ppu->master->io[STAT] &= ~STAT_MODE;
        ppu->master->io[STAT] |= 1;
    }
    ppu->cycle++;
    if (ppu->cycle == CYCLES_PER_SCANLINE) {
        ppu->cycle = 0;
        ppu->scanline++;
        if (ppu->scanline == SCANLINES_PER_FRAME) {
            ppu->scanline = 0;
            ppu->frame_complete = true;
        }
        ppu->master->io[LY] = ppu->scanline;
    }
}