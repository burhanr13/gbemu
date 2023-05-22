#include "ppu.h"

#include "gb.h"

void init_ppu(struct gb* master, struct gb_ppu* ppu) {
    ppu->master = master;
    ppu->cycle = 0;
    ppu->scanline = 0;
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
        if(ppu->cycle == 0){
            ppu->master->io[STAT] &= ~STAT_MODE;
            ppu->master->io[STAT] |= 2;
            if (ppu->master->io[STAT] & STAT_I_OAM)
                ppu->master->io[IF] |= I_STAT;
            // search OAM for sprites
        }
    } else if (ppu->scanline == GB_SCREEN_H && ppu->cycle == 0) {
        ppu->master->io[IF] |= I_VBLANK;
        ppu->master->io[STAT] &= ~STAT_MODE;
        if(ppu->master->io[STAT] & STAT_I_VBLANK)
            ppu->master->io[IF] |= I_STAT;
    }
    ppu->cycle++;
    if(ppu->cycle == CYCLES_PER_SCANLINE){
        ppu->cycle = 0;
        ppu->scanline++;
        if(ppu->scanline == SCANLINES_PER_FRAME) {
            ppu->scanline = 0;
        }
        ppu->master->io[LY] = ppu->scanline;
    }
}