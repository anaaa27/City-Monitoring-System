#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

#define PID_FILE ".monitor_pid"

void handle_sigint(int sig) {
    printf("\n[Monitor] Received SIGINT. Cleaning up and exiting...\n");
    unlink(PID_FILE);
    exit(0);
}

void handle_sigusr1(int sig) {
    write(STDOUT_FILENO, "[Monitor] Notification: A new report was added!\n", 48);
}

int main() {
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open pid file"); exit(1); }
    
    char pid_str[10];
    int len = sprintf(pid_str, "%d", getpid());
    write(fd, pid_str, len);
    close(fd);

    printf("[Monitor] Started with PID: %d. Waiting for signals...\n", getpid());
    struct sigaction sa_int, sa_usr1;

    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    while(1) {
        pause(); 
    }

    return 0;
}

