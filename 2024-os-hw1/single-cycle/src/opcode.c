#include "../header/opcode.h"

const char* OpcodeNames[] = {
    [ADD] = "add", [ADDI] = "addi", [ADDIU] = "addiu", [ADDU] = "addu",
    [AND] = "and", [ANDI] = "andi", [BEQ] = "beq", [BNE] = "bne",
    [J] = "j", [JAL] = "jal", [JR] = "jr", [JALR] = "jalr", [LUI] = "lui",
    [LW] = "lw", [NOR] = "nor", [OR] = "or", [ORI] = "ori",
    [SLT] = "slt", [SLTI] = "slti", [SLTIU] = "sltiu", [SLTU] = "sltu",
    [SLL] = "sll", [SRL] = "srl", [SW] = "sw", [SUB] = "sub", [SUBU] = "subu",
    [INVALID] = "invalid"
};

Opcode get_opcode(int opcode, int func) {
    switch(opcode) {
        case 0x0:
            switch(func){
                case 0x20: return ADD;
                case 0x21: return ADDU;
                case 0x24: return AND;
                case 0x08: return JR;
                case 0x09: return JALR;
                case 0x27: return NOR;
                case 0x25: return OR;
                case 0x2A: return SLT;
                case 0x2B: return SLTU;
                case 0x00: return SLL;
                case 0x02: return SRL;
                case 0x22: return SUB;
                case 0x23: return SUBU;
            }
        case 0x8: return ADDI;
        case 0x9: return ADDIU;
        case 0xC: return ANDI;
        case 0x4: return BEQ;
        case 0x5: return BNE;
        case 0x2: return J;
        case 0x3: return JAL;
        case 0xF: return LUI;
        case 0x23: return LW;
        case 0xD: return ORI;
        case 0xA: return SLTI;
        case 0xB: return SLTIU;
        case 0x2B: return SW;
        default: return INVALID;
    } 
}
