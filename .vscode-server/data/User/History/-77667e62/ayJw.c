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

void handle_internal_command(char **args) {
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

char* find_command(const char* command) {
    char* path = getenv("PATH");
    if (!path) return NULL;

    char* path_copy = strdup(path);
    char* dir = strtok(path_copy, ":");
    static char full_path[MAX_PATH_LENGTH];

    while (dir != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return full_path;
        }
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return NULL;
}

void handle_external_command(char **args) {
    char *full_path;
    extern char **environ;

    full_path = find_command(args[0]);
    if (full_path == NULL) {
        if (access(args[0], X_OK) == 0) {
            full_path = args[0];  // Use the command as-is if it's an executable in the current directory
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
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

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
        if (strcmp(input, "quit") == 0) { //|| strcmp(input, "exit") == 0
            break;
        }

        // Execute the command
        execute_command(input);
    }

    printf("Exiting SiSH...\n");
    return 0;
}