#ifndef MEMORY_H
#define MEMORY_H

#define MEMORY_SIZE 0x4000000

extern int inst_memory[];
extern int *memory;

typedef struct {
    int opcode;
    int rs;
    int rt;
    int rd;

    int shamt;
    int func;
    int imm;
    int s_imm;
    int zero_ext_imm;
    int branch_addr;
    int jump_addr;
} Instruction;

int load_instructions(const char *filename, int *inst_memory);

#endif
