#include "../header/cpu.h"
#include "../header/alu.h"
#include <stdio.h>
#include <stdlib.h>

#define REGISTER_SIZE 32

int pc = 0;
int reg[REGISTER_SIZE] = {0, };

void initiate(){
    reg[29] = 0x1000000;
    reg[31] = 0xffffffff;

    memory = (int *)malloc(sizeof(int) * MEMORY_SIZE);
}

int fetch(){ // pc가 가리키는 inst_mem address에서 inst를 가져옴
    return inst_memory[pc/4]; // inst는 항상a 4 byte size
}

Instruction decode(int inst_byte){ // R,I,J타입 구분
    Instruction decoded_inst;
    decoded_inst.opcode = (inst_byte >> 26) & 0x3F;
    decoded_inst.rs = (inst_byte >> 21) & 0x1F;
    decoded_inst.rt = (inst_byte >> 16) & 0x1F;
    decoded_inst.rd = (inst_byte >> 11) & 0x1F;

    decoded_inst.shamt = (inst_byte >> 6) & 0x1F;
    decoded_inst.func = inst_byte & 0x3F;
    decoded_inst.imm = inst_byte & 0xFFFF;
    decoded_inst.s_imm =  (decoded_inst.imm & 0x8000) ?
                    (decoded_inst.imm | 0xFFFF0000) :
                    (decoded_inst.imm);
    decoded_inst.zero_ext_imm = decoded_inst.imm & 0xFFFF;
    decoded_inst.branch_addr = (decoded_inst.imm & 0x8000) ?
                            (0xFFFC0000 | (decoded_inst.imm << 2)) :
                            (decoded_inst.imm << 2);
    decoded_inst.jump_addr = (pc & 0xF0000000) | ((inst_byte & 0x3FFFFFF)<<2);
    return decoded_inst;
}

void branchAddr_ALU(int branch_Addr) {
    pc += branch_Addr;
}

void pc_update_ALU() {
    pc += 4;
}

void execute(Instruction decoded_inst, int* ALU_result, int* mem_index, ControlSignals *control){ // 각 타입별 명령어 실행
    Opcode opcode = get_opcode(decoded_inst.opcode, decoded_inst.func);
    clearControlSignals(control);
    set_control_signals(opcode, control);
    
    //명령어별로 구분하지 말고 input1, input2로 나눠서 구현..
    int input1 = reg[decoded_inst.rs];
    int input2 = control->ALUSrc ? decoded_inst.s_imm : reg[decoded_inst.rt]; // ALUSrc 신호에 따라 두 번째 ALU 입력 결정

    *ALU_result = ALU(decoded_inst.opcode, input1, input2, decoded_inst.s_imm, decoded_inst.shamt, decoded_inst.zero_ext_imm, control->ALUOp, pc);

    if (control->MemRead || control->MemWrite) {
        int effective_address = input1 + decoded_inst.s_imm; // 유효 주소 계산
        *mem_index = effective_address / 4; // 메모리 인덱스 계산
        if (*mem_index < 0 || *mem_index >= MEMORY_SIZE) {
            fprintf(stderr, "Memory access error: Invalid memory index %d at PC=0x%x\n", *mem_index, pc);
            exit(EXIT_FAILURE);
        }
    }
    pc_update_ALU();
}

void accessMemory(Instruction decoded_inst, int mem_index, int* mem_value, ControlSignals *control) {
    if (control->MemRead) {
        *mem_value = memory[mem_index];
    } else if (control->MemWrite) {
        memory[mem_index] = reg[decoded_inst.rt];
    }
    
}

void writeBack(Instruction decoded_inst, int ALU_result, int mem_value, ControlSignals *control) {
    if (control->RegWrite) {
        int write_val = control->MemtoReg ? mem_value : ALU_result;
        if (control->RegDest) {
            reg[decoded_inst.rd] = write_val;
        } else if (control->RegDest_ra){
            reg[31] = pc + 4;
        } else{
            reg[decoded_inst.rt] = write_val;
        }
    }
    // writeBack에서 pc jump, beq update 처리하기
    Opcode opcode = get_opcode(decoded_inst.opcode, decoded_inst.func);
    
    if (control->Branch && ((opcode == BEQ && reg[decoded_inst.rs] == reg[decoded_inst.rt]) || (opcode == BNE && reg[decoded_inst.rs] != reg[decoded_inst.rt]))) {
        branchAddr_ALU(decoded_inst.branch_addr);
    } else if (control->Jump || control->JAL) {
        pc = decoded_inst.jump_addr;
    } else if (control->JR) {
        pc = reg[decoded_inst.rs];
    }
}
