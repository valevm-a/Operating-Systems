#include "l4_common.h"

#define BACKLOG 5
#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

volatile sig_atomic_t do_work = 1;

int32_t SUMS[MAX_EVENTS] = {0};

void sigint_handler(int sig)
{
    do_work = 0;
}

int32_t calculate_sum_of_digits(pid_t pid)
{
    int sum = 0;
    while (pid > 0)
    {
        sum += pid % 10;
        pid /= 10;
    }
    return sum;
}

void handle_client(int client_socket, int n)
{
    printf("Server: Connection accepted.\n");

    pid_t received_pid;
    ssize_t bytes_received = bulk_read(client_socket, (char *)&received_pid, sizeof(received_pid));
    if (bytes_received < 0)
        ERR("bulk_read");
    if (bytes_received == 0)
        printf("Client disconnected.\n");
    else
    {
        printf("Received pid from client: %d\n", ntohl(received_pid));


        int32_t sum = calculate_sum_of_digits(ntohl(received_pid));
        int32_t network_order_sum = htonl(sum);

        ssize_t bytes_sent = bulk_write(client_socket, (char *)&network_order_sum, sizeof(network_order_sum));
        if (bytes_sent < 0)
            ERR("bulk_write");

        SUMS[n] = sum;
    }

    close(client_socket);
}

void doServer(int server_socket)
{   
    int epoll_fd;
    if ((epoll_fd = epoll_create1(0)) < 0)
        ERR("epoll_create:");

    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    while (do_work)
    {
        int clients_numb = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &oldmask);
        if (clients_numb == -1)
        {
            if (errno == EINTR)
                continue;
            ERR("epoll_wait");
        }

        for (int n = 0; n < clients_numb; n++)
        {
            int client_socket = add_new_client(events[n].data.fd); //data fd aka server_socket
            
            handle_client(client_socket, n);
        }
    }

    int32_t max_sum = SUMS[0];

    for (int32_t i; i < MAX_EVENTS; i++)
    {
        if (max_sum < SUMS[i])
            max_sum = SUMS[i];
    }

    fprintf(stderr, "HIGH SUM=%d\n", max_sum);

    close(epoll_fd);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s <port> \n", name);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int server_socket;

    if (argc != 2)
        usage(argv[0]);

    if (sethandler(SIG_IGN, SIGPIPE))
        ERR("Seting SIGPIPE:");

    if (sethandler(sigint_handler, SIGINT))
        ERR("Seting SIGINT:");

    server_socket = bind_tcp_socket(atoi(argv[1]), BACKLOG);
    int new_flags = fcntl(server_socket, F_GETFL) | O_NONBLOCK;
    fcntl(server_socket, F_SETFL, new_flags);

    printf("Server: Listening for connections on port %s\n", argv[1]);

    doServer(server_socket);

    if (TEMP_FAILURE_RETRY(close(server_socket)) < 0)
        ERR("close");
    fprintf(stderr, "Server has terminated.\n");
    return EXIT_SUCCESS;
}