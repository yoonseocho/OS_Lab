#include "swapping.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

Queue* run_queue = NULL;
Queue* wait_queue = NULL;
FILE *fp = NULL;
int msgq_id = 0;                    // IPC 메시지 큐 ID
int msgq_id_parent_to_child = 0;    // 부모 -> 자식 메시지 큐
int msgq_id_child_to_parent = 0;
int current_time = 0;               // 모든 타임틱을 1씩 증가하며 출력용으로 사용
int completed_processes = 0;
pid_t child_pids[NUM_PROCESSES] = {0}; // 프로세스 정보를 담을 배열과 자식 PID 배열
Process* processes[NUM_PROCESSES] = {0};
int minute = 2000; // 10분

// Virtual Memory(Paging) 관련 배열 선언
pcb pcbs[NUM_PROCESSES] = {0};
frame* RAM;
bool* free_page_list;
int ram_usage_timestamps[RAM_FRAME_COUNT] = {0}; // RAM 프레임별 사용 시간 저장
frame* DISK;
bool* free_disk_list;
int used_ram_frames;
int used_disk_frames;

Queue* init_queue() {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->front = 0;
    queue->rear = 0;
    return queue;
}
int isEmpty(Queue* queue) {
    return queue->front == queue->rear;
}
int isFull(Queue* queue) {
    return (queue->rear + 1) % MAX_QUEUE_SIZE == queue->front;
}
void enqueue(Queue* queue, Process* process) {
    if (isFull(queue)) {
        return;
    }
    queue->processes[queue->rear] = process;
    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
}
Process* dequeue(Queue* queue) {
    Process* process = NULL;
    if (isEmpty(queue)) {
        return process;
    }
    process = queue->processes[queue->front];
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    return process;
}

void setup_msgq() {
    // 기존 메시지 큐들을 먼저 제거
    msgctl(msgget(KEY, 0666), IPC_RMID, NULL);
    msgctl(msgget(KEY + 1, 0666), IPC_RMID, NULL);
    msgctl(msgget(KEY + 2, 0666), IPC_RMID, NULL);
    
    // 새로운 메시지 큐 생성
    msgq_id = msgget(KEY, IPC_CREAT | 0666);
    if (msgq_id == -1) {
        perror("msgget failed");
        exit(1);
    }
    
    msgq_id_parent_to_child = msgget(KEY + 1, IPC_CREAT | 0666);
    if (msgq_id_parent_to_child == -1) {
        perror("msgget failed for parent -> child");
        exit(1);
    }

    msgq_id_child_to_parent = msgget(KEY + 2, IPC_CREAT | 0666);
    if (msgq_id_child_to_parent == -1) {
        perror("msgget failed for child -> parent");
        exit(1);
    }
}

void print_queue_status() {
    fprintf(fp, "\nTimer tick: %d\n", current_time);  // 디버깅용 출력 추가
    fprintf(fp, "Completed %d/%d processes\n", completed_processes, NUM_PROCESSES);

    // Run Queue 상태 출력
    fprintf(fp, "RunQ = [");
    int i = run_queue->front;
    while (i != run_queue->rear) {
        Process* process = run_queue->processes[i];
        fprintf(fp, "P_%d(%d, %d)", process->pid, process->remaining_cpu, process->remaining_io);
        if ((i + 1) % MAX_QUEUE_SIZE != run_queue->rear) {
            fprintf(fp, ", ");
        }
        i = (i + 1) % MAX_QUEUE_SIZE;
    }
    fprintf(fp, "]\n");

    // Wait Queue 상태 출력
    fprintf(fp, "WaitQ = [");
    int j = wait_queue->front;
    while (j != wait_queue->rear) {
        Process* process = wait_queue->processes[j];
        fprintf(fp, "P_%d(%d, %d)", process->pid, process->remaining_cpu, process->remaining_io);
        if ((j + 1) % MAX_QUEUE_SIZE != wait_queue->rear) {
            fprintf(fp, ", ");
        }
        j = (j + 1) % MAX_QUEUE_SIZE;
    }
    fprintf(fp, "]\n");
    fflush(fp);
}

void alrm_handler(int signum){
    current_time++;
}

void signal_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));         // 구조체를 0으로 초기화
    sa.sa_handler = alrm_handler;       // 핸들러 함수 설정
    sigaction(SIGALRM, &sa, NULL);      // SIGALRM 시그널에 대한 핸들러 설정
}

void setup_timer() {
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = TIME_QUANTUM * TIME_TICK * 1000;
    timer.it_interval = timer.it_value;

    setitimer(ITIMER_REAL, &timer, NULL);
}

// Virtual Memory(Paging) 관련 함수
void initialize_free_page_list() {
    RAM = (frame*)malloc(TOTAL_PAGE_SIZE * sizeof(frame));
    if (!RAM) {
        perror("Failed to allocate memory for RAM");
        exit(1);
    }
    memset(RAM, 0, TOTAL_PAGE_SIZE * sizeof(frame));

    free_page_list = (bool*)malloc(RAM_FRAME_COUNT * sizeof(bool));
    if (!free_page_list) {
        perror("Failed to allocate memory for free_ram_list");
        exit(1);
    }
    
    memset(free_page_list, true, RAM_FRAME_COUNT * sizeof(bool));
}

int allocate_frame() {
    for (int i = 0; i < RAM_FRAME_COUNT; i++) {
        if (free_page_list[i]) { // 사용 가능한 프레임 찾기
            free_page_list[i] = false; // 할당 상태로 변경
            return i;
        }
    }
    return -1; // 사용 가능한 프레임 없음
}

void initialize_pcb(pcb* process_pcb, int pid) {
    process_pcb->pid = pid; // 프로세스 ID 설정

    // L1 디렉토리를 위한 메모리 할당
    process_pcb->pg_dir = (int**)malloc(NUM_L1_ENTRY * sizeof(int*));
    if (!process_pcb->pg_dir) {
        printf("Error: Failed to allocate memory for L1 directory!\n");
        exit(1);
    }

    // L1 디렉토리 초기화
    for (int i = 0; i < NUM_L1_ENTRY; i++) {
        process_pcb->pg_dir[i] = NULL; // 초기에는 L2 테이블 없음
    }

    printf("Initialized PCB for process %d with L1 directory at %p\n", pid, process_pcb->pg_dir);
}

int translate_VA_to_PA(int** TTBR, int va) {
    int l1_index = (va >> 20) & L1_INDEX_MASK;  // 상위 12비트
    int l2_index = (va >> 12) & L2_INDEX_MASK;  // 중간 8비트
    int offset = va & PAGE_OFFSET_MASK;         // 하위 12비트

    // L1 디렉토리에서 L2 테이블 확인
    if (TTBR[l1_index] == NULL) {
        printf("!!! - Error: L2 table is NULL for L1 index %d\n", l1_index);
        return -1; // 페이지 폴트 발생
    }

    // L2 테이블 가져오기
    int* L2 = TTBR[l1_index];

    // 유효 비트 확인 (예: L2 테이블의 MSB를 유효 비트로 사용)
    if ((L2[l2_index] & VALID_BIT_MASK) == 0) { // 유효 비트가 0이라면
        printf("!!! - Error: L2[%d] for L1[%d] is NULL!\n", l2_index, l1_index);
        return -1; // 페이지 폴트 발생
    }
    int frame = L2[l2_index] & FRAME_NUMBER_MASK; // 프레임 번호 추출

    // DISK에 있는 경우 스왑 인 필요
    if ((L2[l2_index] & VALID_BIT_MASK) && (L2[l2_index] & RAM_DISK_BIT_MASK)) {
        frame = swap_in(frame);
        L2[l2_index] = (frame & FRAME_NUMBER_MASK) | VALID_BIT_MASK;
    }

    ram_usage_timestamps[frame] = current_time; // 타임스탬프 갱신
    int pa = (frame << 12) | offset;            // 물리 주소 생성
    return pa;
}

bool is_page_fault(int** TTBR, int va) {
    int l1_index = (va >> 20) & L1_INDEX_MASK; // 상위 12비트로 L1 인덱스 계산
    int l2_index = (va >> 12) & L2_INDEX_MASK; // 중간 8비트로 L2 인덱스 계산

    // L1 디렉토리 확인
    if (TTBR[l1_index] == NULL) {
        // printf("Page fault detected: L1[%08x] entry is NULL.\n", l1_index);
        return true;
    }

    // L2 테이블의 유효 비트 확인
    int* L2 = TTBR[l1_index];
    if ((L2[l2_index] & VALID_BIT_MASK) == 0) {
        // printf("Page fault detected: L2[%08x] for L1[%08x] is invalid.\n", l2_index, l1_index);
        return true;
    }

    return false; // 페이지 폴트 아님
}

void handle_page_fault(int** TTBR, int va, int pid) {
    int l1_index = (va >> 20) & L1_INDEX_MASK;
    int l2_index = (va >> 12) & L2_INDEX_MASK;

    // L1 디렉토리에서 L2 테이블 확인 및 할당
    if (TTBR[l1_index] == NULL) {
        // printf("Allocating L2 table for L1[%08x]\n", l1_index);
        TTBR[l1_index] = (int*)malloc(NUM_L2_ENTRY * sizeof(int));
        if (TTBR[l1_index] == NULL) {
            // printf("Error: Failed to allocate memory for L2 table\n");
            exit(1);
        }

        // L2 테이블 초기화
        for (int i = 0; i < NUM_L2_ENTRY; i++) {
            TTBR[l1_index][i] = 0; // 초기화 (유효하지 않은 상태로 설정)
        }
    }

    int* L2 = TTBR[l1_index];

    // 새로운 프레임 할당
    int frame = allocate_frame();
    if (frame == -1) { // RAM이 부족하면 스왑 아웃 수행
        // printf("RAM is full. Performing swap out.\n");
        swap_out();                 // 가장 오래된 프레임을 DISK로 이동
        frame = allocate_frame();   // 다시 프레임 할당 시도
        if (frame == -1) {
            printf("Error: No available frames even after swap out\n");
            exit(1);
        }
    }

    // L2 테이블 엔트리 업데이트: 유효 비트 설정 및 프레임 번호 기록
    L2[l2_index] = (frame & FRAME_NUMBER_MASK) | VALID_BIT_MASK; // 프레임 번호와 유효 비트 설정
    // printf("Page fault resolved: VA %08x -> Frame %d allocated\n", va, frame);
}

void initialize_disk() {
    DISK = (frame*)malloc(TOTAL_PAGE_SIZE * sizeof(frame));
    if (!DISK) {
        perror("Failed to allocate memory for DISK");
        exit(1);
    }
    memset(DISK, 0, TOTAL_PAGE_SIZE * sizeof(frame));

    // DISK 상태를 관리할 리스트 초기화
    free_disk_list = (bool*)malloc(TOTAL_PAGE_SIZE * sizeof(bool));
    if (!free_disk_list) {
        perror("Failed to allocate memory for free_disk_list");
        exit(1);
    }
    memset(free_disk_list, true, TOTAL_PAGE_SIZE * sizeof(bool));
}

void swap_out() {
    // 0 ~ 2^18
    int oldest_frame = find_oldest_frame();
    if (oldest_frame == -1) {
        printf("Error: No frame available to swap out\n");
        exit(1);
    }

    // 사용 가능한 DISK 프레임 찾기
    int disk_frame = -1;
    for (int i = 0; i < TOTAL_PAGE_SIZE; i++) {
        if (free_disk_list[i]) {
            disk_frame = i;
            free_disk_list[i] = false; // DISK 프레임 할당
            break;
        }
    }

    if (disk_frame == -1) {
        printf("Error: No available disk frame\n");
        exit(1);
    }

    memcpy(&DISK[disk_frame], &RAM[oldest_frame], sizeof(frame));
    // oldest_frame을 가리켰던, L2 페이지테이블 값을 업데이트 필요
    for (int i = 0; i < NUM_PROCESSES; i++) {
        pcb* process_pcb = &pcbs[i];
        for (int l1_index = 0; l1_index < NUM_L1_ENTRY; l1_index++) {
            int* L2 = process_pcb->pg_dir[l1_index];
            if (L2 == NULL) continue; // L2 테이블이 없는 경우 스킵

            for (int l2_index = 0; l2_index < NUM_L2_ENTRY; l2_index++) {
                // 현재 RAM 프레임을 참조 중인 L2 엔트리를 찾음
                if ((L2[l2_index] & FRAME_NUMBER_MASK) == oldest_frame) {
                    // DISK 프레임으로 업데이트
                    L2[l2_index] = (disk_frame & FRAME_NUMBER_MASK) | VALID_BIT_MASK | RAM_DISK_BIT_MASK;
                    free_page_list[oldest_frame] = true; // RAM에서 비활성화
                    printf("Swapped out RAM frame %d to DISK frame %d\n", oldest_frame, disk_frame);
                    return; // 업데이트 후 종료
                }
            }
        }
    }
    // L2[l2_index] = (disk_frame & FRAME_NUMBER_MASK) | 0xc0000000;
}

int swap_in(int disk_frame) {
    // 0 ~ 2^18
    int ram_frame = allocate_frame(); // RAM에서 사용 가능한 프레임 할당
    if (ram_frame == -1) { // RAM이 부족하면 스왑 아웃 실행
        swap_out();
        ram_frame = allocate_frame(); // 다시 프레임 할당
    }

    // DISK 데이터를 RAM으로 복사
    memcpy(&RAM[ram_frame], &DISK[disk_frame], sizeof(frame));

    // DISK 프레임 해제
    free_disk_list[disk_frame] = true;

    // RAM 프레임 활성화
    free_page_list[ram_frame] = false;
    printf("Swapped in DISK frame %d to RAM frame %d\n", disk_frame, ram_frame);

    return ram_frame;
}

int find_oldest_frame() {
    int oldest_frame = -1;
    int oldest_time = __INT_MAX__;

    for (int i = 0; i < RAM_FRAME_COUNT; i++) {
        if (!free_page_list[i] && ram_usage_timestamps[i] < oldest_time) {
            oldest_time = ram_usage_timestamps[i];
            oldest_frame = i;
        }
    }
    return oldest_frame;
}

void print_memory_status() {
    used_ram_frames = 0;
    used_disk_frames = 0;

    // RAM 사용 상태 계산
    for (int i = 0; i < RAM_FRAME_COUNT; i++) {
        if (!free_page_list[i]) used_ram_frames++;
    }

    // DISK 사용 상태 계산
    for (int i = 0; i < TOTAL_PAGE_SIZE; i++) {
        if (!free_disk_list[i]) used_disk_frames++;
    }

    // 출력
    printf("RAM: %d / %d\n", used_ram_frames, RAM_FRAME_COUNT);
    printf("DISK: %d / %d\n", used_disk_frames, TOTAL_PAGE_SIZE);
}


void execute_parent_task() {
    // 1. wait queue 처리
    if (!isEmpty(wait_queue)) {
        int i = wait_queue->front;
        while (i != wait_queue->rear) {
            Process* current_io_process = wait_queue->processes[i];
            if (current_io_process != NULL) {
                if (current_io_process->remaining_io > 0) {
                    current_io_process->remaining_io -= TIME_QUANTUM;
                    current_io_process->remaining_io = (current_io_process->remaining_io <= 0) ? 0 : current_io_process->remaining_io;
                    fprintf(fp, "Process %d: I/O burst decreased to %d\n", current_io_process->pid, current_io_process->remaining_io);
                }
            }
            i = (i + 1) % MAX_QUEUE_SIZE;
        }

        // I/O가 완료된 프로세스 처리
        while (!isEmpty(wait_queue)) {
            Process* front_process = wait_queue->processes[wait_queue->front];
            if (front_process == NULL || front_process->remaining_io > 0) {
                break;
            }

            front_process = dequeue(wait_queue);
            // 실행 시간을 길게 시뮬레이팅 하기 위해서 다시 큐 혹은 종료
            if (current_time < minute) {
                front_process->cpu_burst = front_process->remaining_cpu = rand() % 10 + 1;
                front_process->io_burst = front_process->remaining_io = rand() % 10 + 1;
                front_process->cpu_use_time += front_process->cpu_burst;
                enqueue(run_queue, front_process);
            } else {
                front_process->running_time = current_time;
                kill(front_process->pid, SIGTERM);
                completed_processes++;
                fprintf(fp, "Process %d completed (total completed: %d)\n", front_process->pid, completed_processes);
            }
        }
    }

    // 2-1. 자식으로부터 메시지를 받아 처리
    MsgBuf msg;
    while (msgrcv(msgq_id, &msg, sizeof(MsgBuf) - sizeof(long), 1, IPC_NOWAIT) != -1) {
        // printf("[Parent] Received message from child PID: %d\n", msg.pid);
        Process* current_process = NULL;
        for (int i=0; i<NUM_PROCESSES; i++) {
            if (processes[i]->pid == msg.pid) {
                current_process = processes[i];
                break;
            }
        }

        if (!current_process) {
            // printf("Unknown process %d received message, ignoring\n", msg.pid);
            continue;
        }

        if (current_process->remaining_cpu <= 0) {
            if (!isFull(wait_queue)) {
                enqueue(wait_queue, current_process);
                fprintf(fp, "Process %d moved to wait queue (CPU completed)\n", current_process->pid);
            }
        } else {
            if (!isFull(run_queue)) {
                enqueue(run_queue, current_process);
                fprintf(fp, "Process %d moved back to run queue\n", current_process->pid);
            }
        }
    }

    // 2-2. 자식으로부터 메시지를 받아 처리 - 10개의 VA처리
    MsgBuf2 msg_from_child;
    MsgBuf2 msg_to_child;
    while (msgrcv(msgq_id_child_to_parent, &msg_from_child, sizeof(MsgBuf2) - sizeof(long), 2, IPC_NOWAIT) != -1) {
        // 자식 프로세스의 PCB 확인
        int index = -1;
        for (int i=0; i<NUM_PROCESSES; i++) {
            if (processes[i]->pid == msg_from_child.pid) {
                index = i;
                break;
            }
        }

        if (index == -1) {
            // printf("Invalid PID %d\n", msg_from_child.pid);
            continue;
        }

        // 10개의 VA처리
        for (int i = 0; i < NUM_MEMORY_ACCESS; i++) {
            int va = msg_from_child.virtual_address[i];
            char type = msg_from_child.type[i];

            // printf("[Parent] Processing VA: %08x, Type: %c\n", va, type);

            if (is_page_fault(pcbs[index].pg_dir, va)) { // 페이지 폴트 처리
                handle_page_fault(pcbs[index].pg_dir, va, msg_from_child.pid);
            }

            int pa = translate_VA_to_PA(pcbs[index].pg_dir, va);
            // printf("Translated VA %08x to PA %08x\n", va, pa);
            fprintf(fp, "Process %d: VA %08x -> PA %08x\n", msg_from_child.pid, va, pa);

            if (type == 'w') {
                char value = msg_from_child.value[i]; // 'write' 작업의 경우 자식으로부터 받은 value를 사용
                RAM[(pa >> 12) & FRAME_NUMBER_MASK].block[pa & PAGE_OFFSET_MASK] = (char)value;
                fprintf(fp, "[Parent] Wrote Value: %c to PA: %08x\n", value, pa);
            } else if (type == 'r') {
                char read_value = RAM[(pa >> 12) & FRAME_NUMBER_MASK].block[pa & PAGE_OFFSET_MASK];
                msg_to_child.value[i] = read_value; // 읽은 값을 메시지에 저장
                fprintf(fp, "[Parent] Read Value: %c from PA: %08x\n", read_value, pa);
            }

            // 메시지에 처리 결과를 저장
            msg_to_child.virtual_address[i] = va;
            msg_to_child.type[i] = type;
        }

        // 부모가 자식에게 처리 결과 전달
        msg_to_child.mtype = msg_from_child.pid;
        msg_to_child.pid = msg_from_child.pid;
        if (msgsnd(msgq_id_parent_to_child, &msg_to_child, sizeof(MsgBuf2) - sizeof(long), 0) == -1) {
            perror("Error sending message to child");
        } else {
            // printf("[Parent] Sent response to child PID: %d\n", msg_to_child.pid);
        }
        fflush(fp);
    }

    // 3. run queue 처리
    if (!isEmpty(run_queue)) {
        Process* current_process = dequeue(run_queue);
        if (current_process != NULL) {
            current_process->remaining_cpu -= TIME_QUANTUM;
            current_process->remaining_cpu = (current_process->remaining_cpu <= 0) ? 0 : current_process->remaining_cpu;
            kill(current_process->pid, SIGUSR1);
        }
    }

    // 런 큐 웨이팅 타임 계산
    int i = run_queue->front;
    while (i != run_queue->rear) {
        Process* process = run_queue->processes[i];
        if (process != NULL) {
            process->waiting_time++;
        }
        i = (i + 1) % MAX_QUEUE_SIZE;
    }
}

void execute_child_task(int pid) {
    // printf("Child process %d CPU ing...\n", pid);

    // 첫 번째 메시지: 간단한 상태 전달
    MsgBuf msg;
    msg.mtype = 1;
    msg.pid = pid;

    // 부모 프로세스로 상태 메시지 전송
    if (msgsnd(msgq_id, &msg, sizeof(MsgBuf) - sizeof(long), 0) == -1) {
        perror("msgsnd failed sending to parent process");
        exit(1);
    }

    // 두 번째 메시지: 가상 주소 및 메모리 접근 정보 전달
    MsgBuf2 msg_to_parent;

    srand(time(NULL) ^ (pid << 16)); // pid를 활용해 시드 값에 고유성을 부여
    // 10개의 가상 주소를 생성하여 메시지에 추가
    // printf("Child process %d is generating virtual addresses...\n", pid);
    for (int i = 0; i < NUM_MEMORY_ACCESS; i++) {
        int va = rand(); // 32비트 가상 주소 생성
        char type = (rand() % 2) == 0 ? 'r' : 'w'; // 랜덤으로 읽기 또는 쓰기 결정

        // 작업 설정
        msg_to_parent.virtual_address[i] = va;
        msg_to_parent.type[i] = type;

        if (type == 'w') {
            msg_to_parent.value[i] = (char)((rand() % 26) + 'a'); // 'w'인 경우만 value를 할당
        } else {
            msg_to_parent.value[i] = 0; // 'r'인 경우 value를 0으로 초기화 (옵션)
        }

        fprintf(fp, "Child %d: VA[%d] = %08x, Value = %c, Type = %c\n",
           pid, i, va, type == 'w' ? msg_to_parent.value[i] : 0, type);
        fflush(fp);
    }
    msg_to_parent.mtype = 2;
    msg_to_parent.pid = pid;
    // 부모에게 메시지 전송 (VA, Value, Type 전송)
    if (msgsnd(msgq_id_child_to_parent, &msg_to_parent, sizeof(MsgBuf2) - sizeof(long), 0) == -1) {
        perror("Failed to send memory access request to parent");
        exit(1);
    }
    
    // 부모로부터 결과 수신
    MsgBuf2 response_from_parent;
    if (msgrcv(msgq_id_parent_to_child, &response_from_parent, sizeof(MsgBuf2) - sizeof(long), pid, 0) != -1) {
        // 부모로부터 받은 결과 출력
        for (int i = 0; i < NUM_MEMORY_ACCESS; i++) {
            int va = response_from_parent.virtual_address[i];
            char type = response_from_parent.type[i];
            char value = response_from_parent.value[i];

            if (type == 'r') { // 읽기 작업일 경우만 value 출력
                fprintf(fp, "Child %d received: VA[%08x], Read Value from Memory = %c, Type = %c\n",
                        pid, va, value, type);
            } else if (type == 'w') { // 쓰기 작업일 경우 value를 출력하지 않음
                fprintf(fp, "Child %d received: VA[%08x], Type = %c\n",
                        pid, va, type);
            }
        }
    }
}

Process* initialize_process(int pid) {
    Process* process = (Process*)malloc(sizeof(Process));
    process->pid = pid;
    process->cpu_burst = rand() % MAX_BURST_TIME + 1;
    process->io_burst = rand() % MAX_BURST_TIME + 1;
    process->remaining_cpu = process->cpu_burst;
    process->remaining_io = process->io_burst;
    process->running_time = 0;
    process->waiting_time = 0;
    process->cpu_use_time = process->cpu_burst;
    return process;
}

void initialize_all_processes(int child_pids[]) {
    srand(time(NULL));
    for (int i = 0; i < NUM_PROCESSES; i++) {
        Process* process = initialize_process(child_pids[i]);
        processes[i] = process;
        enqueue(run_queue, process);
        // printf("Created process %d (CPU burst: %d, I/O burst: %d)\n", 
            //    process->pid, process->cpu_burst, process->io_burst);
    }
}

void handle_sigterm(int signum) {
    // printf("Terminating child processes... %d\n", getpid());
    exit(0);
}

void handle_sigusr1(int signum) {
    execute_child_task(getpid());
}

// 메모리 해제를 위한 cleanup 함수들
void free_page_tables(pcb* process_pcb) {
    if (!process_pcb->pg_dir) {
        return;
    }

    // L2 테이블들 해제
    for (int i = 0; i < NUM_L1_ENTRY; i++) {
        if (process_pcb->pg_dir[i]) {
            free(process_pcb->pg_dir[i]);
            process_pcb->pg_dir[i] = NULL;
        }
    }

    // L1 디렉토리 해제
    free(process_pcb->pg_dir);
    process_pcb->pg_dir = NULL;
}

void cleanup_virtual_memory() {
    // 각 프로세스의 페이지 테이블 해제
    for (int i = 0; i < NUM_PROCESSES; i++) {
        free_page_tables(&pcbs[i]);
    }

    // RAM 해제
    if (RAM) {
        free(RAM);
        RAM = NULL;
    }
}

int main() {
    fp = fopen("swapping.txt", "w");
    if (fp == NULL) {
        perror("Error opening schedule_dump.txt");
        exit(1);
    }   

    setup_msgq();

    run_queue = init_queue();
    wait_queue = init_queue();

    initialize_free_page_list();
    initialize_disk();
    
    for (int i = 0; i < NUM_PROCESSES; i++) { // 10개의 자식 프로세스 생성
        int pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {  // 자식 프로세스
            signal(SIGTERM, handle_sigterm);
            signal(SIGUSR1, handle_sigusr1);
            while (1) {
                pause();
            }
        } else { // 부모 프로세스
            child_pids[i] = pid;  // 부모가 자식의 PID 저장
         
         // PCB와 페이지 테이블 초기화
         initialize_pcb(&pcbs[i], pid);
        //  printf("Initialized PCB for process %d with L1 directory at %p\n", pid, pcbs[i].pg_dir);
        }
    }

    initialize_all_processes(child_pids);
    fprintf(fp, "------------------------------\n");
    fprintf(fp, "Initialized status of elements\n");
    print_queue_status();
    fprintf(fp, "------------------------------\n");

    signal_handler();
    setup_timer();
    while (completed_processes < NUM_PROCESSES) {
        execute_parent_task();
        print_memory_status();
        print_queue_status();
        usleep(TIME_QUANTUM * TIME_TICK * 1000);
    }

    fprintf(fp, "\n\n-------------------------");
    fprintf(fp, "\nFinal running time result\n");
    for (int i=0; i<NUM_PROCESSES; i++) {
        fprintf(fp, "Child[%d] : %fs\n", processes[i]->pid, processes[i]->running_time * TIME_QUANTUM * TIME_TICK / 1000.0);
    }
    fprintf(fp, "-------------------------\n\n");

    fprintf(fp, "\n\n-------------------------");
    fprintf(fp, "\nFinal waiting time result\n");
    for (int i=0; i<NUM_PROCESSES; i++) {
        fprintf(fp, "Child[%d] : %fs\n", processes[i]->pid, processes[i]->waiting_time * TIME_QUANTUM * TIME_TICK / 1000.0);
    }
    fprintf(fp, "-------------------------\n\n");

    fprintf(fp, "\n\n-------------------------");
    fprintf(fp, "\nFinal cpu use time result\n");
    for (int i=0; i<NUM_PROCESSES; i++) {
        fprintf(fp, "Child[%d] : %fs\n", processes[i]->pid, processes[i]->cpu_use_time * TIME_TICK / 1000.0);
    }
    fprintf(fp, "-------------------------\n\n");

    cleanup_virtual_memory();
    msgctl(msgq_id, IPC_RMID, NULL);
    msgctl(msgq_id_parent_to_child, IPC_RMID, NULL);
    msgctl(msgq_id_child_to_parent, IPC_RMID, NULL);

    free(run_queue);
    free(wait_queue);
    fclose(fp);

    return 0;
}