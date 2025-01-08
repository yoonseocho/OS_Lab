#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define MAX_STRING_LENGTH 30
#define ASCII_SIZE	256

// char_stat.c의 전역변수 추가
int stat [MAX_STRING_LENGTH];
int stat2 [ASCII_SIZE];
pthread_mutex_t stat_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct sharedobject {
	FILE *rfile;
	int linenum;
	char *line;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int full; // 공유 버퍼(이 경우 한 줄)가 가득 찼는지 비어 있는지 나타내는 flag
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

		while(so->full) pthread_cond_wait(&so->cond, &so->lock);// 공유객체가 꽉차면 wait

		read = getdelim(&line, &len, '\n', rfile); //  input 파일에서 한 줄을 읽어 line 버퍼에 저장, read에는 읽은 문자열의 길이를 반환
		if (read == -1) {
			so->full = 1;
			so->line = NULL; // consumer에게 더 이상 처리할 데이터가 없다는 신호
			
			pthread_cond_signal(&so->cond);
			pthread_mutex_unlock(&so->lock);

			break;
		}
		so->linenum = i;
		so->line = strdup(line); // line 버퍼의 내용을 복사하여 공유객체에 저장
		i++;
		so->full = 1;

		pthread_cond_signal(&so->cond);
		pthread_mutex_unlock(&so->lock);
	}
	free(line); // getdelim()용 버퍼 해제
	printf("Prod_%x: %d lines\n", (unsigned int)pthread_self(), i);
	*ret = i;
	pthread_exit(ret);
}

void *consumer(void *arg) {
	so_t *so = arg;
	int *ret = malloc(sizeof(int));
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

		while(!so->full) pthread_cond_wait(&so->cond, &so->lock);

		line = so->line;
		if (line == NULL) { // if producer ended
			pthread_mutex_unlock(&so->lock);
			pthread_cond_signal(&so->cond);
			break;
		}
		len = strlen(line);
		printf("Cons_%x: [%02d:%02d] %s",
			(unsigned int)pthread_self(), line_count, so->linenum, line);

		char *line_copy = strdup(line);
		cptr = line_copy;

		for (substr = strtok_r(cptr, sep, &brka); substr; substr = strtok_r(NULL, sep, &brka)) {
			size_t length = strlen(substr);
			if (length >= 30) length = 30;

            // pthread_mutex_lock(&stat_lock);
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
			// pthread_mutex_unlock(&stat_lock);
		}
		free(line_copy); // strdup() 복사본 해제
		free(so->line);
		so->line = NULL;
		line_count++;
		so->full = 0;

		pthread_cond_signal(&so->cond);
		pthread_mutex_unlock(&so->lock);
	}
	printf("Cons: %d lines\n", line_count);

	// consumer가 통계 출력
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
	*ret = line_count;
	pthread_exit(ret);
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

	share->rfile = rfile;
	share->line = NULL;
	// share->total_consumers = Ncons; // char_stat.c 코드 추가

	pthread_mutex_init(&share->lock, NULL);
	pthread_cond_init(&share->cond, NULL);

	for (i = 0 ; i < Nprod ; i++)
		pthread_create(&prod[i], NULL, producer, share);
	for (i = 0 ; i < Ncons ; i++)
		pthread_create(&cons[i], NULL, consumer, share);
	printf("main continuing\n");

	for (i = 0 ; i < Ncons ; i++) {
		rc = pthread_join(cons[i], (void **) &ret);
		printf("main: consumer_%d joined with %d\n", i, *ret);
	}
	for (i = 0 ; i < Nprod ; i++) {
		rc = pthread_join(prod[i], (void **) &ret);
		printf("main: producer_%d joined with %d\n", i, *ret);
	}

	pthread_mutex_destroy(&share->lock);
	pthread_cond_destroy(&share->cond);

	pthread_exit(NULL);
	exit(0);
}

