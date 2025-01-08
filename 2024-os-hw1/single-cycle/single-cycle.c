#include "header/cpu.h"
#include "header/executionStats.h"
#include <stdio.h>
#include <stdlib.h>

#define PC_END 0xFFFFFFFF

void print_state(Instruction decoded_inst, int mem_index, int mem_value);
void print_result(ExecutionStats *stats);

int main(int argc, char*argv[]){
    if(argc < 2){
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return 1; // 일반적인 사용법 오류
    }
    
    int num_instructions = load_instructions(argv[1], inst_memory);
    
    if(num_instructions < 0){
        fprintf(stderr, "Failed to load instructions from %s\n", argv[1]);
        return 1; // 파일 관련 오류
    }

    initiate();

    int ALU_result = 0;
    int mem_index = 0;
    int mem_value = 0;
    ControlSignals control;

    while(pc != PC_END){
        int inst_byte = fetch();
        Instruction decoded_inst = decode(inst_byte);
        decode_and_update_stats(decoded_inst, &stats);
        execute(decoded_inst, &ALU_result, &mem_index, &control);
        accessMemory(decoded_inst, mem_index, &mem_value, &control);
        writeBack(decoded_inst, ALU_result, mem_value, &control);
        print_state(decoded_inst, mem_index, mem_value);
        stats.cycle_count++;
    }

    print_result(&stats);
    free(memory);
    return 0;
}

void print_state(Instruction decoded_inst, int mem_index, int mem_value){
    // fprintf(stderr, "R[%d] = %d, R[%d] = %d, R[%d] = %d, s_imm = %d\n",decoded_inst.rs, reg[decoded_inst.rs], decoded_inst.rt, reg[decoded_inst.rt], decoded_inst.rd, reg[decoded_inst.rd], decoded_inst.s_imm);
    Opcode opcode = get_opcode(decoded_inst.opcode, decoded_inst.func);
    switch(opcode) {
        case ADD:
        case ADDU:
        case AND:
        case NOR:
        case OR:
        case SLT:
        case SLTU:
        case SLL:
        case SRL:
        case SUB:
        case SUBU:
            printf("@0x%x : %s R[%d] : %d\n", pc-4, OpcodeNames[opcode], decoded_inst.rd, reg[decoded_inst.rd]);
            break;
        case ADDI:
        case ADDIU:
        case ANDI:
        case LUI:
        case ORI:
        case SLTI:
        case SLTIU:
            printf("@0x%x : %s R[%d] : %d\n", pc-4, OpcodeNames[opcode], decoded_inst.rt, reg[decoded_inst.rt]);
            break;
        case LW:
            printf("@0x%x : %s R[%d] : %d\n", pc-4, OpcodeNames[opcode], decoded_inst.rt, mem_value);
            break;
        case SW:
            printf("@0x%x : %s M[%d] : %d\n", pc-4, OpcodeNames[opcode], mem_index, mem_value);
            break;
        case BEQ:
        case BNE:
        case J:
        case JAL:
        case JR:
        case JALR:
            printf("%s PC : 0x%x\n",OpcodeNames[opcode], pc);
            break;
        default:
            break;
    }
}

void print_result(ExecutionStats *stats){
    printf("\n===========================PROGRAM RESULT=============================\n");
	printf("Return value R[2] : %d\n", reg[2]);
	printf("Total Cycle : %d\n", stats->cycle_count);
	printf("Executed 'R' instruction : %d\n", stats->r_type_count);
	printf("Executed 'I' instruction : %d\n", stats->i_type_count);
	printf("Executed 'J' instruction : %d\n", stats->j_type_count);
	printf("Number of Memory Access Instruction : %d\n", stats->memory_access_count);
    printf("Number of Branch Taken : %d\n", stats->branch_count);
    printf("======================================================================\n");
    return;
}
