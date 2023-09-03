#include "sm83.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gb.h"

static void set_flag(struct sm83* cpu, int flag, int val) {
    if (val) {
        cpu->F |= flag;
    } else {
        cpu->F &= ~flag;
    }
}

static void resolve_flags(struct sm83* cpu, int flags, u8 pre, u8 post,
                          int carry) {
    set_flag(cpu, FN, flags & FN);
    if (flags & FZ) {
        set_flag(cpu, FZ, post == 0);
    }
    if (flags & FH) {
        if (flags & FN)
            set_flag(cpu, FH,
                     (pre & 0x0f) < (post & 0x0f) ||
                         ((pre & 0x0f) == (post & 0x0f) && carry));
        else
            set_flag(cpu, FH,
                     (pre & 0x0f) > (post & 0x0f) ||
                         ((pre & 0x0f) == (post & 0x0f) && carry));
    }
    if (flags & FC) {
        if (flags & FN) set_flag(cpu, FC, pre < post || (pre == post && carry));
        else set_flag(cpu, FC, pre > post || (pre == post && carry));
    }
}

static int eval_cond(struct sm83* cpu, u8 opcode) {
    switch ((opcode & 0b00011000) >> 3) {
        case 0: // NZ
            return !(cpu->F & FZ);
        case 1: // Z
            return cpu->F & FZ;
        case 2: // NC
            return !(cpu->F & FC);
        case 3: // C
            return cpu->F & FC;
    }
    return 0;
}

static u16* getr16mod(struct sm83* cpu, u8 opcode) {
    switch ((opcode & 0b00110000) >> 4) {
        case 0:
            return &cpu->BC;
        case 1:
            return &cpu->DE;
        case 2:
            return &cpu->HL;
        case 3:
            return &cpu->SP;
    }
    return NULL;
}

static u16* getr16stack(struct sm83* cpu, u8 opcode) {
    switch ((opcode & 0b00110000) >> 4) {
        case 0:
            return &cpu->BC;
        case 1:
            return &cpu->DE;
        case 2:
            return &cpu->HL;
        case 3:
            return &cpu->AF;
    }
    return NULL;
}

static u16 getr16addr(struct sm83* cpu, u8 opcode) {
    switch ((opcode & 0b00110000) >> 4) {
        case 0:
            return cpu->BC;
        case 1:
            return cpu->DE;
        case 2:
            return cpu->HL++;
        case 3:
            return cpu->HL--;
    }
    return 0;
}

static u8 getr8src(struct sm83* cpu, u8 opcode) {
    switch (opcode & 0b00000111) {
        case 0:
            return cpu->B;
        case 1:
            return cpu->C;
        case 2:
            return cpu->D;
        case 3:
            return cpu->E;
        case 4:
            return cpu->H;
        case 5:
            return cpu->L;
        case 6:
            return cpu_read8(cpu, cpu->HL);
        case 7:
            return cpu->A;
    }
    return 0;
}

static u8* getr8dest(struct sm83* cpu, u8 opcode) {
    switch ((opcode & 0b00111000) >> 3) {
        case 0:
            return &cpu->B;
        case 1:
            return &cpu->C;
        case 2:
            return &cpu->D;
        case 3:
            return &cpu->E;
        case 4:
            return &cpu->H;
        case 5:
            return &cpu->L;
        case 6:
            return NULL;
        case 7:
            return &cpu->A;
    }
    return NULL;
}

static void run_alu(struct sm83* cpu, u8 opcode, u8 op2) {
    u8 pre = cpu->A;
    switch ((opcode & 0b00111000) >> 3) {
        case 0: // ADD
            cpu->A += op2;
            resolve_flags(cpu, FZ | FH | FC, pre, cpu->A, 0);
            break;
        case 1: // ADC
            cpu->A += op2 + (cpu->F & FC ? 1 : 0);
            resolve_flags(cpu, FZ | FH | FC, pre, cpu->A, cpu->F & FC);
            break;
        case 2: // SUB
            cpu->A -= op2;
            resolve_flags(cpu, FZ | FN | FH | FC, pre, cpu->A, 0);
            break;
        case 3: // SBC
            cpu->A -= op2 + (cpu->F & FC ? 1 : 0);
            resolve_flags(cpu, FZ | FN | FH | FC, pre, cpu->A, cpu->F & FC);
            break;
        case 4: // AND
            cpu->A &= op2;
            set_flag(cpu, FZ, cpu->A == 0);
            cpu->F &= ~(FN | FC);
            cpu->F |= FH;
            break;
        case 5: // XOR
            cpu->A ^= op2;
            set_flag(cpu, FZ, cpu->A == 0);
            cpu->F &= ~(FN | FH | FC);
            break;
        case 6: // OR
            cpu->A |= op2;
            set_flag(cpu, FZ, cpu->A == 0);
            cpu->F &= ~(FN | FH | FC);
            break;
        case 7: // CP
            resolve_flags(cpu, FZ | FN | FH | FC, cpu->A, cpu->A - op2, 0);
            break;
    }
}

static void push(struct sm83* cpu, u16 val) {
    cpu_write8(cpu, --cpu->SP, val >> 8);
    cpu_write8(cpu, --cpu->SP, val);
}

static u16 pop(struct sm83* cpu) {
    u8 lo = cpu_read8(cpu, cpu->SP++);
    u8 hi = cpu_read8(cpu, cpu->SP++);
    return lo | (hi << 8);
}

void run_instruction(struct sm83* cpu) {
    u8 opcode = cpu_read8(cpu, cpu->PC++);
    switch ((opcode & 0b11000000) >> 6) {
        case 0:
            if ((opcode & 0b00000111) == 0) {
                if ((opcode & 0b00100000) == 0) {
                    switch ((opcode & 0b00011000) >> 3) {
                        case 0: // NOP
                            break;
                        case 1: // LD (nn), SP
                            u16 addr = cpu_read16(cpu, cpu->PC++);
                            cpu->PC++;
                            cpu_write16(cpu, addr, cpu->SP);
                            break;
                        case 2: // STOP
                            cpu->master->div = 0x0000;
                            if (cpu->master->io[KEY1] & 1) {
                                cpu->master->io[KEY1] =
                                    ~cpu->master->io[KEY1] & (1 << 7);
                            } else {
                                cpu->stop = true;
                            }
                            break;
                        case 3: // JR d
                            s8 disp = cpu_read8(cpu, cpu->PC++);
                            gb_m_cycle(cpu->master);
                            cpu->PC += disp;
                            break;
                    }
                } else { // JR cc, d
                    s8 disp = cpu_read8(cpu, cpu->PC++);
                    if (eval_cond(cpu, opcode)) {
                        gb_m_cycle(cpu->master);
                        cpu->PC += disp;
                    }
                }
            } else {
                if ((opcode & 0b00000100) == 0) {
                    switch (opcode & 0x0F) {
                        case 0b0001: // LD rr, nn
                            u16 nn = cpu_read16(cpu, cpu->PC++);
                            cpu->PC++;
                            *getr16mod(cpu, opcode) = nn;
                            break;
                        case 0b1001: // ADD HL, rr
                            gb_m_cycle(cpu->master);
                            u16 prev = cpu->HL;
                            cpu->HL += *getr16mod(cpu, opcode);
                            resolve_flags(cpu, FH | FC, prev >> 8, cpu->H,
                                          (prev & 0x00ff) > cpu->L);
                            break;
                        case 0b0010: // LD (rr), A
                            cpu_write8(cpu, getr16addr(cpu, opcode), cpu->A);
                            break;
                        case 0b1010: // LD A, (rr)
                            cpu->A = cpu_read8(cpu, getr16addr(cpu, opcode));
                            break;
                        case 0b0011: // INC rr
                            gb_m_cycle(cpu->master);
                            (*getr16mod(cpu, opcode))++;
                            break;
                        case 0b1011: // DEC rr
                            gb_m_cycle(cpu->master);
                            (*getr16mod(cpu, opcode))--;
                            break;
                    }
                } else {
                    u8* r;
                    u8 pre, post;
                    u8 c;
                    switch (opcode & 0b00000011) {
                        case 0: // INC r
                            if ((r = getr8dest(cpu, opcode))) {
                                pre = *r;
                                (*r)++;
                                post = *r;
                            } else {
                                pre = cpu_read8(cpu, cpu->HL);
                                post = pre + 1;
                                cpu_write8(cpu, cpu->HL, post);
                            }
                            resolve_flags(cpu, FZ | FH, pre, post, 0);
                            break;
                        case 1: // DEC r
                            if ((r = getr8dest(cpu, opcode))) {
                                pre = *r;
                                (*r)--;
                                post = *r;
                            } else {
                                pre = cpu_read8(cpu, cpu->HL);
                                post = pre - 1;
                                cpu_write8(cpu, cpu->HL, post);
                            }
                            resolve_flags(cpu, FZ | FN | FH, pre, post, 0);
                            break;
                        case 2: // LD r, n
                            u8 n = cpu_read8(cpu, cpu->PC++);
                            if ((r = getr8dest(cpu, opcode))) {
                                *r = n;
                            } else {
                                cpu_write8(cpu, cpu->HL, n);
                            }
                            break;
                        case 3:
                            switch ((opcode & 0b00111000) >> 3) {
                                case 0: // RLCA
                                    cpu->F &= ~(FZ | FN | FH);
                                    set_flag(cpu, FC, cpu->A & 0x80);
                                    cpu->A =
                                        cpu->A << 1 | (cpu->F & FC ? 0x01 : 0);
                                    break;
                                case 1: // RRCA
                                    cpu->F &= ~(FZ | FN | FH);
                                    set_flag(cpu, FC, cpu->A & 0x01);
                                    cpu->A =
                                        cpu->A >> 1 | (cpu->F & FC ? 0x80 : 0);
                                    break;
                                case 2: // RLA
                                    cpu->F &= ~(FZ | FN | FH);
                                    c = (cpu->F & FC ? 0x01 : 0);
                                    set_flag(cpu, FC, cpu->A & 0x80);
                                    cpu->A = cpu->A << 1 | c;
                                    break;
                                case 3: // RRA
                                    cpu->F &= ~(FZ | FN | FH);
                                    c = (cpu->F & FC ? 0x80 : 0);
                                    set_flag(cpu, FC, cpu->A & 0x01);
                                    cpu->A = cpu->A >> 1 | c;
                                    break;
                                case 4: // DAA
                                    u8 correction = 0x00;
                                    if (cpu->F & FN) {
                                        if (cpu->F & FH) {
                                            correction |= 0x06;
                                        }
                                        if (cpu->F & FC) {
                                            correction |= 0x60;
                                            cpu->F |= FC;
                                        }
                                        cpu->A -= correction;
                                    } else {
                                        if (cpu->F & FH ||
                                            (cpu->A & 0xf) > 0x9) {
                                            correction |= 0x06;
                                        }
                                        if (cpu->F & FC || cpu->A > 0x99) {
                                            correction |= 0x60;
                                            cpu->F |= FC;
                                        }
                                        cpu->A += correction;
                                    }
                                    cpu->F &= ~FH;
                                    set_flag(cpu, FZ, cpu->A == 0);
                                    break;
                                case 5: // CPL
                                    cpu->A = ~cpu->A;
                                    cpu->F |= FN | FH;
                                    break;
                                case 6: // SCF
                                    set_flag(cpu, FN, 0);
                                    set_flag(cpu, FH, 0);
                                    set_flag(cpu, FC, 1);
                                    break;
                                case 7: // CCF
                                    set_flag(cpu, FN, 0);
                                    set_flag(cpu, FH, 0);
                                    cpu->F ^= FC;
                                    break;
                            }
                            break;
                    }
                }
            }
            break;
        case 1:
            if (opcode == 0b01110110) { // HALT
                cpu->halt = true;
            } else { // LD r, r
                u8* r = getr8dest(cpu, opcode);
                if (r) {
                    *r = getr8src(cpu, opcode);
                } else {
                    cpu_write8(cpu, cpu->HL, getr8src(cpu, opcode));
                }
            }
            break;
        case 2: // ALU r
            run_alu(cpu, opcode, getr8src(cpu, opcode));
            break;
        case 3:
            switch (opcode & 0b00000111) {
                case 0:
                    if ((opcode & 0b00100000) == 0) { // RET cc
                        gb_m_cycle(cpu->master);
                        if (eval_cond(cpu, opcode)) {
                            cpu->PC = pop(cpu);
                            gb_m_cycle(cpu->master);
                        }
                    } else {
                        s8 disp;
                        switch ((opcode & 0b00011000) >> 3) {
                            case 0: // LD (FF00+n), A
                                cpu_write8(cpu,
                                           0xff00 + cpu_read8(cpu, cpu->PC++),
                                           cpu->A);
                                break;
                            case 1: // ADD SP, d
                                disp = cpu_read8(cpu, cpu->PC++);
                                u16 pre = cpu->SP;
                                gb_m_cycle(cpu->master);
                                gb_m_cycle(cpu->master);
                                cpu->SP += disp;
                                cpu->F &= ~FZ;
                                resolve_flags(cpu, FH | FC, pre & 0x00ff,
                                              cpu->SP & 0x00ff, 0);
                                break;
                            case 2: // LD A, (FF00+n)
                                u8 num = cpu_read8(cpu, cpu->PC++);
                                cpu->A = cpu_read8(cpu, 0xff00 + num);
                                break;
                            case 3: // LD HL, SP+d
                                disp = cpu_read8(cpu, cpu->PC++);
                                gb_m_cycle(cpu->master);
                                cpu->HL = cpu->SP + disp;
                                cpu->F &= ~FZ;
                                resolve_flags(cpu, FH | FC, cpu->SP & 0x00ff,
                                              cpu->L, 0);
                                break;
                        }
                    }
                    break;
                case 1:
                    if ((opcode & 0b00001000) == 0) { // POP rr
                        *getr16stack(cpu, opcode) = pop(cpu);
                        cpu->F &= 0xf0;
                    } else {
                        switch ((opcode & 0b00110000) >> 4) {
                            case 0: // RET
                                cpu->PC = pop(cpu);
                                gb_m_cycle(cpu->master);
                                break;
                            case 1: // RETI
                                cpu->PC = pop(cpu);
                                gb_m_cycle(cpu->master);
                                cpu->IME = true;
                                break;
                            case 2: // JP HL
                                cpu->PC = cpu->HL;
                                break;
                            case 3: // LD SP, HL
                                gb_m_cycle(cpu->master);
                                cpu->SP = cpu->HL;
                                break;
                        }
                    }
                    break;
                case 2:
                    if ((opcode & 0b00100000) == 0) { // JP cc, nn
                        u16 addr = cpu_read16(cpu, cpu->PC++);
                        cpu->PC++;
                        if (eval_cond(cpu, opcode)) {
                            gb_m_cycle(cpu->master);
                            cpu->PC = addr;
                        }
                    } else {
                        u16 addr;
                        switch ((opcode & 0b00011000) >> 3) {
                            case 0: // LD (FF00+C), A
                                cpu_write8(cpu, 0xff00 + cpu->C, cpu->A);
                                break;
                            case 1: // LD (nn), A
                                addr = cpu_read16(cpu, cpu->PC++);
                                cpu->PC++;
                                cpu_write8(cpu, addr, cpu->A);
                                break;
                            case 2: // LD A, (FF00+C)
                                cpu->A = cpu_read8(cpu, 0xff00 + cpu->C);
                                break;
                            case 3: // LD A, (nn)
                                addr = cpu_read16(cpu, cpu->PC++);
                                cpu->PC++;
                                cpu->A = cpu_read8(cpu, addr);
                                break;
                        }
                    }
                    break;
                case 3:
                    switch ((opcode & 0b00111000) >> 3) {
                        case 0: // JP nn
                            u16 addr = cpu_read16(cpu, cpu->PC++);
                            cpu->PC++;
                            gb_m_cycle(cpu->master);
                            cpu->PC = addr;
                            break;
                        case 1: // prefix
                            u8 cbcode = cpu_read8(cpu, cpu->PC++);
                            u8 val = getr8src(cpu, cbcode);
                            u8* dest = getr8dest(cpu, cbcode << 3);
                            u8 bit = (cbcode & 0b00111000) >> 3;
                            int store = 1;
                            int c;
                            switch ((cbcode & 0b11000000) >> 6) {
                                case 0:
                                    cpu->F &= ~(FN | FH);
                                    switch (bit) {
                                        case 0: // RLC r
                                            set_flag(cpu, FC, val & 0x80);
                                            val = val << 1 |
                                                  (cpu->F & FC ? 0x01 : 0);
                                            break;
                                        case 1: // RRC r
                                            set_flag(cpu, FC, val & 0x01);
                                            val = val >> 1 |
                                                  (cpu->F & FC ? 0x80 : 0);
                                            break;
                                        case 2: // RL r
                                            c = (cpu->F & FC ? 0x01 : 0);
                                            set_flag(cpu, FC, val & 0x80);
                                            val = val << 1 | c;
                                            break;
                                        case 3: // RR r
                                            c = (cpu->F & FC ? 0x80 : 0);
                                            set_flag(cpu, FC, val & 0x01);
                                            val = val >> 1 | c;
                                            break;
                                        case 4: // SLA r
                                            set_flag(cpu, FC, val & 0x80);
                                            val <<= 1;
                                            break;
                                        case 5: // SRA r
                                            set_flag(cpu, FC, val & 0x01);
                                            val = (s8) val >> 1;
                                            break;
                                        case 6: // SWAP r
                                            val = (val & 0xf0) >> 4 |
                                                  (val & 0x0f) << 4;
                                            cpu->F &= ~FC;
                                            break;
                                        case 7: // SRL r
                                            set_flag(cpu, FC, val & 0x01);
                                            val >>= 1;
                                            break;
                                    }
                                    set_flag(cpu, FZ, val == 0);
                                    break;
                                case 1: // BIT b, r
                                    store = 0;
                                    cpu->F &= ~FN;
                                    cpu->F |= FH;
                                    set_flag(cpu, FZ, !(val & (1 << bit)));
                                    break;
                                case 2: // RES b, r
                                    val &= ~(1 << bit);
                                    break;
                                case 3: // SET b, r
                                    val |= 1 << bit;
                                    break;
                            }
                            if (store) {
                                if (dest) *dest = val;
                                else cpu_write8(cpu, cpu->HL, val);
                            }
                            break;
                        case 6: // DI
                            cpu->IME = false;
                            break;
                        case 7: // EI
                            cpu->ei = true;
                            break;
                        default:
                            cpu->ill = true;
                            break;
                    }
                    break;
                case 4: // CALL cc, nn
                    u16 addr = cpu_read16(cpu, cpu->PC++);
                    cpu->PC++;
                    if (eval_cond(cpu, opcode)) {
                        gb_m_cycle(cpu->master);
                        push(cpu, cpu->PC);
                        cpu->PC = addr;
                    }
                    break;
                case 5:
                    if ((opcode & 0b00001000) == 0) { // PUSH rr
                        gb_m_cycle(cpu->master);
                        push(cpu, *getr16stack(cpu, opcode));
                    } else { // CALL nn
                        u16 addr = cpu_read16(cpu, cpu->PC++);
                        cpu->PC++;
                        gb_m_cycle(cpu->master);
                        push(cpu, cpu->PC);
                        cpu->PC = addr;
                    }
                    break;
                case 6: // ALU n
                    run_alu(cpu, opcode, cpu_read8(cpu, cpu->PC++));
                    break;
                case 7: // RST n
                    gb_m_cycle(cpu->master);
                    push(cpu, cpu->PC);
                    cpu->PC = opcode & 0b00111000;
                    break;
            }
            break;
    }
}

void cpu_isr(struct sm83* cpu) {
    cpu->halt = false;
    if (cpu->master->io[IF] & I_JOYPAD) cpu->stop = false;
    if (cpu->IME) {
        gb_m_cycle(cpu->master);
        gb_m_cycle(cpu->master);
        cpu->IME = false;
        cpu_write8(cpu, --cpu->SP, cpu->PC >> 8);
        int i;
        for (i = 0; i < 5; i++) {
            if ((cpu->master->IE & cpu->master->io[IF]) & (1 << i)) break;
        }
        cpu_write8(cpu, --cpu->SP, cpu->PC);
        gb_m_cycle(cpu->master);
        if (i < 5) {
            cpu->master->io[IF] &= ~(1 << i);
            cpu->PC = 0b01000000 | (i << 3);
        } else {
            cpu->PC = 0x0000;
        }
    }
}

void cpu_clock(struct sm83* cpu) {
    if (cpu->ill) return;

    if (cpu->master->hdma_index) {
        gb_m_cycle(cpu->master);
        return;
    }
    if (cpu->master->IE & cpu->master->io[IF] & 0b00011111) cpu_isr(cpu);
    if (cpu->ei) {
        cpu->IME = true;
        cpu->ei = false;
    }
    if (!cpu->halt && !cpu->stop) {
        run_instruction(cpu);
    } else {
        gb_m_cycle(cpu->master);
    }
}

u8 cpu_read8(struct sm83* cpu, u16 addr) {
    gb_m_cycle(cpu->master);

    if (0x8000 <= addr && addr < 0xa000 &&
        (cpu->master->io[LCDC] & LCDC_ENABLE) &&
        (cpu->master->io[STAT] & STAT_MODE) == 3)
        return 0xff;
    if (0xfe00 <= addr && addr < 0xfea0 &&
        (((cpu->master->io[LCDC] & LCDC_ENABLE) &&
          (cpu->master->io[STAT] & STAT_MODE) >= 2) ||
         cpu->master->dma_active))
        return 0xff;

    return read8(cpu->master, addr);
}

void cpu_write8(struct sm83* cpu, u16 addr, u8 data) {
    gb_m_cycle(cpu->master);

    if (cpu->master->dma_active && addr < 0xff00) return;
    if (0x8000 <= addr && addr < 0xa000 &&
        (cpu->master->io[LCDC] & LCDC_ENABLE) &&
        (cpu->master->io[STAT] & STAT_MODE) == 3)
        return;
    if (0xfe00 <= addr && addr < 0xfea0 &&
        (((cpu->master->io[LCDC] & LCDC_ENABLE) &&
          (cpu->master->io[STAT] & STAT_MODE) >= 2) ||
         cpu->master->dma_active))
        return;

    write8(cpu->master, addr, data);
}

u16 cpu_read16(struct sm83* cpu, u16 addr) {
    u8 lo = cpu_read8(cpu, addr++);
    u8 hi = cpu_read8(cpu, addr++);
    return lo | (hi << 8);
}

void cpu_write16(struct sm83* cpu, u16 addr, u16 data) {
    cpu_write8(cpu, addr++, data);
    cpu_write8(cpu, addr++, data >> 8);
}

void print_cpu_state(struct sm83* cpu) {
    fprintf(stderr,
            "A: %02X F: %02X B: %02X C: %02X D: %02X E: %02X H: %02X "
            "L: %02X SP: %04X PC: %04X (%02X %02X %02X %02X)\n",
            cpu->A, cpu->F, cpu->B, cpu->C, cpu->D, cpu->E, cpu->H, cpu->L,
            cpu->SP, cpu->PC, read8(cpu->master, cpu->PC),
            read8(cpu->master, cpu->PC + 1), read8(cpu->master, cpu->PC + 2),
            read8(cpu->master, cpu->PC + 3));
}
