#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "types.h"

#define ROM_BANK_SIZE 0x4000  // 16k
#define SRAM_BANK_SIZE 0x2000 // 8k

enum mbc { MBC0, MBC1, MBC2, MBC3, MMM01, MBC5, MBC6, MBC7 };

enum cart_region { CART_ROM0, CART_ROM1, CART_RAM };

struct cartridge {
    enum mbc mapper;

    int rom_banks;
    int ram_banks;

    u8 (*rom)[ROM_BANK_SIZE];
    u8 (*ram)[SRAM_BANK_SIZE];

    bool battery;
    int sav_fd;

    bool cgb_compat;

    union {
        struct {
            u8 ram_enable;
            u8 cur_bank_5;
            u8 cur_bank_2;
            u8 mode;
        } mbc1;
        struct {
            u8 ram_enable;
            u8 cur_rom_bank;
            u8 cur_ram_bank;
        } mbc3;
        struct {
            u8 ram_enable;
            union {
                u16 cur_rom_bank;
                struct {
                    u8 cur_rom_bank_l;
                    u8 cur_rom_bank_h;
                };
            };
            u8 cur_ram_bank;
        } mbc5;
    };
};

struct cartridge* cart_create(char* filename);
void cart_destroy(struct cartridge* cart);

u8 cart_read(struct cartridge* cart, u16 addr, enum cart_region region);
void cart_write(struct cartridge* cart, u16 addr, enum cart_region region,
                u8 data);

#endif