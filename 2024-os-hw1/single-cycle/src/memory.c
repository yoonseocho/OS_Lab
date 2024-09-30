#include "../header/memory.h"
#include <stdio.h>
#include <stdlib.h>

int inst_memory[MEMORY_SIZE] = {0, };
int *memory;

int load_instructions(const char *filename, int *inst_memory){
    FILE *fp = fopen(filename, "rb");
    if(!fp){
        perror("File opening failed");
        return -1; //파일 관련 오류
    }

    int inst_byte = 0;
    int count = 0;
    
    int i = 0;
    while(1){
        int numRead = fread(&inst_byte, sizeof(inst_byte),1,fp);
        if(numRead == 0){
            printf("All instructions are loaded to inst_mem\n\n");
            printf("===========single-cycle 시작=======================\n");
            break;
        }

        // printf("fread binary: 0x%x\n", inst_byte);

        //바이트 엔디언 변환(bin파일에서 읽은 데이터의 바이트 순서 변경) - 빅엔디언(0x123456678 -> 0x12가 가장 낮은주소에 위치), 리틀엔디언 to 빅엔디언
        unsigned int byte0, byte1, byte2, byte3;
        byte0 = inst_byte & 0x000000FF;
        byte1 = (inst_byte & 0x0000FF00) >> 8;
        byte2 = (inst_byte & 0x00FF0000) >> 16;
        byte3 = (inst_byte >> 24) & 0xFF;
        inst_byte = (byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3;

        // printf("reordered data: 0x%x\n", inst_byte);
        
        count++;
        inst_memory[i] = inst_byte;
        i++;
    }

    fclose(fp);

    return count;
}
