#include "scheduling.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

Queue* run_queue;
Queue* wait_queue;
FILE *fp;
int msgq_id;               // IPC 메시지 큐 ID
int current_time = 0;      // 모든 타임틱을 1씩 증가하며 출력용으로 사용
int completed_processes = 0;
pid_t child_pids[NUM_PROCESSES]; // 프로세스 정보를 담을 배열과 자식 PID 배열
Process* processes[NUM_PROCESSES];
int minute = 2000; // 10분

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

// IPC 설정 및 통신 관리를 담당하는 함수
void setup_msgq() {
    // IPC를 위한 메시지 큐 생성
    msgq_id = msgget(KEY, IPC_CREAT | 0666);
    if (msgq_id == -1) {
        perror("msgget failed");
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

    setitimer(ITIMER_REAL, &timer, NULL);             // 타이머 설정 및 시작
}

void execute_parent_task() {
    MsgBuf msg;

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

    // 2. 자식으로부터 메시지를 받아 처리
    while (msgrcv(msgq_id, &msg, sizeof(MsgBuf) - sizeof(long), 1, IPC_NOWAIT) != -1) {
        printf("[Parent] Received message from child PID: %d\n", msg.pid);
        Process* current_process = NULL;
        for (int i=0; i<NUM_PROCESSES; i++) {
            if (processes[i]->pid == msg.pid) {
                current_process = processes[i];
                break;
            }
        }

        if (!current_process) {
            printf("Unknown process %d received message, ignoring\n", msg.pid);
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
    // 부모에게 보낼 메시지 구성
    MsgBuf msg;
    printf("Child process %d CPU ing...\n", pid);
    msg.mtype = 1;
    msg.pid = pid;

    // 부모 프로세스로 상태 메시지 전송
    if (msgsnd(msgq_id, &msg, sizeof(MsgBuf) - sizeof(long), 0) == -1) {
        perror("msgsnd failed sending to parent process");
        exit(1);
    }
    // 실제로 해당 시간 동안 처리하는 것으로 가정
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
        printf("Created process %d (CPU burst: %d, I/O burst: %d)\n", 
               process->pid, process->cpu_burst, process->io_burst);
    }
}

void handle_sigterm(int signum) {
    printf("Terminating child processes... %d\n", getpid());
    exit(0);
}

void handle_sigusr1(int signum) {
    execute_child_task(getpid());
}

int main() {
    fp = fopen("schedule_dump.txt", "w");
    if (fp == NULL) {
        perror("Error opening schedule_dump.txt");
        exit(1);
    }   

    setup_msgq();

    run_queue = init_queue();
    wait_queue = init_queue();
    
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

    msgctl(msgq_id, IPC_RMID, NULL);

    free(run_queue);
    free(wait_queue);
    fclose(fp);

    return 0;
}
