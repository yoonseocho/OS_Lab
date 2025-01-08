#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define BUFFER_SIZE 1000  // 버퍼 크기 정의
#define MAX_STRING_LENGTH 30
#define ASCII_SIZE	256

// char_stat.c의 전역변수 추가
int stat [MAX_STRING_LENGTH];
int stat2 [ASCII_SIZE];
pthread_mutex_t stat_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct sharedobject {
	FILE *rfile;
	char **lines; // 여러 줄을 저장할 수 있는 배열
	int linenum;
	int count; // 현재 버퍼에 있는 항목 수
	pthread_mutex_t lock;
	pthread_cond_t has_data;     // 버퍼에 데이터가 있음을 알리는 조건 변수
    pthread_cond_t has_space;    // 버퍼에 공간이 있음을 알리는 조건 변수
    int done;  // producer가 모든 파일을 다 읽었는지를 나타내는 flag
} so_t;

void *producer(void *arg) {
	so_t *so = arg;
	int *ret = malloc(sizeof(int)); // producer가 처리한 총 라인 수
	FILE *rfile = so->rfile;
	int i = 0;
	char *line = NULL;
	size_t len = 0;
	ssize_t read = 0;

	while (1) {
		pthread_mutex_lock(&so->lock);

		while(so->count == BUFFER_SIZE) pthread_cond_wait(&so->has_space, &so->lock);// 버퍼가 가득 찼으면 대기

		read = getdelim(&line, &len, '\n', rfile); //  input 파일에서 한 줄을 읽어 line 버퍼에 저장, read에는 읽은 문자열의 길이를 반환
		if (read == -1) {
			so->done = 1;
			
			pthread_cond_broadcast(&so->has_data); // 모든 consumer에게 알림
			pthread_mutex_unlock(&so->lock);

			break;
		}
		so->linenum = i;
		so->lines[so->count] = strdup(line); // line 버퍼의 내용을 복사하여 공유객체에 저장
		i++;
		so->count++;

		pthread_cond_broadcast(&so->has_data);
		pthread_mutex_unlock(&so->lock);
	}
	free(line); // getdelim()용 버퍼 해제
	printf("Prod_%x: %d lines\n", (unsigned int)pthread_self(), i);
	*ret = i;
	pthread_exit(ret);
}

void *consumer(void *arg) {
	so_t *so = arg;
	int *ret = malloc(sizeof(int)); // consumer가 처리한 총 라인 수
	int line_count = 0;
	int len;
	char *line;

	//char_stat.c 변수 추가
	char *cptr = NULL;
	char *substr = NULL;
	char *brka = NULL;
	char *sep = "{}()[],;\" \n\t^";

	while (1) {
		pthread_mutex_lock(&so->lock);

		while(so->count == 0){
			if(so->done){
				pthread_mutex_unlock(&so->lock);
				*ret = line_count;
				printf("Cons_%x: %d lines\n", (unsigned int)pthread_self(), line_count);
				pthread_exit(ret);
			}
			pthread_cond_wait(&so->has_data, &so->lock);
		}

		
		line = strdup(so->lines[so->count - 1]); //버퍼에서 데이터 가져오기

		printf("Cons_%x: [%02d:%02d] %s",
			(unsigned int)pthread_self(), line_count, so->linenum, line);

		free(so->lines[so->count-1]);  // 버퍼의 해당 위치 메모리 해제
		so->count--;
		
		pthread_cond_signal(&so->has_space);
		pthread_mutex_unlock(&so->lock);

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
		free(line); // 데이터 처리가 끝난 후 메모리 해제
		line_count++;
	}
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
	so_t *share = malloc(sizeof(so_t));
	memset(share, 0, sizeof(so_t));

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

	// lines 배열 초기화
    share->lines = malloc(BUFFER_SIZE * sizeof(char*));
    if (share->lines == NULL) {
        perror("malloc");
        exit(1);
    }

	share->rfile = rfile;
	share->count = 0;
	share->done = 0;

	pthread_mutex_init(&share->lock, NULL);
	pthread_cond_init(&share->has_data, NULL);
	pthread_cond_init(&share->has_space, NULL);

	for (i = 0 ; i < Nprod ; i++)
		pthread_create(&prod[i], NULL, producer, share);
	for (i = 0 ; i < Ncons ; i++)
		pthread_create(&cons[i], NULL, consumer, share);
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

	// 자원 정리
    for (i = 0; i < share->count; i++) {
        free(share->lines[i]);
    }
    free(share->lines);

	pthread_mutex_destroy(&share->lock);
	pthread_cond_destroy(&share->has_data);
	pthread_cond_destroy(&share->has_space);

	fclose(rfile);
    free(share);

	pthread_exit(NULL);
	exit(0);
}