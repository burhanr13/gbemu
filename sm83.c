#include "sm83.h"

#include <stdlib.h>

#include "gb.h"

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
    if(flags & FH) {
        if (flags & FN) set_flag(cpu, FH, (pre & 0x0f) < (post & 0x0f));
        else set_flag(cpu, FH, (pre & 0x0f) > (post & 0x0f));
    }
    if(flags & FC){
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

u16* getr16g1(struct sm83* cpu, u8 opcode) {
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

u16 getr16g2(struct sm83* cpu, u8 opcode) {
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
    switch(opcode & 0b00000111){
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
                            cpu->PC++;
                            cpu->stopped = 1;
                            break;
                        case 3: // JR
                            cpu->cycles += 12;
                            s8 disp = read8(cpu->master, cpu->PC++);
                            cpu->PC += disp;
                            break;
                    }
                } else { // JR cc
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
                            *getr16g1(cpu, opcode) = nn;
                            break;
                        case 0b1001: // ADD HL, rr
                            cpu->cycles += 8;
                            u16 prev = cpu->HL;
                            cpu->HL += *getr16g1(cpu, opcode);
                            set_flag(cpu, FN, 0);
                            set_flag(cpu, FH, (cpu->HL & 0xff) < (prev & 0xff));
                            set_flag(cpu, FC, cpu->HL < prev);
                            break;
                        case 0b0010: // LD (rr), A
                            cpu->cycles += 8;
                            write8(cpu->master, getr16g2(cpu, opcode), cpu->A);
                            break;
                        case 0b1010: // LD A, (rr)
                            cpu->cycles += 8;
                            cpu->A = read8(cpu->master, getr16g2(cpu, opcode));
                            break;
                        case 0b0011: // INC rr
                            cpu->cycles += 8;
                            (*getr16g1(cpu, opcode))++;
                            break;
                        case 0b1011: // DEC rr
                            cpu->cycles += 8;
                            (*getr16g1(cpu, opcode))--;
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
                            if((r = getr8dest(cpu,opcode))){
                                *r = n;
                            }else{
                                write8(cpu->master, cpu->HL, n);
                            }
                            break;
                        case 3:
                            switch((opcode & 0b00111000) >> 3) {
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
            break;
        case 2:
            break;
        case 3:
            break;
    }
}

void clock(struct sm83* cpu) {
    if (cpu->stopped) return;
    if (cpu->cycles == 0) {
        run_instruction(cpu);
    }
    cpu->cycles--;
}