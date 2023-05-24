#include "gb.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>

#include "cartridge.h"

u8 read8(struct gb* bus, u16 addr) {
    if (bus->dma_active && addr < 0xff80) return 0xff;

    if (addr < 0x4000) {
        return cart_read(bus->cart, addr, CART_ROM0);
    }
    if (addr < 0x8000) {
        return cart_read(bus->cart, addr & 0x3fff, CART_ROM1);
    }
    if (addr < 0xa000) {
        if (!(bus->io[LCDC] & LCDC_ENABLE) ||
            (bus->io[STAT] & STAT_MODE) != 3) {
            return bus->vram[0][addr & 0x1fff];
        } else {
            return 0xff;
        }
    }
    if (addr < 0xc000) {
        return cart_read(bus->cart, addr & 0x1fff, CART_RAM);
    }
    if (addr < 0xd000) {
        return bus->wram[0][addr & 0x0fff];
    }
    if (addr < 0xe000) {
        return bus->wram[1][addr & 0x0fff];
    }
    if (addr < 0xfe00) {
        return bus->wram[0][addr & 0x0fff];
    }
    if (addr < 0xfea0) {
        if (!(bus->io[LCDC] & LCDC_ENABLE) || (bus->io[STAT] & STAT_MODE) < 2) {
            return bus->oam[addr - 0xfe00];
        } else {
            return 0xff;
        }
    }
    if (addr < 0xff00) {
        return 0xff;
    }
    if (addr < 0xff4d) {
        return bus->io[addr & 0x00ff];
    }
    if (addr < 0xff80) { // cgb registers
        return 0xff;
    }
    if (addr < 0xffff) {
        return bus->hram[addr - 0xff80];
    }
    if (addr == 0xffff) return bus->IE;
    return 0xff;
}

void write8(struct gb* bus, u16 addr, u8 data) {
    if (bus->dma_active && addr < 0xff80) return;

    if (addr < 0x4000) {
        cart_write(bus->cart, addr, CART_ROM0, data);
        return;
    }
    if (addr < 0x8000) {
        cart_write(bus->cart, addr & 0x3fff, CART_ROM1, data);
        return;
    }
    if (addr < 0xa000) {
        if (!(bus->io[LCDC] & LCDC_ENABLE) ||
            (bus->io[STAT] & STAT_MODE) != 3) {
            bus->vram[0][addr & 0x1fff] = data;
        }
        return;
    }
    if (addr < 0xc000) {
        cart_write(bus->cart, addr & 0x1fff, CART_RAM, data);
        return;
    }
    if (addr < 0xd000) {
        bus->wram[0][addr & 0x0fff] = data;
        return;
    }
    if (addr < 0xe000) {
        bus->wram[1][addr & 0x0fff] = data;
        return;
    }
    if (addr < 0xfe00) {
        bus->wram[0][addr & 0x0fff] = data;
        return;
    }
    if (addr < 0xfea0) {
        if (!(bus->io[LCDC] & LCDC_ENABLE) || (bus->io[STAT] & STAT_MODE) < 2) {
            bus->oam[addr - 0xfe00] = data;
        }
        return;
    }
    if (addr < 0xff00) {
        return;
    }
    if (addr < 0xff80) {
        switch (addr & 0x00ff) {
            case JOYP:
                bus->io[JOYP] =
                    (bus->io[JOYP] & 0b11001111) | (data & 0b00110000);
                break;
            case DIV:
                bus->io[DIV] = 0x00;
                break;
            case TIMA:
                bus->io[TMA] = data;
                break;
            case TMA:
                bus->io[TMA] = data;
                break;
            case TAC:
                bus->io[TAC] = data & 0b0111;
                break;
            case IF:
                bus->io[IF] = data & 0b00011111;
                break;
            case LCDC:
                bus->io[LCDC] = data;
                break;
            case STAT:
                bus->io[STAT] =
                    (bus->io[STAT] & 0b000111) | (data & 0b01111000);
                break;
            case SCY:
                bus->io[SCY] = data;
                break;
            case SCX:
                bus->io[SCX] = data;
                break;
            case LYC:
                bus->io[LYC] = data;
                break;
            case DMA:
                bus->io[DMA] = data;
                bus->dma_active = true;
                bus->dma_index = 0;
                break;
            case BGP:
                bus->io[BGP] = data;
                break;
            case OBP0:
                bus->io[OBP0] = data;
                break;
            case OBP1:
                bus->io[OBP1] = data;
                break;
            case WY:
                bus->io[WY] = data;
                break;
            case WX:
                bus->io[WX] = data;
                break;
        }
        return;
    }
    if (addr < 0xffff) {
        bus->hram[addr - 0xff80] = data;
        return;
    }
    if (addr == 0xffff) bus->IE = data & 0b00011111;
}

u16 read16(struct gb* bus, u16 addr) {
    return read8(bus, addr) | ((u16) read8(bus, addr + 1) << 8);
}

void write16(struct gb* bus, u16 addr, u16 data) {
    write8(bus, addr, data);
    write8(bus, addr + 1, data >> 8);
}

void clock_timers(struct gb* gb, long cycle) {
    if ((cycle & 255) == 0) {
        gb->io[DIV]++;
    }
    if (gb->io[TAC] & 0b100) {
        static int freq[] = {1023, 15, 63, 255};
        if ((cycle & freq[gb->io[TAC] & 0b011]) == 0) {
            gb->io[TIMA]++;
            if (gb->io[TIMA] == 0) {
                gb->io[TIMA] = gb->io[TMA];
                gb->io[IF] |= I_TIMER;
            }
        }
    }
}

void update_joyp(struct gb* gb) {
    u8 buttons = 0b11110000;
    if (!(gb->io[JOYP] & JP_DIR)) {
        buttons |= gb->jp_dir;
    }
    if (!(gb->io[JOYP] & JP_ACT)) {
        buttons |= gb->jp_action;
    }
    buttons = ~buttons;
    if (buttons < (gb->io[JOYP] & 0b1111)) {
        gb->io[IF] |= I_JOYPAD;
    }
    gb->io[JOYP] = (gb->io[JOYP] & 0b11110000) | buttons;
}

void run_dma(struct gb* gb) {
    if (gb->dma_cycles == 0) {
        if (gb->dma_index == OAM_SIZE) {
            gb->dma_active = false;
            return;
        }
        gb->dma_cycles += 4;
        u16 addr = gb->io[DMA] << 8 | gb->dma_index;
        u8 data;
        if (addr < 0x4000) {
            data = cart_read(gb->cart, addr & 0x3fff, CART_ROM0);
        } else if (addr < 0x8000) {
            data = cart_read(gb->cart, addr & 0x3fff, CART_ROM1);
        } else if (addr < 0xa000) {
            data = gb->vram[0][addr & 0x1fff];
        } else if (addr < 0xc000) {
            data = cart_read(gb->cart, addr, CART_RAM);
        } else if (addr < 0xd000) {
            data = gb->wram[0][addr & 0x0fff];
        } else if (addr < 0xe000) {
            data = gb->wram[1][addr & 0x0fff];
        } else {
            data = 0xff;
        }
        gb->oam[gb->dma_index] = data;
        gb->dma_index++;
    }
    gb->dma_cycles--;
}

void gb_handle_event(struct gb* gb, SDL_Event* e) {
    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.scancode) {
            case SDL_SCANCODE_UP:
                gb->jp_dir |= JP_U_SL;
                break;
            case SDL_SCANCODE_DOWN:
                gb->jp_dir |= JP_D_ST;
                break;
            case SDL_SCANCODE_LEFT:
                gb->jp_dir |= JP_L_B;
                break;
            case SDL_SCANCODE_RIGHT:
                gb->jp_dir |= JP_R_A;
                break;
            case SDL_SCANCODE_Z:
                gb->jp_action |= JP_R_A;
                break;
            case SDL_SCANCODE_X:
                gb->jp_action |= JP_L_B;
                break;
            case SDL_SCANCODE_BACKSPACE:
                gb->jp_action |= JP_U_SL;
                break;
            case SDL_SCANCODE_RETURN:
                gb->jp_action |= JP_D_ST;
                break;
            default:
                break;
        }
    }
    if (e->type == SDL_KEYUP) {
        switch (e->key.keysym.scancode) {
            case SDL_SCANCODE_UP:
                gb->jp_dir &= ~JP_U_SL;
                break;
            case SDL_SCANCODE_DOWN:
                gb->jp_dir &= ~JP_D_ST;
                break;
            case SDL_SCANCODE_LEFT:
                gb->jp_dir &= ~JP_L_B;
                break;
            case SDL_SCANCODE_RIGHT:
                gb->jp_dir &= ~JP_R_A;
                break;
            case SDL_SCANCODE_Z:
                gb->jp_action &= ~JP_R_A;
                break;
            case SDL_SCANCODE_X:
                gb->jp_action &= ~JP_L_B;
                break;
            case SDL_SCANCODE_BACKSPACE:
                gb->jp_action &= ~JP_U_SL;
                break;
            case SDL_SCANCODE_RETURN:
                gb->jp_action &= ~JP_D_ST;
                break;
            default:
                break;
        }
    }
}
