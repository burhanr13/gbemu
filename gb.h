#ifndef GB_H
#define GB_H

#include "sm83.h"
#include "types.h"

struct gb {
    struct sm83;
};

u8 read8(struct gb* bus, u16 addr);
u16 read16(struct gb* bus, u16 addr);
void write8(struct gb* bus, u16 addr, u8 data);
void write16(struct gb* bus, u16 addr, u16 data);

#endif