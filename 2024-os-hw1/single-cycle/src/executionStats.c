#include "../header/executionStats.h"
#include "../header/opcode.h"
#include <stdlib.h>

ExecutionStats stats = {0};

void decode_and_update_stats(Instruction decoded_inst, ExecutionStats *stats){
    Opcode opcode = get_opcode(decoded_inst.opcode, decoded_inst.func);
    if(decoded_inst.opcode == 0x0){
        stats->r_type_count++;
    } else if((opcode == J) || (opcode == JAL)){
        stats->j_type_count++;
    } else{
        stats->i_type_count++;
        if((opcode == BEQ) || (opcode == BNE)){
            stats->branch_count++;
        } else if((opcode == LW) || (opcode == SW)){
            stats->memory_access_count++;
        }
    }
}
