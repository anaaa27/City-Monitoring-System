#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

void start_monitor_logic() {
    int pipefd[2];
    if (pipe(pipefd) == -1) { perror("pipe"); return; }
    pid_t hub_mon = fork();

    if (hub_mon == 0) {
        close(pipefd[1]); 

        pid_t monitor = fork();
        if (monitor == 0) { 
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);
            
            execl("./monitor_reports", "monitor_reports", NULL);
            perror("exec failed");
            exit(1);
        }
        char buf[256];
        ssize_t n;
        while ((n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            if (strncmp(buf, "ERR:", 4) == 0) {
                printf("\n[HUB_MON] Fatal Error: %s", buf + 4);
                break;
            } else if (strncmp(buf, "INF:", 4) == 0) {
                printf("\n[HUB_MON] Status: %s", buf + 4);
            } else if (strncmp(buf, "MSG:", 4) == 0) {
                printf("\n[HUB_MON] Event: %s", buf + 4);
            }
            fflush(stdout);
        }
        printf("\n[HUB_MON] Monitor has disconnected/ended.\n");
        close(pipefd[0]);
        exit(0);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    printf("Monitor process dispatched to background (hub_mon PID: %d)\n", hub_mon);
}

int main() {
    char cmd[100];
    printf("═══ CITY HUB INTERACTIVE SHELL ═══\n");
    while (1) {
        printf("hub> ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        cmd[strcspn(cmd, "\n")] = 0;

        if (strcmp(cmd, "start_monitor") == 0) {
            start_monitor_logic();
        } else if (strcmp(cmd, "exit") == 0) {
            break;
        } else if (strlen(cmd) > 0) {
            printf("Unknown command: %s\n", cmd);
        }
    }
    return 0;
}