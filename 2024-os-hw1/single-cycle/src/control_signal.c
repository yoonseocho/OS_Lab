#include "../header/control_signal.h"
#include <string.h>

void clearControlSignals(ControlSignals *control) {
    memset(control, 0, sizeof(ControlSignals));
}

void set_control_signals(Opcode opcode, ControlSignals *control){
    switch(opcode) {
        case ADD:
            control->RegDest = 1;
            control->ALUOp = 1;
            control->RegWrite = 1;
            break;
        case ADDU:
            control->RegDest = 1;
            control->ALUOp = 2;
            control->RegWrite = 1;
            break;
        case AND:
            control->RegDest = 1;
            control->ALUOp = 3;
            control->RegWrite = 1;
            break;
        case NOR:
            control->RegDest = 1;
            control->ALUOp = 4;
            control->RegWrite = 1;
            break;
        case OR:
            control->RegDest = 1;
            control->ALUOp = 5;
            control->RegWrite = 1;
            break;
        case SLT:
            control->RegDest = 1;
            control->ALUOp = 6;
            control->RegWrite = 1;
            break;
        case SLTU:
            control->RegDest = 1;
            control->ALUOp = 7;
            control->RegWrite = 1;
            break;
        case SLL:
            control->RegDest = 1;
            control->ALUOp = 8;
            control->RegWrite = 1;
            break;
        case SRL:
            control->RegDest = 1;
            control->ALUOp = 9;
            control->RegWrite = 1;
            break;
        case SUB:
            control->RegDest = 1;
            control->ALUOp = 10;
            control->RegWrite = 1;
            break;
        case SUBU:
            control->RegDest = 1;
            control->ALUOp = 11;
            control->RegWrite = 1;
            break;
        case JR:
            control->ALUOp = 12;
            control->JR = 1;
            break;
        case JALR:
            control->ALUOp = 13;
            control->RegWrite = 1;
            control->RegDest_ra = 1;
            control->JR = 1;
            break;
        case ADDI:
        case ADDIU:
        case ANDI:
        case LUI:
        case ORI:
        case SLTI:
        case SLTIU:
            control->RegWrite = 1;
            control->ALUSrc = 1;
            break;
        case LW:
            control->RegWrite = 1;
            control->ALUSrc = 1;
            control->MemtoReg = 1;
            control->MemRead = 1;
            break;
        case SW:
            control->RegWrite = 0;
            control->ALUSrc = 1;
            control->MemWrite = 1;
            break;
        case BEQ:
        case BNE:
            control->ALUSrc = 1;
            control->Branch = 1;
            break;
        case J:
            control->Jump = 1;
            break;
        case JAL:
            control->RegWrite = 1;
            control->RegDest_ra = 1;
            control->JAL = 1;
            break;
    
        default:
            break;
    }
}
