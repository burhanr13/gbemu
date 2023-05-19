#include "gb.h"

#include <stdio.h>
#include <stdlib.h>

#include "cartridge.h"

u8 read8(struct gb* bus, u16 addr) {
    if (addr < 0x4000) {
        return cart_read(bus->cart, addr, CART_ROM0);
    }
    if (addr < 0x8000) {
        return cart_read(bus->cart, addr & 0x3fff, CART_ROM1);
    }
    if (addr < 0xa000) {
        return bus->vram[0][addr & 0x1fff];
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
        return bus->oam[addr - 0xfe00];
    }
    if (addr < 0xff00) {
        return 0xff;
    }
    if (addr < 0xff80) {
        if (addr == 0xff0f) return bus->IF;
        return 0xff; // io registers
    }
    if (addr < 0xffff) {
        return bus->hram[addr - 0xff80];
    }
    if (addr == 0xffff) return bus->IE;
    return 0;
}

void write8(struct gb* bus, u16 addr, u8 data) {
    if (addr < 0x4000) {
        cart_write(bus->cart, addr, CART_ROM0, data);
        return;
    }
    if (addr < 0x8000) {
        cart_write(bus->cart, addr & 0x3fff, CART_ROM1, data);
        return;
    }
    if (addr < 0xa000) {
        bus->vram[0][addr & 0x1fff] = data;
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
        bus->oam[addr - 0xfe00] = data;
        return;
    }
    if (addr < 0xff00) {
        return;
    }
    if (addr < 0xff80) {
        if (addr == 0xff0f) bus->IF = data;
        if (addr == 0xff01) printf("%c", data);
        return;
    }
    if (addr < 0xffff) {
        bus->hram[addr - 0xff80] = data;
        return;
    }
    if (addr == 0xffff) bus->IE = data;
}

u16 read16(struct gb* bus, u16 addr) {
    return read8(bus, addr) | ((u16) read8(bus, addr + 1) << 8);
}

void write16(struct gb* bus, u16 addr, u16 data) {
    write8(bus, addr, data);
    write8(bus, addr + 1, data >> 8);
}
