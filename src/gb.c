#include "gb.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>

#include "cartridge.h"
#include "emulator.h"

u8 read8(struct gb* bus, u16 addr) {
    bus->cpu.cycles += 4;
    if (bus->dma_active && addr < 0xff00) return 0xff;

    if (addr < 0x4000) {
        return cart_read(bus->cart, addr, CART_ROM0);
    }
    if (addr < 0x8000) {
        return cart_read(bus->cart, addr & 0x3fff, CART_ROM1);
    }
    if (addr < 0xa000) {
        if (!(bus->io[LCDC] & LCDC_ENABLE) ||
            (bus->io[STAT] & STAT_MODE) != 3) {
            return bus->vram[bus->io[VBK] & 1][addr & 0x1fff];
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
        u8 bank = bus->io[SVBK];
        return bus->wram[bank ? bank : 1][addr & 0x0fff];
    }
    if (addr < 0xf000) {
        return bus->wram[0][addr & 0x0fff];
    }
    if (addr < 0xfe00) {
        u8 bank = bus->io[SVBK];
        return bus->wram[bank ? bank : 1][addr & 0x0fff];
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
        if ((addr & 0x00ff) == DIV) return bus->div >> 8;
        if ((addr & 0x00f0) == WAVERAM) {
            if (bus->apu.ch3_enable) return 0xff;
            else return (bus->io + WAVERAM)[addr & 0x000f];
        }
        return bus->io[addr & 0x00ff];
    }
    if (addr < 0xff80) { // cgb registers
        if (bus->cgb_mode) {
            if ((addr & 0x00ff) == BCPD) {
                if (!(bus->io[LCDC] & LCDC_ENABLE) ||
                    (bus->io[STAT] & STAT_MODE) != 3) {
                    return bus->bg_cram[bus->io[BCPS] & CPS_ADDR];
                } else return 0xff;
            }
            if ((addr & 0x00ff) == OCPD) {
                if (!(bus->io[LCDC] & LCDC_ENABLE) ||
                    (bus->io[STAT] & STAT_MODE) != 3) {
                    return bus->obj_cram[bus->io[OCPS] & CPS_ADDR];
                } else return 0xff;
            }
            return bus->io[addr & 0x00ff];
        } else return 0xff;
    }
    if (addr < 0xffff) {
        return bus->hram[addr - 0xff80];
    }
    if (addr == 0xffff) return bus->IE;
    return 0xff;
}

void write8(struct gb* bus, u16 addr, u8 data) {
    bus->cpu.cycles += 4;
    if (bus->dma_active && addr < 0xff00) return;

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
            bus->vram[bus->io[VBK] & 1][addr & 0x1fff] = data;
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
        u8 bank = bus->io[SVBK];
        bus->wram[bank ? bank : 1][addr & 0x0fff] = data;
        return;
    }
    if (addr < 0xf000) {
        bus->wram[0][addr & 0x0fff] = data;
        return;
    }
    if (addr < 0xfe00) {
        u8 bank = bus->io[SVBK];
        bus->wram[bank ? bank : 1][addr & 0x0fff] = data;
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
        if (!bus->apu.ch3_enable && (addr & 0x00f0) == WAVERAM) {
            (bus->io + WAVERAM)[addr & 0x000f] = data;
            return;
        }
        switch (addr & 0x00ff) {
            case JOYP:
                bus->io[JOYP] =
                    (bus->io[JOYP] & 0b11001111) | (data & 0b00110000);
                break;
            case DIV:
                bus->div = 0x0000;
                break;
            case TIMA:
                bus->io[TIMA] = data;
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
            case NR10:
                if ((data & NR10_PACE) == 0) bus->apu.ch1_sweep_pace = 0;
                bus->io[NR10] = data;
                break;
            case NR11:
                bus->apu.ch1_len_counter = data & NRX1_LEN;
                bus->io[NR11] = data & NRX1_DUTY;
                break;
            case NR12:
                if (!(data & 0b11111000)) bus->apu.ch1_enable = false;
                bus->io[NR12] = data;
                break;
            case NR13:
                bus->apu.ch1_wavelen = (bus->apu.ch1_wavelen & 0xff00) | data;
                break;
            case NR14:
                bus->apu.ch1_wavelen = (bus->apu.ch1_wavelen & 0x00ff) |
                                       ((data & NRX4_WVLEN_HI) << 8);
                if ((bus->io[NR12] & 0b11111000) && (data & NRX4_TRIGGER)) {
                    bus->apu.ch1_enable = true;
                    bus->apu.ch1_counter = bus->apu.ch1_wavelen;
                    bus->apu.ch1_duty_index = 0;
                    bus->apu.ch1_env_counter = 0;
                    bus->apu.ch1_env_pace = bus->io[NR12] & NRX2_PACE;
                    bus->apu.ch1_env_dir = bus->io[NR12] & NRX2_DIR;
                    bus->apu.ch1_volume = (bus->io[NR12] & NRX2_VOL) >> 4;
                    bus->apu.ch1_sweep_pace = (bus->io[NR10] & NR10_PACE) >> 4;
                    bus->apu.ch1_sweep_counter = 0;
                }
                bus->io[NR14] = data & NRX4_LEN_ENABLE;
                break;
            case NR21:
                bus->apu.ch2_len_counter = data & NRX1_LEN;
                bus->io[NR21] = data & NRX1_DUTY;
                break;
            case NR22:
                if (!(data & 0b11111000)) bus->apu.ch2_enable = false;
                bus->io[NR22] = data;
                break;
            case NR23:
                bus->apu.ch2_wavelen = (bus->apu.ch2_wavelen & 0xff00) | data;
                break;
            case NR24:
                bus->apu.ch2_wavelen = (bus->apu.ch2_wavelen & 0x00ff) |
                                       ((data & NRX4_WVLEN_HI) << 8);
                if ((bus->io[NR22] & 0b11111000) && (data & NRX4_TRIGGER)) {
                    bus->apu.ch2_enable = true;
                    bus->apu.ch2_counter = bus->apu.ch2_wavelen;
                    bus->apu.ch2_duty_index = 0;
                    bus->apu.ch2_env_counter = 0;
                    bus->apu.ch2_env_pace = bus->io[NR22] & NRX2_PACE;
                    bus->apu.ch2_env_dir = bus->io[NR22] & NRX2_DIR;
                    bus->apu.ch2_volume = (bus->io[NR22] & NRX2_VOL) >> 4;
                }
                bus->io[NR24] = data & NRX4_LEN_ENABLE;
                break;
            case NR30:
                if (!(data & 0b10000000)) bus->apu.ch3_enable = false;
                bus->io[NR30] = data & 0b10000000;
                break;
            case NR31:
                bus->apu.ch3_len_counter = data;
                break;
            case NR32:
                bus->io[NR32] = data & 0b01100000;
                break;
            case NR33:
                bus->apu.ch3_wavelen = (bus->apu.ch3_wavelen & 0xff00) | data;
                break;
            case NR34:
                bus->apu.ch3_wavelen = (bus->apu.ch3_wavelen & 0x00ff) |
                                       ((data & NRX4_WVLEN_HI) << 8);
                if ((bus->io[NR30] & 0b10000000) && (data & NRX4_TRIGGER)) {
                    bus->apu.ch3_enable = true;
                    bus->apu.ch3_counter = bus->apu.ch3_wavelen;
                    bus->apu.ch3_sample_index = 0;
                }
                bus->io[NR34] = data & NRX4_LEN_ENABLE;
                break;
            case NR41:
                bus->apu.ch4_len_counter = data & NRX1_LEN;
                break;
            case NR42:
                if (!(data & 0b11111000)) bus->apu.ch4_enable = false;
                bus->io[NR42] = data;
                break;
            case NR43:
                bus->io[NR43] = data;
                break;
            case NR44:
                if ((bus->io[NR42] & 0b11111000) && (data & NRX4_TRIGGER)) {
                    bus->apu.ch4_enable = true;
                    bus->apu.ch4_counter = 0;
                    bus->apu.ch4_lfsr = 0;
                    bus->apu.ch4_env_counter = 0;
                    bus->apu.ch4_env_pace = bus->io[NR42] & NRX2_PACE;
                    bus->apu.ch4_env_dir = bus->io[NR42] & NRX2_DIR;
                    bus->apu.ch4_volume = (bus->io[NR42] & NRX2_VOL) >> 4;
                }
                bus->io[NR44] = data & NRX4_LEN_ENABLE;
                break;
            case NR50:
                bus->io[NR50] = data;
                break;
            case NR51:
                bus->io[NR51] = data;
                break;
            case NR52:
                bus->io[NR52] = data & 0b10000000;
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
            default:
                if (bus->cgb_mode) {
                    switch (addr & 0x00ff) {
                        case KEY1:
                            bus->io[KEY1] = (bus->io[KEY1] & ~1) | (data & 1);
                            break;
                        case VBK:
                            bus->io[VBK] = ~1 | (data & 1);
                            break;
                        case HDMA1:
                            bus->hdma_src =
                                (bus->hdma_src & 0x00ff) | (data << 8);
                            break;
                        case HDMA2:
                            bus->hdma_src =
                                (bus->hdma_src & 0xff00) | (data & 0xf0);
                            break;
                        case HDMA3:
                            bus->hdma_dest = (bus->hdma_dest & 0x00ff) |
                                             ((data & 0x1f) << 8);
                            break;
                        case HDMA4:
                            bus->hdma_dest =
                                (bus->hdma_dest & 0xff00) | (data & 0xf0);
                            break;
                        case HDMA5:
                            if (!bus->hdma_active) {
                                bus->hdma_active = true;
                                bus->hdma_hblank = data & (1 << 7);
                                bus->io[HDMA5] = data & ~(1 << 7);
                                bus->hdma_block = 0;
                                if (bus->hdma_hblank) {
                                    bus->hdma_index = 0;
                                } else {
                                    bus->hdma_index = 0x10;
                                }
                            } else if (!(data & (1 << 7))) {
                                bus->hdma_active = false;
                                bus->io[HDMA5] &= 1 << 7;
                            }
                            break;
                        case BCPS:
                            bus->io[BCPS] = data;
                            break;
                        case BCPD:
                            if (!(bus->io[LCDC] & LCDC_ENABLE) ||
                                (bus->io[STAT] & STAT_MODE) != 3) {
                                bus->bg_cram[bus->io[BCPS] & CPS_ADDR] = data;
                            }

                            if (bus->io[BCPS] & CPS_INC) {
                                bus->io[BCPS]++;
                                bus->io[BCPS] &= ~(1 << 6);
                            }
                            break;
                        case OCPS:
                            bus->io[OCPS] = data;
                            break;
                        case OCPD:
                            if (!(bus->io[LCDC] & LCDC_ENABLE) ||
                                (bus->io[STAT] & STAT_MODE) != 3) {
                                bus->obj_cram[bus->io[OCPS] & CPS_ADDR] = data;
                            }
                            if (bus->io[OCPS] & CPS_INC) {
                                bus->io[OCPS]++;
                                bus->io[OCPS] &= ~(1 << 6);
                            }
                            break;
                        case OPRI:
                            bus->io[OPRI] = data & 1;
                        case SVBK:
                            bus->io[SVBK] = data & 0b111;
                            break;
                    }
                }
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

void tick_gb(struct gb* gb) {
    check_stat_irq(gb);
    clock_timers(gb);
    update_joyp(gb);
    if (gb->dma_active) run_dma(gb);
    if (!(gb->io[KEY1] & (1 << 7)) || gb->div % 2 == 0) {
        ppu_clock(&gb->ppu);
        if (gb->hdma_active) run_hdma(gb);
        apu_clock(&gb->apu);
    }
    cpu_clock(&gb->cpu);
}

void check_stat_irq(struct gb* gb) {
    if (gb->io[LYC] == gb->io[LY]) {
        gb->io[STAT] |= STAT_LYCEQ;
    } else {
        gb->io[STAT] &= ~STAT_LYCEQ;
    }
    bool new_stat_int =
        ((gb->io[STAT] & STAT_LYCEQ) && (gb->io[STAT] & STAT_I_LYCEQ)) ||
        ((gb->io[STAT] & STAT_MODE) == 0 && (gb->io[STAT] & STAT_I_HBLANK)) ||
        ((gb->io[STAT] & STAT_MODE) == 1 && (gb->io[STAT] & STAT_I_VBLANK)) ||
        ((gb->io[STAT] & STAT_MODE) == 2 && (gb->io[STAT] & STAT_I_OAM));
    if (new_stat_int && !gb->prev_stat_int) gb->io[IF] |= I_STAT;
    gb->prev_stat_int = new_stat_int;
}

void clock_timers(struct gb* gb) {
    gb->div++;
    if (gb->timer_overflow) {
        gb->io[IF] |= I_TIMER;
        gb->io[TIMA] = gb->io[TMA];
        gb->timer_overflow = false;
    }
    static const int freq[] = {1024, 16, 64, 256};
    bool new_timer_inc =
        (gb->io[TAC] & 0b100) && (gb->div & (freq[gb->io[TAC] & 0b011]) / 2);
    if (!new_timer_inc && gb->prev_timer_inc) {
        gb->io[TIMA]++;
        if (gb->io[TIMA] == 0) {
            gb->timer_overflow = true;
        }
    }
    gb->prev_timer_inc = new_timer_inc;
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
            data = gb->vram[gb->io[VBK] & 1][addr & 0x1fff];
        } else if (addr < 0xc000) {
            data = cart_read(gb->cart, addr & 0x1fff, CART_RAM);
        } else if (addr < 0xd000) {
            data = gb->wram[0][addr & 0x0fff];
        } else if (addr < 0xe000) {
            u8 bank = gb->io[SVBK];
            data = gb->wram[bank ? bank : 1][addr & 0x0fff];
        } else {
            data = 0xff;
        }
        gb->oam[gb->dma_index++] = data;
    }
    gb->dma_cycles--;
}

void run_hdma(struct gb* gb) {
    if (gb->hdma_cycles == 0) {
        if (gb->io[HDMA5] == 0xff) {
            gb->hdma_active = false;
            gb->hdma_index = 0;
            return;
        }
        if (gb->hdma_index > 0) {
            gb->hdma_cycles += 2;
            u16 addr =
                gb->hdma_src + 0x10 * gb->hdma_block + (0x10 - gb->hdma_index);
            u8 data;
            if (addr < 0x4000) {
                data = cart_read(gb->cart, addr & 0x3fff, CART_ROM0);
            } else if (addr < 0x8000) {
                data = cart_read(gb->cart, addr & 0x3fff, CART_ROM1);
            } else if (addr < 0xa000) {
                data = 0xff;
            } else if (addr < 0xc000) {
                data = cart_read(gb->cart, addr & 0x1fff, CART_RAM);
            } else if (addr < 0xd000) {
                data = gb->wram[0][addr & 0x0fff];
            } else if (addr < 0xe000) {
                u8 bank = gb->io[SVBK];
                data = gb->wram[bank ? bank : 1][addr & 0x0fff];
            } else {
                data = 0xff;
            }

            u16 dest =
                gb->hdma_dest + 0x10 * gb->hdma_block + (0x10 - gb->hdma_index);
            if ((gb->io[STAT] & STAT_MODE) != 3) {
                gb->vram[gb->io[VBK] & 1][dest & 0x1fff] = data;
            }

            gb->hdma_index--;

            if (gb->hdma_index == 0) {
                gb->hdma_block++;
                gb->io[HDMA5]--;
                if (!gb->hdma_hblank) gb->hdma_index = 0x10;
            }
        }
    }
    if (gb->hdma_cycles != 0) gb->hdma_cycles--;
}

void reset_gb(struct gb* gb, struct cartridge* cart) {
    memset(gb, 0x00, sizeof *gb);
    gb->cpu.master = gb;
    gb->ppu.master = gb;
    gb->apu.master = gb;

    gb->cart = cart;
    if (gb->cart && gb->cart->cgb_compat && !gbemu.force_dmg) {
        gb->cpu.A = 0x11;
        gb->cgb_mode = true;
    } else {
        gb->cpu.A = 0x01;
        gb->io[OPRI] = 1;
    }

    gb->cpu.SP = 0xfffe;
    gb->cpu.PC = 0x0100;

    gb->io[LCDC] |= LCDC_ENABLE;
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
    if (e->type == SDL_CONTROLLERBUTTONDOWN) {
        switch (e->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                gb->jp_dir |= JP_U_SL;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                gb->jp_dir |= JP_D_ST;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                gb->jp_dir |= JP_L_B;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                gb->jp_dir |= JP_R_A;
                break;
            case SDL_CONTROLLER_BUTTON_A:
                gb->jp_action |= JP_R_A;
                break;
            case SDL_CONTROLLER_BUTTON_X:
                gb->jp_action |= JP_L_B;
                break;
            case SDL_CONTROLLER_BUTTON_BACK:
                gb->jp_action |= JP_U_SL;
                break;
            case SDL_CONTROLLER_BUTTON_START:
                gb->jp_action |= JP_D_ST;
                break;
            default:
                break;
        }
    }
    if (e->type == SDL_CONTROLLERBUTTONUP) {
        switch (e->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                gb->jp_dir &= ~JP_U_SL;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                gb->jp_dir &= ~JP_D_ST;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                gb->jp_dir &= ~JP_L_B;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                gb->jp_dir &= ~JP_R_A;
                break;
            case SDL_CONTROLLER_BUTTON_A:
                gb->jp_action &= ~JP_R_A;
                break;
            case SDL_CONTROLLER_BUTTON_X:
                gb->jp_action &= ~JP_L_B;
                break;
            case SDL_CONTROLLER_BUTTON_BACK:
                gb->jp_action &= ~JP_U_SL;
                break;
            case SDL_CONTROLLER_BUTTON_START:
                gb->jp_action &= ~JP_D_ST;
                break;
            default:
                break;
        }
    }
}
