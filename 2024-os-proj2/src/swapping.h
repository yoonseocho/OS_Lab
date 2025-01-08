#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>

#define MAX_QUEUE_SIZE 11
#define MAX_BURST_TIME 10
#define NUM_PROCESSES 10
#define TIME_QUANTUM 3 // TIME_QUANTUM * TIME_TICK = 300ms로 가정
#define TIME_TICK 100 // ms
#define KEY 123456

#define PAGE_SIZE 4096              // 4KB
#define L1_INDEX_MASK 0xfff         // 12비트
#define L2_INDEX_MASK 0xff          // 8비트
#define PAGE_OFFSET_MASK 0xfff      // 12비트
#define TOTAL_PAGE_SIZE 1048576     // 2^20
#define NUM_L1_ENTRY 4096           // 2^12
#define NUM_L2_ENTRY 256            // 2^8
#define BLOCK_TO_CHAR 4096          // 4KB/CHAR
#define NUM_MEMORY_ACCESS 200
#define VALID_BIT_MASK 0x80000000   // MSB (유효 비트)
#define FRAME_NUMBER_MASK 0x000FFFFF// 하위 20비트 (프레임 번호)

#define RAM_DISK_BIT_MASK 0x40000000// 상위 두 번째 비트로 RAM(0)과 DISK(1) 구분
#define RAM_FRAME_COUNT (TOTAL_PAGE_SIZE / 4)  // RAM은 전체의 1/4

// 프로세스 구조체 정의
typedef struct {
    int pid;
    int cpu_burst;
    int io_burst;
    int remaining_cpu;
    int remaining_io;
    int running_time;
    int waiting_time;
    int cpu_use_time;
} Process;

// 큐 구조체 정의
typedef struct {
    Process* processes[MAX_QUEUE_SIZE];
    int front;
    int rear;
} Queue;

// IPC 메시지 구조체
typedef struct {
    long mtype;
    int pid;
} MsgBuf;

typedef struct { // P -> C
    long mtype;
    int pid;
    int virtual_address[NUM_MEMORY_ACCESS]; // 0x123455
    unsigned char value[NUM_MEMORY_ACCESS]; // 'a'
    unsigned char type[NUM_MEMORY_ACCESS]; // 'w', 'r'
} MsgBuf2;
// {0x123456, 100, 'w'}
// {0x123455, 0, 'r'}


// Virtual Memory(Paging) 관련 구조체 선언
typedef struct {
    int pid;
    int** pg_dir; //TTBR
} pcb;

typedef struct {
    char block[BLOCK_TO_CHAR];
} frame;

// queue 관련
Queue* init_queue();
int isEmpty(Queue* queue);
int isFull(Queue* queue);
void enqueue(Queue* queue, Process* process);
Process* dequeue(Queue* queue);

// IPC 관련
void setup_msgq();
void alrm_handler(int signum);
void signal_handler(); // 부모 프로세스
void setup_timer();

// 디버깅 관련
void print_queue_status();

// Virtual Memory(Paging) 관련 함수
void initialize_free_page_list();
int allocate_frame();
void initialize_pcb(pcb* process_pcb, int pid);
int translate_VA_to_PA(int** L1, int va);
bool is_page_fault(int** L1, int va);
void handle_page_fault(int** L1, int va, int pid);

void initialize_disk();
void swap_out();
int swap_in(int disk_frame);
int find_oldest_frame();
void print_memory_status();


// process 관련
void execute_parent_task();
void execute_child_task(int pid); // 자식 프로세스
Process* initialize_process(int pid);
void initialize_all_processes(int child_pids[]);
void update_process_state(int pid, int remaining_cpu, int remaining_io);
void handle_sigterm(int signum);
void handle_sigusr1(int signum);
void signal_handler();


extern Queue* run_queue;
extern Queue* wait_queue;
extern MsgBuf msg;
extern FILE* fp;
extern int msgq_id;
extern int msgq_id_parent_to_child;
extern int msgq_id_child_to_parent;
extern int current_time;
extern int completed_processes;
extern pid_t child_pids[NUM_PROCESSES];
extern Process* processes[NUM_PROCESSES];

// Virtual Memory(Paging) 관련 배열 정의
extern pcb pcbs[NUM_PROCESSES];
extern frame* RAM;
extern bool* free_page_list;
extern int ram_usage_timestamps[RAM_FRAME_COUNT];
extern frame* DISK;
extern bool* free_disk_list;