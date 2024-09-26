#ifndef OPCODE_H
#define OPCODE_H

typedef enum
{
    ADD, ADDI, ADDIU, ADDU, AND, ANDI, BEQ, BNE, J, JAL, JR, JALR, LUI, LW, NOR, OR, ORI, SLT, SLTI, SLTIU, SLTU, SLL, SRL, SW, SUB, SUBU, INVALID
} Opcode;

extern const char* OpcodeNames[];

Opcode get_opcode(int opcode, int func);

#endif
