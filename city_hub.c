#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MONITOR_PID_FILE ".monitor_pid"
#define MONITOR_BIN      "./monitor_reports"
#define SCORER_BIN       "/home/kraks/GitHub/SO/scorer"

static void cmd_start_monitor(void) {
    pid_t hub_mon_pid = fork();

    if (hub_mon_pid < 0) {
        perror("city_hub: fork hub_mon");
        return;
    }

    if (hub_mon_pid == 0) {
    
        int pipefd[2];
        if (pipe(pipefd) != 0) {
            perror("hub_mon: pipe");
            exit(1);
        }

        pid_t monitor_pid = fork();

        if (monitor_pid < 0) {
            perror("hub_mon: fork monitor");
            exit(1);
        }

        if (monitor_pid == 0) {
            close(pipefd[0]);                 
            dup2(pipefd[1], STDOUT_FILENO); 
            close(pipefd[1]);                  

            execl(MONITOR_BIN, "monitor_reports", (char *)NULL);
            perror("hub_mon: execl monitor_reports");
            exit(1);
        }

        close(pipefd[1]);

        char buf[512];
        ssize_t n;
        int monitor_done = 0;

        while (!monitor_done &&
               (n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';

            char *line = buf;
            char *newline;
            while ((newline = strchr(line, '\n')) != NULL) {
                *newline = '\0';

                if (strncmp(line, "MSG:", 4) == 0) {
                    printf("[monitor] %s\n", line + 4);
                    fflush(stdout);
                } else if (strncmp(line, "ERR:", 4) == 0) {
                    printf("[monitor ERROR] %s\n", line + 4);
                    printf("[hub_mon] monitor ended due to error.\n");
                    fflush(stdout);
                    monitor_done = 1;
                } else if (strncmp(line, "END:", 4) == 0) {
                    printf("[monitor] %s\n", line + 4);
                    printf("[hub_mon] monitor has shut down.\n");
                    fflush(stdout);
                    monitor_done = 1;
                } else if (strlen(line) > 0) {
                    printf("[monitor] %s\n", line);
                    fflush(stdout);
                }

                line = newline + 1;
            }
        }

        close(pipefd[0]);

        waitpid(monitor_pid, NULL, 0);
        exit(0);
    }

    printf("Monitor started (hub_mon PID %d).\n", (int)hub_mon_pid);
    printf("Monitor output will appear as it arrives.\n");
    fflush(stdout);
}

static void cmd_calculate_scores(char **districts, int n_districts) {
    if (n_districts == 0) {
        printf("Usage: calculate_scores <district> [district2 ...]\n");
        return;
    }

    printf("=== Workload Report ===\n");

    for (int d = 0; d < n_districts; d++) {
        const char *district = districts[d];
        printf("--- District: %s ---\n", district);

        int pipefd[2];
        if (pipe(pipefd) != 0) {
            perror("city_hub: pipe");
            continue;
        }

        pid_t scorer_pid = fork();

        if (scorer_pid < 0) {
            perror("city_hub: fork scorer");
            close(pipefd[0]);
            close(pipefd[1]);
            continue;
        }

        if (scorer_pid == 0) {
            close(pipefd[0]);               
            dup2(pipefd[1], STDOUT_FILENO); 
            close(pipefd[1]);

            execl(SCORER_BIN, "scorer", district, (char *)NULL);
            perror("city_hub: execl scorer");
            exit(1);
        }

        close(pipefd[1]);  

        char buf[1024];
        ssize_t n;
        int got_output = 0;

        while ((n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            char *line = buf;
            char *newline;
            while ((newline = strchr(line, '\n')) != NULL) {
                *newline = '\0';
                if (strlen(line) > 0) {
                    printf("  %s\n", line);
                    got_output = 1;
                }
                line = newline + 1;
            }
        }

        if (!got_output) {
            printf("  (no reports found or district does not exist)\n");
        }

        close(pipefd[0]);
        waitpid(scorer_pid, NULL, 0);
    }

    printf("======================\n");
}

int main(void) {
    printf("city_hub started. Commands: start_monitor, calculate_scores <districts...>, quit\n");

    char line[512];

    while (1) {
        printf("hub> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\ncity_hub exiting.\n");
            break;
        }

        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        char *tokens[32];
        int n_tokens = 0;
        char *tok = strtok(line, " ");
        while (tok && n_tokens < 32) {
            tokens[n_tokens++] = tok;
            tok = strtok(NULL, " ");
        }
        if (n_tokens == 0) continue;

        if (strcmp(tokens[0], "start_monitor") == 0) {
            cmd_start_monitor();

        } else if (strcmp(tokens[0], "calculate_scores") == 0) {
            cmd_calculate_scores(tokens + 1, n_tokens - 1);

        } else if (strcmp(tokens[0], "quit") == 0 ||
                   strcmp(tokens[0], "exit") == 0) {
            printf("city_hub exiting.\n");
            break;

        } else {
            printf("Unknown command: %s\n", tokens[0]);
            printf("Commands: start_monitor, calculate_scores <districts...>, quit\n");
        }
    }

    return 0;
}