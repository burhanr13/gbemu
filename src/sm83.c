#include "sm83.h"

#include <stdlib.h>
#include <string.h>

#include "gb.h"

void init_cpu(struct gb* master, struct sm83* cpu) {
    memset(cpu, 0, sizeof(*cpu));
    cpu->master = master;
    cpu->A = 0x01;
    cpu->PC = 0x0100;
}

void set_flag(struct sm83* cpu, int flag, int val) {
    if (val) {
        cpu->F |= flag;
    } else {
        cpu->F &= ~flag;
    }
}

void resolve_flags(struct sm83* cpu, int flags, u8 pre, u8 post) {
    set_flag(cpu, FN, flags & FN);
    if (flags & FZ) {
        set_flag(cpu, FZ, post == 0);
    }
    if (flags & FH) {
        if (flags & FN) set_flag(cpu, FH, (pre & 0x0f) < (post & 0x0f));
        else set_flag(cpu, FH, (pre & 0x0f) > (post & 0x0f));
    }
    if (flags & FC) {
        if (flags & FN) set_flag(cpu, FC, pre < post);
        else set_flag(cpu, FC, pre > post);
    }
}

int eval_cond(struct sm83* cpu, u8 opcode) {
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

u16* getr16mod(struct sm83* cpu, u8 opcode) {
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

u16* getr16stack(struct sm83* cpu, u8 opcode) {
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

u16 getr16addr(struct sm83* cpu, u8 opcode) {
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

u8 getr8src(struct sm83* cpu, u8 opcode) {
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
            return read8(cpu->master, cpu->HL);
        case 7:
            return cpu->A;
    }
    return 0;
}

u8* getr8dest(struct sm83* cpu, u8 opcode) {
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

void run_alu(struct sm83* cpu, u8 opcode, u8 op2) {
    u8 pre = cpu->A;
    switch ((opcode & 0b00111000) >> 3) {
        case 0: // ADD
            cpu->A += op2;
            resolve_flags(cpu, FZ | FH | FC, pre, cpu->A);
            break;
        case 1: // ADC
            cpu->A += op2 + (cpu->F & FC ? 1 : 0);
            resolve_flags(cpu, FZ | FH | FC, pre, cpu->A);
            break;
        case 2: // SUB
            cpu->A -= op2;
            resolve_flags(cpu, FZ | FN | FH | FC, pre, cpu->A);
            break;
        case 3: // SBC
            cpu->A -= op2 + (cpu->F & FC ? 1 : 0);
            resolve_flags(cpu, FZ | FN | FH | FC, pre, cpu->A);
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
            resolve_flags(cpu, FZ | FN | FH | FC, cpu->A, cpu->A - op2);
            break;
    }
}

void push(struct sm83* cpu, u16 val) {
    cpu->SP -= 2;
    write16(cpu->master, cpu->SP, val);
}

u16 pop(struct sm83* cpu) {
    u16 val = read16(cpu->master, cpu->SP);
    cpu->SP += 2;
    return val;
}

void run_instruction(struct sm83* cpu) {
    u8 opcode = read8(cpu->master, cpu->PC++);
    switch ((opcode & 0b11000000) >> 6) {
        case 0:
            if ((opcode & 0b00000111) == 0) {
                if ((opcode & 0b00100000) == 0) {
                    switch ((opcode & 0b00011000) >> 3) {
                        case 0: // NOP
                            cpu->cycles += 4;
                            break;
                        case 1: // LD (nn), SP
                            cpu->cycles += 20;
                            u16 addr = read16(cpu->master, cpu->PC++);
                            cpu->PC++;
                            write16(cpu->master, addr, cpu->SP);
                            break;
                        case 2: // STOP
                            cpu->stop = 1;
                            break;
                        case 3: // JR d
                            cpu->cycles += 12;
                            s8 disp = read8(cpu->master, cpu->PC++);
                            cpu->PC += disp;
                            break;
                    }
                } else { // JR cc, d
                    cpu->cycles += 8;
                    s8 disp = read8(cpu->master, cpu->PC++);
                    if (eval_cond(cpu, opcode)) {
                        cpu->cycles += 4;
                        cpu->PC += disp;
                    }
                }
            } else {
                if ((opcode & 0b00000100) == 0) {
                    switch (opcode & 0x0F) {
                        case 0b0001: // LD rr, nn
                            cpu->cycles += 12;
                            u16 nn = read16(cpu->master, cpu->PC++);
                            cpu->PC++;
                            *getr16mod(cpu, opcode) = nn;
                            break;
                        case 0b1001: // ADD HL, rr
                            cpu->cycles += 8;
                            u16 prev = cpu->HL;
                            cpu->HL += *getr16mod(cpu, opcode);
                            set_flag(cpu, FN, 0);
                            set_flag(cpu, FH, (cpu->HL & 0xff) < (prev & 0xff));
                            set_flag(cpu, FC, cpu->HL < prev);
                            break;
                        case 0b0010: // LD (rr), A
                            cpu->cycles += 8;
                            write8(cpu->master, getr16addr(cpu, opcode),
                                   cpu->A);
                            break;
                        case 0b1010: // LD A, (rr)
                            cpu->cycles += 8;
                            cpu->A =
                                read8(cpu->master, getr16addr(cpu, opcode));
                            break;
                        case 0b0011: // INC rr
                            cpu->cycles += 8;
                            (*getr16mod(cpu, opcode))++;
                            break;
                        case 0b1011: // DEC rr
                            cpu->cycles += 8;
                            (*getr16mod(cpu, opcode))--;
                            break;
                    }
                } else {
                    u8* r;
                    u8 pre, post;
                    u8 c;
                    switch (opcode & 0b00000011) {
                        case 0: // INC r
                            cpu->cycles += 4;
                            if ((r = getr8dest(cpu, opcode))) {
                                pre = *r;
                                (*r)++;
                                post = *r;
                            } else {
                                cpu->cycles += 8;
                                pre = read8(cpu->master, cpu->HL);
                                post = pre + 1;
                                write8(cpu->master, cpu->HL, post);
                            }
                            resolve_flags(cpu, FZ | FH, pre, post);
                            break;
                        case 1: // DEC r
                            cpu->cycles += 4;
                            if ((r = getr8dest(cpu, opcode))) {
                                pre = *r;
                                (*r)--;
                                post = *r;
                            } else {
                                cpu->cycles += 8;
                                pre = read8(cpu->master, cpu->HL);
                                post = pre - 1;
                                write8(cpu->master, cpu->HL, post);
                            }
                            resolve_flags(cpu, FZ | FN | FH, pre, post);
                            break;
                        case 2: // LD r, n
                            cpu->cycles += 8;
                            u8 n = read8(cpu->master, cpu->PC++);
                            if ((r = getr8dest(cpu, opcode))) {
                                *r = n;
                            } else {
                                write8(cpu->master, cpu->HL, n);
                            }
                            break;
                        case 3:
                            switch ((opcode & 0b00111000) >> 3) {
                                case 0: // RLCA
                                    cpu->cycles += 4;
                                    cpu->F &= ~(FZ | FN | FH);
                                    set_flag(cpu, FC, cpu->A & 0x80);
                                    cpu->A =
                                        cpu->A << 1 | (cpu->F & FC ? 0x01 : 0);
                                    break;
                                case 1: // RRCA
                                    cpu->cycles += 4;
                                    cpu->F &= ~(FZ | FN | FH);
                                    set_flag(cpu, FC, cpu->A & 0x01);
                                    cpu->A =
                                        cpu->A >> 1 | (cpu->F & FC ? 0x80 : 0);
                                    break;
                                case 2: // RLA
                                    cpu->cycles += 4;
                                    cpu->F &= ~(FZ | FN | FH);
                                    c = (cpu->F & FC ? 0x01 : 0);
                                    set_flag(cpu, FC, cpu->A & 0x80);
                                    cpu->A = cpu->A << 1 | c;
                                    break;
                                case 3: // RRA
                                    cpu->cycles += 4;
                                    cpu->F &= ~(FZ | FN | FH);
                                    c = (cpu->F & FC ? 0x80 : 0);
                                    set_flag(cpu, FC, cpu->A & 0x01);
                                    cpu->A = cpu->A >> 1 | c;
                                    break;
                                case 4: // DAA
                                    cpu->cycles += 4;
                                    if (cpu->F & FN) {
                                        if (cpu->F & FH) {
                                            cpu->A -= 0x06;
                                        }
                                        if (cpu->F & FC) {
                                            cpu->A -= 0x06;
                                            cpu->F |= FC;
                                        }
                                    } else {
                                        if (cpu->F & FH ||
                                            (cpu->A & 0xf) > 0x9) {
                                            cpu->A += 0x06;
                                        }
                                        if (cpu->F & FC || cpu->A > 0x99) {
                                            cpu->A += 0x60;
                                            cpu->F |= FC;
                                        }
                                    }
                                    cpu->F &= ~FH;
                                    set_flag(cpu, FZ, cpu->A == 0);
                                    break;
                                case 5: // CPL
                                    cpu->cycles += 4;
                                    cpu->A = ~cpu->A;
                                    cpu->F |= FN | FH;
                                    break;
                                case 6: // SCF
                                    cpu->cycles += 4;
                                    set_flag(cpu, FN, 0);
                                    set_flag(cpu, FH, 0);
                                    set_flag(cpu, FC, 1);
                                    break;
                                case 7: // CCF
                                    cpu->cycles += 4;
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
                cpu->cycles += 4;
                cpu->halt = 1;
            } else { // LD r, r
                cpu->cycles += 4;
                u8* r = getr8dest(cpu, opcode);
                if ((opcode & 0b00000111) == 6) cpu->cycles += 4;
                if (r) {
                    *r = getr8src(cpu, opcode);
                } else {
                    cpu->cycles += 4;
                    write8(cpu->master, cpu->HL, getr8src(cpu, opcode));
                }
            }
            break;
        case 2: // ALU r
            cpu->cycles += 4;
            if ((opcode & 0b00000111) == 6) cpu->cycles += 4;
            run_alu(cpu, opcode, getr8src(cpu, opcode));
            break;
        case 3:
            switch (opcode & 0b00000111) {
                case 0:
                    if ((opcode & 0b00100000) == 0) { // RET cc
                        cpu->cycles += 8;
                        if (eval_cond(cpu, opcode)) {
                            cpu->cycles += 12;
                            cpu->PC = pop(cpu);
                        }
                    } else {
                        s8 disp;
                        switch ((opcode & 0b00011000) >> 3) {
                            case 0: // LD (FF00+n), A
                                cpu->cycles += 12;
                                write8(cpu->master,
                                       0xff00 + read8(cpu->master, cpu->PC++),
                                       cpu->A);
                                break;
                            case 1: // ADD SP, d
                                cpu->cycles += 16;
                                disp = read8(cpu->master, cpu->PC++);
                                u16 pre = cpu->SP;
                                cpu->SP += disp;
                                cpu->F &= ~(FZ | FN);
                                set_flag(cpu, FH,
                                         (pre & 0x00ff) > (cpu->SP & 0x00ff));
                                set_flag(cpu, FC, pre > cpu->SP);
                                break;
                            case 2: // LD A, (FF00+n)
                                cpu->cycles += 12;
                                cpu->A = read8(
                                    cpu->master,
                                    0xff00 + read8(cpu->master, cpu->PC++));
                                break;
                            case 3: // LD HL, SP+d
                                cpu->cycles += 12;
                                disp = read8(cpu->master, cpu->PC++);
                                cpu->HL = cpu->SP + disp;
                                cpu->F &= ~(FZ | FN);
                                set_flag(cpu, FH,
                                         (cpu->SP & 0x00ff) >
                                             (cpu->HL & 0x00ff));
                                set_flag(cpu, FC, cpu->SP > cpu->HL);
                                break;
                        }
                    }
                    break;
                case 1:
                    if ((opcode & 0b00001000) == 0) { // POP rr
                        cpu->cycles += 12;
                        *getr16stack(cpu, opcode) = pop(cpu);
                    } else {
                        switch ((opcode & 0b00110000) >> 4) {
                            case 0: // RET
                                cpu->cycles += 16;
                                cpu->PC = pop(cpu);
                                break;
                            case 1: // RETI
                                cpu->cycles += 16;
                                cpu->PC = pop(cpu);
                                cpu->master->IME = 1;
                                break;
                            case 2: // JP HL
                                cpu->cycles += 4;
                                cpu->PC = cpu->HL;
                                break;
                            case 3: // LD SP, HL
                                cpu->cycles += 8;
                                cpu->SP = cpu->HL;
                                break;
                        }
                    }
                    break;
                case 2:
                    if ((opcode & 0b00100000) == 0) { // JP cc, nn
                        cpu->cycles += 12;
                        u16 addr = read16(cpu->master, cpu->PC++);
                        cpu->PC++;
                        if (eval_cond(cpu, opcode)) {
                            cpu->cycles += 4;
                            cpu->PC = addr;
                        }
                    } else {
                        u16 addr;
                        switch ((opcode & 0b00011000) >> 3) {
                            case 0: // LD (FF00+C), A
                                cpu->cycles += 8;
                                write8(cpu->master, 0xff00 + cpu->C, cpu->A);
                                break;
                            case 1: // LD (nn), A
                                cpu->cycles += 16;
                                addr = read16(cpu->master, cpu->PC++);
                                cpu->PC++;
                                write8(cpu->master, addr, cpu->A);
                                break;
                            case 2: // LD A, (FF00+C)
                                cpu->cycles += 8;
                                cpu->A = read8(cpu->master, 0xff00 + cpu->C);
                                break;
                            case 3: // LD A, (nn)
                                cpu->cycles += 16;
                                addr = read16(cpu->master, cpu->PC++);
                                cpu->PC++;
                                cpu->A = read8(cpu->master, addr);
                                break;
                        }
                    }
                    break;
                case 3:
                    switch ((opcode & 0b00111000) >> 3) {
                        case 0: // JP nn
                            cpu->cycles += 16;
                            u16 addr = read16(cpu->master, cpu->PC++);
                            cpu->PC++;
                            cpu->PC = addr;
                            break;
                        case 1: // prefix
                            cpu->cycles += 8;
                            u8 cbcode = read8(cpu->master, cpu->PC++);
                            u8 val = getr8src(cpu, cbcode);
                            u8* dest = getr8dest(cpu, cbcode << 3);
                            if (!dest) cpu->cycles += 8;
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
                                    cpu->cycles -= 4;
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
                                else write8(cpu->master, cpu->HL, val);
                            }
                            break;
                        case 6: // DI
                            cpu->cycles += 4;
                            cpu->master->IME = 0;
                            break;
                        case 7: // EI
                            cpu->cycles += 4;
                            cpu->ei = 1;
                            break;
                        default:
                            cpu->ill = 1;
                            break;
                    }
                    break;
                case 4: // CALL cc, nn
                    cpu->cycles += 12;
                    u16 addr = read16(cpu->master, cpu->PC++);
                    cpu->PC++;
                    if (eval_cond(cpu, opcode)) {
                        cpu->cycles += 12;
                        push(cpu, cpu->PC);
                        cpu->PC = addr;
                    }
                    break;
                case 5:
                    if ((opcode & 0b00001000) == 0) { // PUSH rr
                        cpu->cycles += 16;
                        push(cpu, *getr16stack(cpu, opcode));
                    } else { // CALL nn
                        cpu->cycles += 24;
                        u16 addr = read16(cpu->master, cpu->PC++);
                        cpu->PC++;
                        push(cpu, cpu->PC);
                        cpu->PC = addr;
                    }
                    break;
                case 6: // ALU n
                    cpu->cycles += 8;
                    run_alu(cpu, opcode, read8(cpu->master, cpu->PC++));
                    break;
                case 7: // RST n
                    cpu->cycles += 16;
                    push(cpu, cpu->PC);
                    cpu->PC = opcode & 0b00111000;
                    break;
            }
            break;
    }
}

void cpu_clock(struct sm83* cpu) {
    if (cpu->stop || cpu->ill) return;
    if(cpu->master->IME && (cpu->master->IE & cpu->master->IF)) {
        cpu->cycles += 20;
        cpu->master->IME = 0;
        int i;
        for (i = 0; i < 5; i++) {
            if (cpu->master->IF & (1 << i)) break;
        }
        cpu->master->IF &= ~(1 << i);
        push(cpu, cpu->PC);
        cpu->PC = 0b01000000 | (i << 3);
        cpu->halt = 0;
    }
    if (cpu->ei) {
        cpu->master->IME = 1;
        cpu->ei = 0;
    }
    if (cpu->cycles == 0 && !cpu->halt) {
        run_instruction(cpu);
    }
    if (cpu->cycles) cpu->cycles--;
}