#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARG_SIZE 64
#define MAX_PATH_LENGTH 1024

pid_t child_pid = -1;

void print_banner();
void print_prompt();
void sigint_handler(int sig);
char* find_command(const char* command);
void handle_piped_commands(char *commands[MAX_ARG_SIZE][MAX_ARG_SIZE], int num_commands);
void handle_internal_command(char **args);
void handle_external_command(char **args);
void execute_command(char *command);


void print_banner(){
    printf(" _       __     __                             __           _____ _ _____ __  __\n");
    printf("| |     / /__  / /________  ____ ___  ___     / /_____     / ___/(_) ___// / / /\n");
    printf("| | /| / / _ \\/ / ___/ __ \\/ __ `__ \\/ _ \\   / __/ __ \\    \\__ \\/ /\\__ \\/ /_/ / \n");
    printf("| |/ |/ /  __/ / /__/ /_/ / / / / / /  __/  / /_/ /_/ /   ___/ / /___/ / __  /  \n");
    printf("|__/|__/\\___/_/\\___/\\____/_/ /_/ /_/\\___/   \\__/\\____/   /____/_//____/_/ /_/   \n");
    printf("                                                                                \n\n");
}

void print_prompt(){
    char* user = getenv("USER");
    char cwd[1024];
    
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s@SiSH:%s$ ", user, cwd);
    } else {
        printf("%s@SiSH$ ", user);
    }
}

void sigint_handler(int sig) {
    if (child_pid != -1) {
        // 자식 프로세스가 실행 중이면 종료
        kill(child_pid, SIGTERM);
    } else {
        // 자식 프로세스가 없으면 새 프롬프트 출력
        printf("\n");
        print_prompt();
        fflush(stdout);
    }
}

char* find_command(const char* command) {
    char* path = getenv("PATH");
    if (!path) return NULL;

    char* path_copy = strdup(path);
    char* dir = strtok(path_copy, ":");
    static char full_path[MAX_PATH_LENGTH];

    while (dir != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);
        if (access(full_path, X_OK) == 0) {
            // printf("Command '%s' found in: %s\n", command, dir);  // 명령어를 찾은 경우에만
            free(path_copy);
            return full_path;
        }
        dir = strtok(NULL, ":");
    }

    // printf("Command '%s' not found in PATH\n", command);  // 명령어를 찾지 못한 경우 출력
    free(path_copy);
    return NULL;
}

void handle_piped_commands(char *commands[MAX_ARG_SIZE][MAX_ARG_SIZE], int num_commands) {
    int pipefd[2 * (num_commands - 1)];
    pid_t pids[MAX_ARG_SIZE];
    extern char **environ;

    // 파이프 생성 (명령어 수 - 1 만큼)
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipefd + i * 2) == -1) {
            perror("pipe error");
            exit(1);
        }
    }

    // 각 명령어에 대해 프로세스 생성 및 파이프 연결
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("fork error");
            exit(1);
        }

        if (pids[i] == 0) {  // 자식 프로세스
            // 첫 번째 명령어가 아니면 이전 파이프로부터 입력 받기
            if (i != 0) {
                dup2(pipefd[(i - 1) * 2], STDIN_FILENO);
            }

            // 마지막 명령어가 아니면 다음 파이프로 출력하기
            if (i != num_commands - 1) {
                dup2(pipefd[i * 2 + 1], STDOUT_FILENO);
            }

            // 모든 파이프 닫기
            for (int j = 0; j < 2 * (num_commands - 1); j++) {
                close(pipefd[j]);
            }

            // 명령어의 절대 경로를 찾아서 execve로 실행
            char *cmd_path = find_command(commands[i][0]);
            if (cmd_path == NULL) {
                fprintf(stderr, "Command not found: %s\n", commands[i][0]);
                exit(1);
            }

            // execve로 명령어 실행
            if (execve(cmd_path, commands[i], environ) == -1) {
                perror("Error executing command");
                exit(1);
            }
        }
    }

    // 부모 프로세스는 모든 파이프 닫기
    for (int i = 0; i < 2 * (num_commands - 1); i++) {
        close(pipefd[i]);
    }

    // 모든 자식 프로세스 종료 대기
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

void handle_internal_command(char **args) {
    int num_commands = 0;
    char *commands[MAX_ARG_SIZE][MAX_ARG_SIZE];  // 명령어 리스트
    int arg_index = 0;

    // 파이프가 포함된 명령어인지 확인하고 명령어를 분리
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            args[i] = NULL;  // 파이프 기호를 NULL로 바꿔 명령어 분리
            commands[num_commands][arg_index] = NULL;  // 인자 끝에 NULL 추가
            num_commands++;
            arg_index = 0;
        } else {
            commands[num_commands][arg_index++] = args[i];
        }
    }
    commands[num_commands][arg_index] = NULL;  // 마지막 명령어에 대해 인자 끝에 NULL 추가
    num_commands++;

    if (num_commands > 1) {
        // 파이프가 있는 경우 처리
        handle_piped_commands(commands, num_commands);
    } else{
        if (strcmp(args[0], "cd") == 0) {
            char *path = args[1];
            if (path == NULL || strcmp(path, "~") == 0) {
                path = getenv("HOME");
            }
            if (chdir(path) != 0) {
                perror("chdir failed");
            }
        } else if (strcmp(args[0], "exit") == 0) {
            printf("Exiting SiSH...\n");
            exit(0);
        } else if (args[0][0] == '$') {
            char* env_value = getenv(args[0] + 1);
            if (env_value) {
                printf("%s\n", env_value);
            } else {
                printf("Environment variable not found\n");
            }
        } else {
            printf("Unknown internal command: %s\n", args[0]);
        }
    }
}

void handle_external_command(char **args) {
    char *full_path;
    extern char **environ;

    full_path = find_command(args[0]);
    if (full_path == NULL) {
        if (access(args[0], X_OK) == 0) {
            full_path = args[0];  // Use the command as-is if it's an executable in the current directory
            // printf("Executing '%s' from current directory\n", args[0]);
        } else {
            fprintf(stderr, "Command not found: %s\n", args[0]);
            return;
        }
    }

    child_pid = fork();
    if (child_pid == -1) {
        perror("fork error");
    }
    else if (child_pid == 0) { // child
        signal(SIGINT, SIG_DFL);
        if (execve(full_path, args, environ) == -1) {
            perror("Error executing command");
            exit(1);
        }
    } else { // parent
        int status;
        waitpid(child_pid, &status, 0);
        child_pid = -1; 
    }
}

void execute_command(char *command) {
    char *args[MAX_ARG_SIZE];
    char *token;
    int i = 0;

    // Tokenize the command
    token = strtok(command, " ");
    while (token != NULL && i < MAX_ARG_SIZE - 1) {
        // 작은 따옴표가 있는 경우 제거
        if (token[0] == '\'' && token[strlen(token) - 1] == '\'' ||
            (token[0] == '\"' && token[strlen(token) - 1] == '\"')) {
            token[strlen(token) - 1] = '\0';  // 끝의 따옴표 제거
            token++;  // 시작 부분의 따옴표 건너뜀
        }
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    // 파이프가 있는지 확인
    for (int j = 0; j < i; j++) {
        if (strcmp(args[j], "|") == 0) {
            // 파이프가 포함된 경우, 명령어를 나누어 파이프 처리 함수로 전달
            handle_internal_command(args);
            return;
        }
    }

    // Check if it's an internal command
    if (args[0] != NULL && (strcmp(args[0], "cd") == 0 || strcmp(args[0], "exit") == 0 || args[0][0] == '$')) {
        handle_internal_command(args);
    } else {
        handle_external_command(args);
    }
}

int main(int argc, char *argv[]) {
    char input[MAX_INPUT_SIZE];

    // SIGINT 핸들러 설정
    signal(SIGINT, sigint_handler);

    print_banner();

    while (1) {
        print_prompt();

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        // Remove newline character
        input[strcspn(input, "\n")] = 0;

        // Check for quit command
        if (strcmp(input, "quit") == 0) {
            break;
        }

        // Execute the command
        execute_command(input);
    }

    printf("Exiting SiSH...\n");
    return 0;
}