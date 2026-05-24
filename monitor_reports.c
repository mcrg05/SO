#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define MONITOR_PID_FILE ".monitor_pid"

static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_sigint  = 0;

static void handler_sigusr1(int signo) { (void)signo; got_sigusr1 = 1; }
static void handler_sigint(int signo)  { (void)signo; got_sigint  = 1; }

static void emit(const char *type, const char *msg) {
    printf("%s:%s\n", type, msg);
    fflush(stdout);
}

static void current_timestamp(char *buf, size_t n) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", tm_info);
}

static pid_t read_existing_pid(void) {
    int fd = open(MONITOR_PID_FILE, O_RDONLY);
    if (fd < 0) return -1;

    char buf[32];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;

    buf[n] = '\0';
    pid_t pid = (pid_t)atoi(buf);
    return (pid > 0) ? pid : -1;
}

static void write_pid_file(void) {
    int fd = open(MONITOR_PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("monitor: open .monitor_pid"); exit(1); }
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
    write(fd, buf, len);
    close(fd);
}

static void remove_pid_file(void) {
    if (unlink(MONITOR_PID_FILE) != 0 && errno != ENOENT)
        perror("monitor: unlink .monitor_pid");
}

int main(void) {
    pid_t existing = read_existing_pid();
    if (existing > 0) {
        if (kill(existing, 0) == 0) {
            char msg[64];
            snprintf(msg, sizeof(msg),
                "another monitor is already running with PID %d", (int)existing);
            emit("ERROR", msg);
            return 1;
        }
    }

    struct sigaction sa_usr1, sa_int;

    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handler_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa_usr1, NULL) != 0) {
        emit("ERROR", "sigaction SIGUSR1 failed");
        return 1;
    }

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handler_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) != 0) {
        emit("ERROR", "sigaction SIGINT failed");
        return 1;
    }

    write_pid_file();

    char ts[32];
    current_timestamp(ts, sizeof(ts));
    char msg[128];
    snprintf(msg, sizeof(msg), "[%s] monitor started (PID %d)", ts, (int)getpid());
    emit("INFO", msg);

    while (1) {
        pause();

        if (got_sigusr1) {
            got_sigusr1 = 0;
            current_timestamp(ts, sizeof(ts));
            snprintf(msg, sizeof(msg), "[%s] new report has been added", ts);
            emit("NOTIFY", msg);
        }

        if (got_sigint) {
            current_timestamp(ts, sizeof(ts));
            snprintf(msg, sizeof(msg), "[%s] monitor shutting down (SIGINT)", ts);
            emit("EXIT", msg);
            remove_pid_file();
            break;
        }
    }

    return 0;
}