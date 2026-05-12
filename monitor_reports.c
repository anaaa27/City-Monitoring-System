#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#define PID_FILE ".monitor_pid"

void handle_sigint(int sig) {
    unlink(PID_FILE);
    exit(0);
}

void handle_sigusr1(int sig) {
    const char *msg = "MSG:New report added to the system!\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

int main() {
    int pid_fd = open(PID_FILE, O_RDONLY);
    if (pid_fd >= 0) {
        char pid_buf[16] = {0};
        read(pid_fd, pid_buf, 15);
        close(pid_fd);
        printf("ERR:Already running with PID %s\n", pid_buf);
        fflush(stdout);
        exit(1); 
    }

 
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    char pid_str[16];
    sprintf(pid_str, "%d", getpid());
    write(fd, pid_str, strlen(pid_str));
    close(fd);

    struct sigaction sa_int = {.sa_handler = handle_sigint};
    sigaction(SIGINT, &sa_int, NULL);
    struct sigaction sa_usr = {.sa_handler = handle_sigusr1};
    sigaction(SIGUSR1, &sa_usr, NULL);


    printf("INF:Started successfully with PID %d\n", getpid());
    fflush(stdout);

    while(1) pause();
    return 0;
}