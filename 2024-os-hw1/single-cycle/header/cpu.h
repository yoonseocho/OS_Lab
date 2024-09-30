#ifndef CPU_H
#define CPU_H

#include "memory.h"
#include "control_signal.h"

extern int pc;
extern int reg[];

void initiate();
int fetch();
Instruction decode(int inst_byte);
void execute(Instruction decoded_inst, int* ALU_result, int* mem_index, ControlSignals *control);
void accessMemory(Instruction decoded_inst, int mem_index, int* mem_value, ControlSignals *control);
void writeBack(Instruction decoded_inst, int ALU_result, int mem_value, ControlSignals *control);
void branchAddr_ALU(int branch_Addr);
void pc_update_ALU();

#endif
