#include "../header/alu.h"
#include <stdio.h>
#include <stdlib.h>


int ALU(int opcode, int rs, int rt, int s_imm, int shamt, int zero_ext_imm, int control_ALUOp, int pc){
    switch(opcode){
        case 0x0:
            switch(control_ALUOp){
                case 1: //ADD
                    if((rs > 0 && rt > 0 && (rs+rt)<0) || (rs<0 && rt<0 && (rs+rt)>0)){
                    fprintf(stderr, "Overflow error in ADD at pc=%x\n",pc);
                    exit(EXIT_FAILURE);
                    }
                case 2: //ADDU
                    return (unsigned)rs + (unsigned)rt;
                case 3: //AND
                    return rs & rt;
                case 4: //NOR
                    return ~(rs | rt);
                case 5: //OR
                    return rs | rt;
                case 6: //SLT
                    return (rs < rt) ? 1 : 0;
                case 7: //SLTU
                    return ((unsigned)rs < (unsigned)rt) ? 1 : 0;
                case 8: //SLL
                    return rt << shamt;
                case 9: //SRL
                    return rt >> shamt;
                case 10: //SUB
                    if((rs > 0 && rt > 0 && (rs+rt)<0) || (rs<0 && rt<0 && (rs+rt)>0)){
                    fprintf(stderr, "Overflow error in SUB at pc=%x\n",pc);
                    exit(EXIT_FAILURE);
                    }
                case 11: //SUBU
                    return (unsigned)rs - (unsigned)rt;
                default: return 0;
            }
        case 0x8:
            if((rs > 0 && rt > 0 && (rs+rt)<0) || (rs<0 && rt<0 && (rs+rt)>0)){
                fprintf(stderr, "Overflow error in ADDI at pc=%x\n",pc);
                exit(EXIT_FAILURE);
            }
        case 0x9: return (unsigned)rs + (unsigned)s_imm;
        case 0xC: return rs & zero_ext_imm;
        case 0xF: return s_imm << 16;
        case 0xD: return rs | zero_ext_imm;
        case 0xA: return (rs < s_imm) ? 1 : 0;
        case 0xB: return ((unsigned)rs < (unsigned)s_imm) ? 1 : 0;
        default: return 0;
    }
}
