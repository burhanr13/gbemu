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
    enum mbc mbc;
    if (data[0] == 0 || data[0] == 0x08 || data[0] == 0x09) mbc = MBC0;
    else if (0x01 <= data[0] && data[0] <= 0x03) mbc = MBC1;
    else if (data[0] == 0x05 || data[0] == 0x06) mbc = MBC2;
    else if (0x0b <= data[0] && data[0] <= 0x0d) mbc = MMM01;
    else if (0x0f <= data[0] && data[0] <= 0x13) mbc = MBC3;
    else if (0x19 <= data[0] && data[0] <= 0x1e) mbc = MBC5;
    else if (data[0] == 0x20) mbc = MBC6;
    else if (data[0] == 0x22) mbc = MBC7;
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
    bool has_rtc = data[0] == 0x0f || data[0] == 0x10;
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

    int filename_len = strlen(filename);
    cart->rom_filename = malloc(filename_len + 1);
    strcpy(cart->rom_filename, filename);
    int i = filename_len;
    while (i >= 0 && filename[i] != '.') i--;
    cart->sav_filename = malloc(i + 5);
    cart->sst_filename = malloc(i + 5);
    strncpy(cart->sav_filename, filename, i + 1);
    strncpy(cart->sst_filename, filename, i + 1);
    strcpy(cart->sav_filename + i, ".sav");
    strcpy(cart->sst_filename + i, ".sst");

    cart->mbc = mbc;
    cart->battery = battery;
    cart->has_rtc = has_rtc;
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

    cart->sav_size = cart->ram_banks * SRAM_BANK_SIZE +
                     (cart->has_rtc ? sizeof(struct rtc) : 0);

    cart->cgb_compat = cart->rom[0][0x0143] & 0x80;
    memcpy(cart->title, &cart->rom[0x0134], sizeof cart->title);

    if (cart->battery) {
        cart->sav_fd =
            open(cart->sav_filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (ftruncate(cart->sav_fd, cart->sav_size)) {
            cart_destroy(cart);
            return NULL;
        }
        cart->ram = mmap(NULL, cart->sav_size, PROT_READ | PROT_WRITE,
                         MAP_SHARED, cart->sav_fd, 0);
        if (cart->has_rtc) cart->rtc = (struct rtc*) cart->ram[cart->ram_banks];
    } else if (cart->ram_banks) {
        cart->ram = calloc(cart->ram_banks, SRAM_BANK_SIZE);
    }
    return cart;
}

void cart_destroy(struct cartridge* cart) {
    if (!cart) return;
    free(cart->rom);
    if (cart->battery) {
        munmap(cart->ram, cart->sav_size);
        close(cart->sav_fd);
    } else {
        free(cart->ram);
    }
    free(cart->rom_filename);
    free(cart->sav_filename);
    free(cart->sst_filename);
    free(cart);
}

u8 cart_read(struct cartridge* cart, u16 addr, enum cart_region region) {
    if (!cart) return 0xff;
    switch (cart->mbc) {
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
                    if (cart->st.mbc1.mode == 0) {
                        return cart->rom[0][addr];
                    } else {
                        if (cart->rom_banks > 32) {
                            return cart->rom[(cart->st.mbc1.cur_bank_2 << 5) &
                                             (cart->rom_banks - 1)][addr];
                        } else {
                            return cart->rom[0][addr];
                        }
                    }
                case CART_ROM1:
                    return cart->rom[((cart->st.mbc1.cur_bank_5
                                           ? cart->st.mbc1.cur_bank_5
                                           : 1) |
                                      ((cart->rom_banks > 32)
                                           ? (cart->st.mbc1.cur_bank_2 << 5)
                                           : 0)) &
                                     (cart->rom_banks - 1)][addr];
                case CART_RAM:
                    if (cart->ram_banks && cart->st.mbc1.ram_enable) {
                        if (cart->st.mbc1.mode == 0 || cart->rom_banks > 32) {
                            return cart->ram[0][addr];
                        } else {
                            return cart->ram[cart->st.mbc1.cur_bank_2 &
                                             (cart->ram_banks - 1)][addr];
                        }
                    } else return 0xff;
            }
        case MBC3:
            switch (region) {
                case CART_ROM0:
                    return cart->rom[0][addr];
                case CART_ROM1:
                    return cart->rom[(cart->st.mbc3.cur_rom_bank
                                          ? cart->st.mbc3.cur_rom_bank
                                          : 1) &
                                     (cart->rom_banks - 1)][addr];
                case CART_RAM:
                    if (cart->st.mbc3.ram_enable) {
                        if (!cart->has_rtc ||
                            (cart->st.mbc3.cur_ram_bank & 0b1000) == 0) {
                            if (cart->ram_banks) {
                                return cart->ram[cart->st.mbc3.cur_ram_bank &
                                                 (cart->ram_banks - 1)][addr];
                            } else return 0xff;
                        } else {
                            switch (cart->st.mbc3.cur_ram_bank & 0b0111) {
                                case 0:
                                    return cart->rtc->latch.sec;
                                case 1:
                                    return cart->rtc->latch.min;
                                case 2:
                                    return cart->rtc->latch.hr;
                                case 3:
                                    return cart->rtc->latch.day;
                                case 4:
                                    return cart->rtc->latch.dayh;
                                default:
                                    return 0xff;
                            }
                        }
                    } else return 0xff;
            }
        case MBC5:
            switch (region) {
                case CART_ROM0:
                    return cart->rom[0][addr];
                case CART_ROM1:
                    return cart->rom[(cart->st.mbc5.cur_rom_bank) &
                                     (cart->rom_banks - 1)][addr];
                case CART_RAM:
                    if (cart->ram_banks && cart->st.mbc5.ram_enable) {
                        return cart->ram[cart->st.mbc5.cur_ram_bank &
                                         (cart->ram_banks - 1)][addr];
                    } else return 0xff;
            }
        default:
            return 0xff;
    }
}

void rtc_update(struct rtc* rtc) {
    if (rtc->set.dayh & RTC_HALT) return;
    time_t cur_time = time(NULL);
    time_t t = cur_time - rtc->set_time;
    rtc->set_time = cur_time;
    int days = ((rtc->set.dayh & 1) << 8) + rtc->set.day;
    t += ((days * 24 + rtc->set.hr) * 60 + rtc->set.min) * 60 + rtc->set.sec;
    rtc->set.sec = t % 60;
    t /= 60;
    rtc->set.min = t % 60;
    t /= 60;
    rtc->set.hr = t % 24;
    t /= 24;
    rtc->set.day = t & 0xff;
    bool old_carry = rtc->set.dayh & RTC_CARRY;
    rtc->set.dayh = (t & 0x100) >> 8;
    if (t > 511 || old_carry) rtc->set.dayh &= RTC_CARRY;
}

void cart_write(struct cartridge* cart, u16 addr, enum cart_region region,
                u8 data) {
    if (!cart) return;
    switch (cart->mbc) {
        case MBC0:
            if (region == CART_RAM && cart->ram_banks)
                cart->ram[0][addr] = data;
            break;
        case MBC1:
            switch (region) {
                case CART_ROM0:
                    if (addr < 0x2000) {
                        if (data == 0x0a || data == 0x00)
                            cart->st.mbc1.ram_enable = data;
                    } else {
                        cart->st.mbc1.cur_bank_5 = data & 0b00011111;
                    }
                    break;
                case CART_ROM1:
                    if (addr < 0x2000) {
                        cart->st.mbc1.cur_bank_2 = data & 0b00000011;
                    } else {
                        if (data == 0x01 || data == 0x00)
                            cart->st.mbc1.mode = data;
                    }
                    break;
                case CART_RAM:
                    if (cart->ram_banks && cart->st.mbc1.ram_enable) {
                        if (cart->st.mbc1.mode == 0 || cart->rom_banks > 32) {
                            cart->ram[0][addr] = data;
                        } else {
                            cart->ram[cart->st.mbc1.cur_bank_2 &
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
                            cart->st.mbc3.ram_enable = data;
                    } else {
                        cart->st.mbc3.cur_rom_bank = data & 0b01111111;
                    }
                    break;
                case CART_ROM1:
                    if (addr < 0x2000) {
                        cart->st.mbc3.cur_ram_bank = data & 0b1111;
                    } else if (cart->has_rtc) {
                        if (data == 0) cart->st.mbc3.latching = true;
                        if (data == 1 && cart->st.mbc3.latching) {
                            cart->st.mbc3.latching = false;
                            rtc_update(cart->rtc);
                            memcpy(&cart->rtc->latch, &cart->rtc->set,
                                   sizeof(struct rtc_time));
                        }
                    }
                    break;
                case CART_RAM:
                    if (cart->st.mbc3.ram_enable) {
                        if (!cart->has_rtc ||
                            (cart->st.mbc3.cur_ram_bank & 0b1000) == 0) {
                            if (cart->ram_banks) {
                                cart->ram[cart->st.mbc3.cur_ram_bank &
                                          (cart->ram_banks - 1)][addr] = data;
                            }
                        } else {
                            if (cart->rtc->set.dayh & RTC_HALT) {
                                switch (cart->st.mbc3.cur_ram_bank & 0b0111) {
                                    case 0:
                                        cart->rtc->set.sec = data;
                                        break;
                                    case 1:
                                        cart->rtc->set.min = data;
                                        break;
                                    case 2:
                                        cart->rtc->set.hr = data;
                                        break;
                                    case 3:
                                        cart->rtc->set.day = data;
                                        break;
                                    case 4:
                                        cart->rtc->set.dayh = data & 0x11000001;
                                        if (!(data & RTC_HALT))
                                            cart->rtc->set_time = time(NULL);
                                        break;
                                }
                            } else if (cart->st.mbc3.cur_ram_bank == 0xc &&
                                       data & RTC_HALT) {
                                rtc_update(cart->rtc);
                                cart->rtc->set.dayh &= RTC_HALT;
                            }
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
                            cart->st.mbc5.ram_enable = data;
                    } else {
                        cart->st.mbc5.cur_rom_bank_l = data;
                    }
                    break;
                case CART_ROM1:
                    if (addr < 0x2000) {
                        cart->st.mbc5.cur_rom_bank_h = data & 1;
                    } else {
                        cart->st.mbc5.cur_ram_bank = data;
                    }
                    break;
                case CART_RAM:
                    if (cart->ram_banks && cart->st.mbc5.ram_enable) {
                        cart->ram[cart->st.mbc5.cur_ram_bank &
                                  (cart->ram_banks - 1)][addr] = data;
                    }
                    break;
            }
            break;
        default:
            return;
    }
}