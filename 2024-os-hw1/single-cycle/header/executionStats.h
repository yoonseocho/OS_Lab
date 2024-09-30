#ifndef EXECUTIONSTATS_H
#define EXECUTIONSTATS_H

#include "memory.h"

typedef struct {
    int cycle_count;
    int r_type_count;
    int i_type_count;
    int j_type_count;
    int branch_count;
    int memory_access_count;
} ExecutionStats;

extern ExecutionStats stats;

void decode_and_update_stats(Instruction decoded_inst, ExecutionStats *stats);

#endif
