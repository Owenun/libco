/*
 * test3: 100 个请求, 服务端每个都睡 50ms 才回包。
 * 对比 for 循环里的阻塞等待, 和单线程里 100 个协程把等待重叠起来。
 */
#define _GNU_SOURCE
#include "loop.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define N        100
#define DELAY_US 50000

static int   port;
static pid_t srv;

static double now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

static void start_server(void)
{
    struct sockaddr_in a = { .sin_family = AF_INET };
    socklen_t len = sizeof a;

    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port        = 0;

    int ls = socket(AF_INET, SOCK_STREAM, 0);
    bind(ls, (void *)&a, sizeof a);
    listen(ls, 128);
    getsockname(ls, (void *)&a, &len);
    port = ntohs(a.sin_port);

    srv = fork();
    if (srv != 0) return;

    /* 服务端: 一个连接一个进程, 纯粹为了让慢是真的慢 */
    signal(SIGCHLD, SIG_IGN);
    for (;;) {
        int fd = accept(ls, NULL, NULL);
        if (fd < 0) continue;
        if (fork() == 0) {
            usleep(DELAY_US);
            if (write(fd, "ok\n", 3) != 3) _exit(1);
            _exit(0);
        }
        close(fd);
    }
}

static int connect_local(int nonblock)
{
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port) };

    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    connect(fd, (void *)&a, sizeof a);      /* 回环地址, 不会真的等 */
    if (nonblock) fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

static int send_req(int fd)
{
    return write(fd, "GET\n", 4) == 4 ? 0 : -1;
}

/* 基线: 一个请求等完再发下一个 */
static int fetch_blocking(void)
{
    char buf[8];
    int  fd  = connect_local(0);
    int  got = -1;

    if (send_req(fd) == 0) got = read(fd, buf, sizeof buf);    /* 真阻塞 */
    close(fd);
    return got;
}

static int done;

/* 协程: 同样的顺序代码, 等的时候把线程让出去 */
static void fetch(void *arg)
{
    char buf[8];
    int  fd = connect_local(1);

    (void)arg;
    if (send_req(fd) != 0) { close(fd); return; }

    for (;;) {
        int r = read(fd, buf, sizeof buf);
        if (r > 0) break;
        if (r < 0 && errno == EAGAIN) { co_wait_fd(fd); continue; }
        break;
    }

    close(fd);
    done++;
}

int main(void)
{
    double t;
    int    ok = 0;

    start_server();

    printf("%d 个请求, 服务端每个睡 %d ms:\n", N, DELAY_US / 1000);

    t = now_ms();
    for (int i = 0; i < N; i++) if (fetch_blocking() > 0) ok++;
    printf("  串行阻塞    %6.0f ms   (完成 %d 个)\n", now_ms() - t, ok);

    for (int i = 0; i < N; i++) loop_spawn(fetch, NULL);

    t = now_ms();
    loop_run();
    printf("  单线程协程  %6.0f ms   (完成 %d 个)\n", now_ms() - t, done);

    kill(srv, SIGTERM);
    waitpid(srv, NULL, 0);
    return 0;
}
