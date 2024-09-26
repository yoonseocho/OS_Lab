#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <termios.h>
#include <dirent.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARG_COUNT 100
#define MAX_PATH_LENGTH 1024

void printPrompt() {
    char* user = getenv("USER");
    char cwd[1024];
    char buffer[80];
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s@SiSH:%s$ ", user, cwd);
    } else {
        printf("%s@SiSH$ ", user);
    }
    fflush(stdout);
}

char* findExecutable(const char* cmd) {
    char* path = getenv("PATH");
    char* path_copy = strdup(path);
    char* dir = strtok(path_copy, ":");
    static char full_path[MAX_PATH_LENGTH];

    while (dir != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return full_path;
        }
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return NULL;
}

char* tabComplete(const char* partial) {
    DIR* dir;
    struct dirent* entry;
    static char completed[MAX_PATH_LENGTH];
    int match_count = 0;
    char* longest_match = NULL;

    dir = opendir(".");
    if (dir == NULL) {
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(partial, entry->d_name, strlen(partial)) == 0) {
            match_count++;
            if (match_count == 1) {
                longest_match = strdup(entry->d_name);
            } else {
                int i;
                for (i = 0; longest_match[i] && entry->d_name[i] && longest_match[i] == entry->d_name[i]; i++);
                longest_match[i] = '\0';
            }
        }
    }

    closedir(dir);

    if (match_count > 0 && longest_match != NULL) {
        strcpy(completed, longest_match);
        free(longest_match);
        return completed;
    }

    return NULL;
}

int main() {
    char input[MAX_INPUT_SIZE];
    char *args[MAX_ARG_COUNT];
    char *token;
    char *saveptr;
    pid_t pid;
    int status;
    extern char **environ;
    struct termios old_termios, new_termios;

    tcgetattr(STDIN_FILENO, &old_termios);
    new_termios = old_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    while (1) {
        printPrompt();
        
        int i = 0;
        char c;
        while (i < MAX_INPUT_SIZE - 1) {
            read(STDIN_FILENO, &c, 1);
            if (c == '\n') {
                input[i] = '\0';
                printf("\n");
                break;
            } else if (c == '\t') {
                input[i] = '\0';
                char* last_space = strrchr(input, ' ');
                char* partial = last_space ? last_space + 1 : input;
                char* completion = tabComplete(partial);
                if (completion != NULL) {
                    if (last_space) {
                        strcpy(last_space + 1, completion);
                    } else {
                        strcpy(input, completion);
                    }
                    i = strlen(input);
                    printf("\r");
                    printPrompt();
                    printf("%s", input);
                    fflush(stdout);
                }
            } else if (c == 127) { // Backspace
                if (i > 0) {
                    i--;
                    printf("\b \b");
                    fflush(stdout);
                }
            } else {
                input[i++] = c;
                putchar(c);
                fflush(stdout);
            }
        }

        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            printf("Exiting SiSH...\n");
            break;
        }

        i = 0;
        token = strtok_r(input, " ", &saveptr);
        while (token != NULL && i < MAX_ARG_COUNT - 1) {
            args[i] = token;
            token = strtok_r(NULL, " ", &saveptr);
            i++;
        }
        args[i] = NULL;

        if (args[0] == NULL) {
            continue;
        }

        if (args[0][0] == '$') {
            char* env_value = getenv(args[0]+1);
            if (env_value) {
                printf("%s\n", env_value);
            } else {
                printf("Environment variable not found\n");
            }
            continue;
        }

        if (strcmp(args[0], "cd") == 0) {
            char *path = args[1];
            if (path == NULL || strcmp(path, "~") == 0) {
                path = getenv("HOME");
            }

            if (chdir(path) != 0) {
                perror("chdir failed");
            }
            continue;
        }

        pid = fork();

        if (pid == -1) {
            perror("fork failed");
            exit(1);
        } else if (pid == 0) {
            char* cmd_path = findExecutable(args[0]);
            if (cmd_path != NULL) {
                if (execve(cmd_path, args, environ) == -1) {
                    perror("execve failed");
                    exit(1);
                }
            } else {
                printf("Command not found: %s\n", args[0]);
                exit(1);
            }
        } else {
            wait(&status);
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);

    return 0;
}
