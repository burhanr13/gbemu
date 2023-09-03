#ifndef SM83_H
#define SM83_H

#include "types.h"

enum { FZ = (1 << 7), FN = (1 << 6), FH = (1 << 5), FC = (1 << 4) };

struct gb;

struct sm83 {
    struct gb* master;

    union {
        u16 AF;
        struct {
            u8 F;
            u8 A;
        };
    };
    union {
        u16 BC;
        struct {
            u8 C;
            u8 B;
        };
    };
    union {
        u16 DE;
        struct {
            u8 E;
            u8 D;
        };
    };
    union {
        u16 HL;
        struct {
            u8 L;
            u8 H;
        };
    };
    u16 SP;
    u16 PC;
    bool IME;


    bool ei;
    bool halt;
    bool stop;
    bool ill;
};

void cpu_clock(struct sm83* cpu);

u8 cpu_read8(struct sm83* cpu, u16 addr);
void cpu_write8(struct sm83* cpu, u16 addr, u8 data);
u16 cpu_read16(struct sm83* cpu, u16 addr);
void cpu_write16(struct sm83* cpu, u16 addr, u16 data);

void print_cpu_state(struct sm83* cpu);

#endif