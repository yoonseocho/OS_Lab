#ifndef _SCHEDULING_H_
#define _SCHEDULING_H_

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <sys/time.h>

#define MAX_QUEUE_SIZE 11
#define MAX_BURST_TIME 10
#define NUM_PROCESSES 10
#define TIME_QUANTUM 3 // TIME_QUANTUM * TIME_TICK = 300ms로 가정
#define TIME_TICK 100 // ms
#define KEY 12345

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

// process 관련
void execute_parent_task();
void execute_child_task(int pid); // 자식 프로세스
Process* initialize_process(int pid);
void initialize_all_processes(int child_pids[]);
void handle_sigterm(int signum);
void handle_sigusr1(int signum);
void signal_handler();

extern Queue* run_queue;
extern Queue* wait_queue;
extern MsgBuf msg;
extern FILE* fp;
extern int msgq_id;
extern int current_time;
extern int completed_processes;
extern pid_t child_pids[NUM_PROCESSES];
extern Process* processes[NUM_PROCESSES];

#endif