#include "cartridge.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "types.h"

struct cartridge* cart_create(char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;
    u8 data[3];
    fseek(fp, 0x0147, SEEK_SET);
    if (fread(data, 1, 3, fp) < 3) {
        fclose(fp);
        return NULL;
    }
    fseek(fp, 0, SEEK_SET);
    enum mbc mapper;
    if (data[0] == 0 || data[0] == 0x08 || data[0] == 0x09) mapper = MBC0;
    else if (0x01 <= data[0] && data[0] <= 0x03) mapper = MBC1;
    else if (data[0] == 0x05 || data[0] == 0x06) mapper = MBC2;
    else if (0x0b <= data[0] && data[0] <= 0x0d) mapper = MMM01;
    else if (0x0f <= data[0] && data[0] <= 0x13) mapper = MBC3;
    else if (0x19 <= data[0] && data[0] <= 0x1e) mapper = MBC5;
    else if (data[0] == 0x20) mapper = MBC6;
    else if (data[0] == 0x22) mapper = MBC7;
    else {
        fclose(fp);
        return NULL;
    }
    bool battery = false;
    switch (data[0]) {
        case 0x03:
        case 0x06:
        case 0x09:
        case 0x0d:
        case 0x0f:
        case 0x10:
        case 0x13:
        case 0x1b:
        case 0x1e:
        case 0x22:
            battery = true;
            break;
        default:
            battery = false;
            break;
    }
    int rom_banks;
    if (0 <= data[1] && data[1] <= 8) rom_banks = 2 << data[1];
    else {
        fclose(fp);
        return NULL;
    }
    int ram_banks;
    switch (data[2]) {
        case 0:
        case 1:
            ram_banks = 0;
            break;
        case 2:
            ram_banks = 1;
            break;
        case 3:
            ram_banks = 4;
            break;
        case 4:
            ram_banks = 16;
            break;
        case 5:
            ram_banks = 8;
            break;
        default:
            fclose(fp);
            return NULL;
    }
    struct cartridge* cart = calloc(1, sizeof(*cart));
    cart->mapper = mapper;
    cart->battery = battery;
    cart->rom_banks = rom_banks;
    cart->ram_banks = ram_banks;
    cart->rom = malloc(cart->rom_banks * ROM_BANK_SIZE);
    if (fread(cart->rom, ROM_BANK_SIZE, cart->rom_banks, fp) <
        cart->rom_banks) {
        free(cart->rom);
        free(cart);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    cart->cgb_compat = cart->rom[0][0x0143] & 0x80;

    if (cart->ram_banks) {
        if (cart->battery) {
            int i = strlen(filename) - 1;
            while (i >= 0 && filename[i] != '.') i--;
            char* sav_filename = malloc(i + 5);
            strncpy(sav_filename, filename, i + 1);
            sav_filename[i + 1] = 's';
            sav_filename[i + 2] = 'a';
            sav_filename[i + 3] = 'v';
            sav_filename[i + 4] = '\0';
            cart->sav_fd =
                open(sav_filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
            free(sav_filename);
            if (ftruncate(cart->sav_fd, cart->ram_banks * SRAM_BANK_SIZE)) {
                close(cart->sav_fd);
                free(cart->rom);
                free(cart);
                return NULL;
            }
            cart->ram =
                mmap(NULL, cart->ram_banks * SRAM_BANK_SIZE,
                     PROT_READ | PROT_WRITE, MAP_SHARED, cart->sav_fd, 0);
        } else {
            cart->ram = calloc(cart->ram_banks, SRAM_BANK_SIZE);
        }
    }
    return cart;
}

void cart_destroy(struct cartridge* cart) {
    if (!cart) return;
    free(cart->rom);
    if (cart->battery) {
        munmap(cart->ram, cart->ram_banks * SRAM_BANK_SIZE);
        close(cart->sav_fd);
    } else {
        free(cart->ram);
    }
    free(cart);
}

u8 cart_read(struct cartridge* cart, u16 addr, enum cart_region region) {
    if (!cart) return 0xff;
    switch (cart->mapper) {
        case MBC0:
            switch (region) {
                case CART_ROM0:
                    return cart->rom[0][addr];
                case CART_ROM1:
                    return cart->rom[1][addr];
                case CART_RAM:
                    if (cart->ram_banks) return cart->ram[0][addr];
                    else return 0xff;
            }
        case MBC1:
            switch (region) {
                case CART_ROM0:
                    if (cart->mbc1.mode == 0) {
                        return cart->rom[0][addr];
                    } else {
                        if (cart->rom_banks > 32) {
                            return cart->rom[(cart->mbc1.cur_bank_2 << 5) &
                                             (cart->rom_banks - 1)][addr];
                        } else {
                            return cart->rom[0][addr];
                        }
                    }
                case CART_ROM1:
                    return cart->rom
                        [((cart->mbc1.cur_bank_5 ? cart->mbc1.cur_bank_5 : 1) |
                          ((cart->rom_banks > 32) ? (cart->mbc1.cur_bank_2 << 5)
                                                  : 0)) &
                         (cart->rom_banks - 1)][addr];
                case CART_RAM:
                    if (cart->ram_banks && cart->mbc1.ram_enable) {
                        if (cart->mbc1.mode == 0 || cart->rom_banks > 32) {
                            return cart->ram[0][addr];
                        } else {
                            return cart->ram[cart->mbc1.cur_bank_2 &
                                             (cart->ram_banks - 1)][addr];
                        }
                    } else return 0xff;
            }
        case MBC3:
            switch (region) {
                case CART_ROM0:
                    return cart->rom[0][addr];
                case CART_ROM1:
                    return cart
                        ->rom[(cart->mbc3.cur_rom_bank ? cart->mbc3.cur_rom_bank
                                                       : 1) &
                              (cart->rom_banks - 1)][addr];
                case CART_RAM:
                    if (cart->mbc3.ram_enable) {
                        if ((cart->mbc3.cur_ram_bank & 0b1000) == 0) {
                            if (cart->ram_banks) {
                                return cart->ram[cart->mbc3.cur_ram_bank &
                                                 (cart->ram_banks - 1)][addr];
                            } else return 0xff;
                        } else {
                            return 0xff; // rtc
                        }
                    } else return 0xff;
            }
        case MBC5:
            switch (region) {
                case CART_ROM0:
                    return cart->rom[0][addr];
                case CART_ROM1:
                    return cart->rom[(cart->mbc5.cur_rom_bank) &
                                     (cart->rom_banks - 1)][addr];
                case CART_RAM:
                    if (cart->ram_banks && cart->mbc5.ram_enable) {
                        return cart->ram[cart->mbc5.cur_ram_bank &
                                         (cart->ram_banks - 1)][addr];
                    } else return 0xff;
            }
        default:
            return 0xff;
    }
}

void cart_write(struct cartridge* cart, u16 addr, enum cart_region region,
                u8 data) {
    if (!cart) return;
    switch (cart->mapper) {
        case MBC0:
            if (region == CART_RAM && cart->ram_banks)
                cart->ram[0][addr] = data;
            break;
        case MBC1:
            switch (region) {
                case CART_ROM0:
                    if (addr < 0x2000) {
                        if (data == 0x0a || data == 0x00)
                            cart->mbc1.ram_enable = data;
                    } else {
                        cart->mbc1.cur_bank_5 = data & 0b00011111;
                    }
                    break;
                case CART_ROM1:
                    if (addr < 0x2000) {
                        cart->mbc1.cur_bank_2 = data & 0b00000011;
                    } else {
                        if (data == 0x01 || data == 0x00)
                            cart->mbc1.mode = data;
                    }
                    break;
                case CART_RAM:
                    if (cart->ram_banks && cart->mbc1.ram_enable) {
                        if (cart->mbc1.mode == 0 || cart->rom_banks > 32) {
                            cart->ram[0][addr] = data;
                        } else {
                            cart->ram[cart->mbc1.cur_bank_2 &
                                      (cart->ram_banks - 1)][addr] = data;
                        }
                    }
                    break;
            }
            break;
        case MBC3:
            switch (region) {
                case CART_ROM0:
                    if (addr < 0x2000) {
                        if (data == 0x0a || data == 0x00)
                            cart->mbc3.ram_enable = data;
                    } else {
                        cart->mbc3.cur_rom_bank = data & 0b01111111;
                    }
                    break;
                case CART_ROM1:
                    if (addr < 0x2000) {
                        cart->mbc3.cur_ram_bank = data & 0b1111;
                    } else {
                        // rtc
                    }
                    break;
                case CART_RAM:
                    if (cart->mbc3.ram_enable) {
                        if ((cart->mbc3.cur_ram_bank & 0b1000) == 0) {
                            if (cart->ram_banks) {
                                cart->ram[cart->mbc3.cur_ram_bank &
                                          (cart->ram_banks - 1)][addr] = data;
                            }
                        } else {
                            // rtc
                        }
                    }
                    break;
            }
            break;
        case MBC5:
            switch (region) {
                case CART_ROM0:
                    if (addr < 0x2000) {
                        if ((data & 0x0f) == 0x0a || data == 0x00)
                            cart->mbc5.ram_enable = data;
                    } else {
                        cart->mbc5.cur_rom_bank_l = data;
                    }
                    break;
                case CART_ROM1:
                    if (addr < 0x2000) {
                        cart->mbc5.cur_rom_bank_h = data & 1;
                    } else {
                        cart->mbc5.cur_ram_bank = data;
                    }
                    break;
                case CART_RAM:
                    if (cart->ram_banks && cart->mbc5.ram_enable) {
                        cart->ram[cart->mbc5.cur_ram_bank &
                                  (cart->ram_banks - 1)][addr] = data;
                    }
                    break;
            }
            break;
        default:
            return;
    }
}