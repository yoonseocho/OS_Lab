#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define QUEUE_SIZE 1000  // 원형 큐 크기 정의
#define MAX_STRING_LENGTH 30
#define ASCII_SIZE	256
#define K 5  // 공유 객체(큐)의 수

// char_stat.c의 전역변수 추가
int stat [MAX_STRING_LENGTH];
int stat2 [ASCII_SIZE];
pthread_mutex_t stat_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct sharedobject {
	FILE *rfile;
	int linenum[QUEUE_SIZE]; // 각 줄의 번호를 저장하는 큐 - 각 데이터가 파일의 몇 번째 라인인지 추적
    char **lines; // 여러 줄을 저장할 수 있는 큐
    int front;  // 큐의 시작점 (데이터를 소비할 위치)
    int rear;   // 큐의 끝점 (데이터를 추가할 위치)
    int count;  // 현재 큐에 있는 항목 수 (큐가 가득 찼는지 확인하는 데 사용)
	pthread_mutex_t lock;
	pthread_cond_t prod_cond;
	pthread_cond_t cons_cond;
} so_t;

so_t shared_objects[K];  // K개의 공유 객체 배열
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;
int global_done = 0;
pthread_mutex_t global_done_lock = PTHREAD_MUTEX_INITIALIZER;

int next_producer_so = 0;
int next_consumer_so = 0;
pthread_mutex_t so_index_lock = PTHREAD_MUTEX_INITIALIZER;

void *producer(void *arg) {
	int producer_id = *(int*)arg;
	int current_so;  // 각 producer가 시작할 공유 객체 인덱스
	int *ret = malloc(sizeof(int)); // producer가 처리한 총 라인 수
	int i = 0;
	char *line = NULL;
	size_t len = 0;
	ssize_t read = 0;

	while (1) {
        pthread_mutex_lock(&so_index_lock);
        current_so = next_producer_so;
        next_producer_so = (next_producer_so + 1) % K;
        pthread_mutex_unlock(&so_index_lock);

        so_t *so = &shared_objects[current_so];

        pthread_mutex_lock(&so->lock); // 각 공유 버퍼에 대한 보호


		while(so->count == QUEUE_SIZE && !global_done) pthread_cond_wait(&so->prod_cond, &so->lock);// 버퍼가 가득 찼으면 대기

		if (global_done) {
            pthread_mutex_unlock(&so->lock);
            break;
        }
		
		pthread_mutex_lock(&file_lock); // 파일 읽기에 대한 보호
		read = getdelim(&line, &len, '\n', so->rfile); //  input 파일에서 한 줄을 읽어 line 버퍼에 저장, read에는 읽은 문자열의 길이를 반환
		pthread_mutex_unlock(&file_lock);

		if (read == -1) {
			pthread_mutex_lock(&global_done_lock);
            global_done = 1;
            pthread_mutex_unlock(&global_done_lock);

            pthread_cond_broadcast(&so->cons_cond); // 모든 consumer에게 알림
			pthread_mutex_unlock(&so->lock);
			break;
		}
		
        so->linenum[so->rear] = i;  // 큐의 끝에 라인 번호 저장
        so->lines[so->rear] = strdup(line);  // 큐의 끝에 데이터를 복사
        so->rear = (so->rear + 1) % QUEUE_SIZE;  // rear를 원형 큐 방식으로 이동
        so->count++;  // 큐의 항목 수 증가
        i++;

		pthread_cond_signal(&so->cons_cond);
		pthread_mutex_unlock(&so->lock);
	}
	free(line); // getdelim으로 할당된 메모리 해제
	printf("Prod_%x: %d lines\n", (unsigned int)pthread_self(), i);
	*ret = i;
	pthread_exit(ret);
}

void *consumer(void *arg) {
	int consumer_id = *(int*)arg;
	int current_so;
	int *ret = malloc(sizeof(int)); // consumer가 처리한 총 라인 수
	int line_count = 0;

	//char_stat.c 변수 추가
	char *cptr = NULL;
	char *substr = NULL;
	char *brka = NULL;
	char *sep = "{}()[],;\" \n\t^";

	while (1) {
        pthread_mutex_lock(&so_index_lock);
        current_so = next_consumer_so;
        next_consumer_so = (next_consumer_so + 1) % K;
        pthread_mutex_unlock(&so_index_lock);
        
		so_t *so = &shared_objects[current_so];

		pthread_mutex_lock(&so->lock);

		while(so->count == 0 && !global_done) pthread_cond_wait(&so->cons_cond, &so->lock);

        if (so->count == 0 && global_done) {  // 생산자가 파일 끝에 도달했다는 것을 알림
            pthread_mutex_unlock(&so->lock);
            break;
        }

        char *line = so->lines[so->front];  // 큐의 앞에서 데이터 가져오기
		so->lines[so->front] = NULL;  // 메모리 이중 해제 방지
        int linenum = so->linenum[so->front];
        so->front = (so->front + 1) % QUEUE_SIZE;  // front를 원형 큐 방식으로 이동
        so->count--;  // 큐의 항목 수 감소

        printf("Cons_%x: [%02d:%02d] %s", (unsigned int)pthread_self(), line_count, so->linenum, line);

		char *line_copy = strdup(line);
		cptr = line_copy;

		pthread_mutex_lock(&stat_lock);
		for (substr = strtok_r(cptr, sep, &brka); substr; substr = strtok_r(NULL, sep, &brka)) {
			size_t length = strlen(substr);
			if (length >= 30) length = 30;

			stat[length-1]++;

			// number of the character in the sub-string
			for (int i = 0 ; i < length ; i++) {
				if (*cptr < 256 && *cptr > 1) {
					stat2[*cptr]++;
				}
				cptr++;
			}
			cptr++;
			if (*cptr == '\0') break;
		}
		pthread_mutex_unlock(&stat_lock);
		
		free(line_copy); // strdup() 복사본 해제
		free(line);
		line_count++;

		pthread_cond_signal(&so->prod_cond);
		pthread_mutex_unlock(&so->lock);
	}
	printf("Cons_%x: %d lines\n", (unsigned int)pthread_self(), line_count);
	*ret = line_count;
	pthread_exit(ret);
}

void print_statistics(){
	int sum = 0;
	for (int i = 0 ; i < 30 ; i++) {
		sum += stat[i];
	}
	// print out distributions
	printf("*** print out distributions *** \n");
	printf("  #ch  freq \n");
	for (int i = 0 ; i < 30 ; i++) {
		int j = 0;
		int num_star = stat[i]*80/sum;
		printf("[%3d]: %4d \t", i+1, stat[i]);
		for (j = 0 ; j < num_star ; j++)
			printf("*");
		printf("\n");
	}
	printf("       A        B        C        D        E        F        G        H        I        J        K        L        M        N        O        P        Q        R        S        T        U        V        W        X        Y        Z\n");
	printf("%8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d\n",
			stat2['A']+stat2['a'], stat2['B']+stat2['b'],  stat2['C']+stat2['c'],  stat2['D']+stat2['d'],  stat2['E']+stat2['e'],
			stat2['F']+stat2['f'], stat2['G']+stat2['g'],  stat2['H']+stat2['h'],  stat2['I']+stat2['i'],  stat2['J']+stat2['j'],
			stat2['K']+stat2['k'], stat2['L']+stat2['l'],  stat2['M']+stat2['m'],  stat2['N']+stat2['n'],  stat2['O']+stat2['o'],
			stat2['P']+stat2['p'], stat2['Q']+stat2['q'],  stat2['R']+stat2['r'],  stat2['S']+stat2['s'],  stat2['T']+stat2['t'],
			stat2['U']+stat2['u'], stat2['V']+stat2['v'],  stat2['W']+stat2['w'],  stat2['X']+stat2['x'],  stat2['Y']+stat2['y'],
			stat2['Z']+stat2['z']);
}


int main (int argc, char *argv[])
{
	pthread_t prod[100];
	pthread_t cons[100];
	int Nprod, Ncons;
	int rc;   long t;
	int *ret;
	int i;
	FILE *rfile;
	if (argc == 1) {
		printf("usage: ./prod_cons <readfile> #Producer #Consumer\n");
		exit (0);
	}

	rfile = fopen((char *) argv[1], "r");
	if (rfile == NULL) {
		perror("rfile");
		exit(0);
	}
	if (argv[2] != NULL) {
		Nprod = atoi(argv[2]);
		if (Nprod > 100) Nprod = 100;
		if (Nprod == 0) Nprod = 1;
	} else Nprod = 1;
	if (argv[3] != NULL) {
		Ncons = atoi(argv[3]);
		if (Ncons > 100) Ncons = 100;
		if (Ncons == 0) Ncons = 1;
	} else Ncons = 1;

	int producer_ids[Nprod], consumer_ids[Ncons];

    next_producer_so = 0;
    next_consumer_so = 0;
    pthread_mutex_init(&so_index_lock, NULL);

	// Initialize shared objects
    for (i = 0; i < K; i++) {
        shared_objects[i].rfile = rfile;
        shared_objects[i].lines = malloc(QUEUE_SIZE * sizeof(char*));
		// 각 줄을 저장할 공간을 할당
		for (int j = 0; j < QUEUE_SIZE; j++) {
			shared_objects[i].lines[j] = NULL;  // 초기에는 NULL로 설정
		}
        shared_objects[i].front = 0;
        shared_objects[i].rear = 0;
        shared_objects[i].count = 0;
        pthread_mutex_init(&shared_objects[i].lock, NULL);
        pthread_cond_init(&shared_objects[i].prod_cond, NULL);
		pthread_cond_init(&shared_objects[i].cons_cond, NULL);
    }

	for (i = 0 ; i < Nprod ; i++){
		producer_ids[i] = i;
		pthread_create(&prod[i], NULL, producer, &producer_ids[i]);
	}
	for (i = 0 ; i < Ncons ; i++){
		consumer_ids[i] = i;
		pthread_create(&cons[i], NULL, consumer, &consumer_ids[i]);
	}
	printf("main continuing\n");

	for (i = 0 ; i < Ncons ; i++) {
		rc = pthread_join(cons[i], (void **) &ret);
		printf("main: consumer_%d joined with %d\n", i, *ret);
		free(ret);  // ret 메모리 해제 추가
	}

	for (i = 0 ; i < Nprod ; i++) {
		rc = pthread_join(prod[i], (void **) &ret);
		printf("main: producer_%d joined with %d\n", i, *ret);
		free(ret);  // ret 메모리 해제 추가
	}

	print_statistics();

	// Cleanup
    for (i = 0; i < K; i++) {
		for (int j = 0; j < QUEUE_SIZE; j++) {
            free(shared_objects[i].lines[j]);
        }
		free(shared_objects[i].lines);
		pthread_mutex_destroy(&shared_objects[i].lock);
		pthread_cond_destroy(&shared_objects[i].prod_cond);
		pthread_cond_destroy(&shared_objects[i].cons_cond);
	}

    pthread_mutex_destroy(&so_index_lock);
	pthread_mutex_destroy(&stat_lock);
    pthread_mutex_destroy(&file_lock);
	pthread_mutex_destroy(&global_done_lock);

	fclose(rfile);
	pthread_exit(NULL);
	exit(0);
}