#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <time.h>

#include "types.h"

#define ROM_BANK_SIZE 0x4000  // 16k
#define SRAM_BANK_SIZE 0x2000 // 8k

enum mbc { MBC0, MBC1, MBC2, MBC3, MMM01, MBC5, MBC6, MBC7 };

enum cart_region { CART_ROM0, CART_ROM1, CART_RAM };

enum { RTC_HALT = (1 << 6), RTC_CARRY = (1 << 7) };

struct rtc_time {
    int sec;
    int min;
    int hr;
    int day;
    int dayh;
};

struct rtc {
    struct rtc_time set;
    struct rtc_time latch;
    time_t set_time;
};

struct cartridge {
    u8 title[0x10];
    enum mbc mbc;

    int rom_banks;
    int ram_banks;

    u8 (*rom)[ROM_BANK_SIZE];
    u8 (*ram)[SRAM_BANK_SIZE];

    bool battery;
    int sav_fd;
    bool has_rtc;
    struct rtc* rtc;
    size_t sav_size;

    char* rom_filename;
    char* sav_filename;
    char* sst_filename;

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
            bool latching;
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
    } st;
};

struct cartridge* cart_create(char* filename);
void cart_destroy(struct cartridge* cart);

u8 cart_read(struct cartridge* cart, u16 addr, enum cart_region region);
void cart_write(struct cartridge* cart, u16 addr, enum cart_region region,
                u8 data);

#endif