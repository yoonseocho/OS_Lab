#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#define MAX_STRING_LENGTH 30
#define ASCII_SIZE	256


int stat [MAX_STRING_LENGTH]; // 단어 길이의 빈도수
int stat2 [ASCII_SIZE]; // ASCII 문자에 대한 빈도수


int main(int argc, char *argv[]) {
	int i = 0;
	int rc = 0; // return code
	int sum = 0;
	int line_num = 1;
	char *line = NULL;
	size_t length = 0;
	FILE *rfile = NULL;

	if (argc == 1) {
		printf("usage: ./stat <filename>\n");
		exit(0);
	}

	// Open argv[1] file
	rfile = fopen((char *) argv[1], "rb");
	if (rfile == NULL) {
		perror(argv[1]);
		exit(0);
	}

	// initialize stat
	memset(stat, 0, sizeof(stat));
	memset(stat2, 0, sizeof(stat));

	while (1) {
		char *cptr = NULL; //character pointer - 문자열 순회
		char *substr = NULL; //subString - 부분 문자열을 가리키는 포인터
		char *brka = NULL; // break - strtok_r() 내부 상태 유지
		char *sep = "{}()[],;\" \n\t^";

		// For each line,
		rc = getdelim(&line, &length, '\n', rfile); // 한 줄 전체 읽기(동적 메모리 할당)
		if (rc == -1) break;

		cptr = line;
#ifdef _IO_
		printf("[%3d] %s\n", line_num++, line);
#endif
		for (substr = strtok_r(cptr, sep, &brka); substr; substr = strtok_r(NULL, sep, &brka)) {
			length = strlen(substr);
			// update stats

			// length of the sub-string
			if (length >= 30) length = 30;
#ifdef _IO_
			printf("length: %d\n", (int)length);
#endif
			stat[length-1]++;

			// number of the character in the sub-string
			for (int i = 0 ; i < length ; i++) {
				if (*cptr < 256 && *cptr > 1) {
					stat2[*cptr]++;
#ifdef _IO_
					printf("# of %c(%d): %d\n", *cptr, *cptr, stat2[*cptr]);
#endif
				}
				cptr++;
			}
			cptr++;
			if (*cptr == '\0') break;
		}
	}

	// sum
	sum = 0;
	for (i = 0 ; i < 30 ; i++) {
		sum += stat[i];
	}
	// print out distributions
	printf("*** print out distributions *** \n");
	printf("  #ch  freq \n");
	for (i = 0 ; i < 30 ; i++) {
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

	if (line != NULL) free(line);	
	// Close the file
	fclose(rfile);

	return 0;
}
