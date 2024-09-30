#ifndef CONTROLSIGNAL_h
#define CONTROLSIGNAL_h

#include "opcode.h"

typedef struct {
    int RegDest;
    int RegDest_ra;
    int ALUSrc;
    int ALUOp;
    int MemtoReg;
    int MemRead;
    int RegWrite;
    int MemWrite;
    int Branch;
    int Jump;
    int JR;
    int JAL;
} ControlSignals;

void clearControlSignals(ControlSignals *control);
void set_control_signals(Opcode opcode, ControlSignals *control);

#endif
