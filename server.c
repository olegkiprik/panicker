#define _GNU_SOURCE
#include <signal.h>
#include <stdlib.h>
#include <sys/un.h>
#include <stdio.h>
#include <netdb.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define BACKLOG 3

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig) { do_work = 0; }

void doServer()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        ERR("");
    } 

    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int s = getaddrinfo(NULL, "10000", &hints, &result);
    if (s != 0) {
        ERR("");
    }

    int bd = bind(fd, result->ai_addr, result->ai_addrlen);
    if (bd < 0) {
        ERR("");
    }
    while (do_work)
    {
        if (listen(fd, BACKLOG) == -1) {
            ERR("");
        }

        struct sockaddr_in *result_addr = (struct sockaddr_in *)result->ai_addr;
        int client_fd = accept(fd, NULL, NULL);

        char buffer[sizeof(int32_t[2])];

        int len = read(client_fd, buffer, sizeof(buffer) - 1);
        buffer[len] = '\0';
        if (len == (int)(sizeof(buffer) - 1))
            printf("ALARM!!!\n");
    }

    close(fd);
}

int main() {
    if (sethandler(SIG_IGN, SIGPIPE))
        ERR("Seting SIGPIPE:");
    if (sethandler(sigint_handler, SIGINT))
        ERR("Seting SIGINT:");

    doServer();
    fprintf(stderr, "Server has been terminated.\n");
    return EXIT_SUCCESS;
}
