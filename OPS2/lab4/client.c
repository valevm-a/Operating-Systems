#include "l4_common.h"
#define BUFFER_SIZE 1024

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s <server_address> <port> \n", name);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int client_socket;
    //char buffer[BUFFER_SIZE];

    if (argc != 3)
        usage(argv[0]);

    client_socket = connect_tcp_socket(argv[1], argv[2]);
    if (client_socket < 0)
        ERR("connect_tcp_socket");

    pid_t pid_to_send = getpid();
    pid_t network_order_pid = htonl(pid_to_send);

    ssize_t bytes_sent = bulk_write(client_socket, (char *)&network_order_pid, sizeof(network_order_pid));
    if (bytes_sent < 0)
        ERR("bulk_write");

    printf("Client: Sent PID to server.\n");

    int32_t received_sum;
    ssize_t bytes_received = bulk_read(client_socket, (char *)&received_sum, sizeof(received_sum));
    if (bytes_received < 0)
        ERR("bulk_read");
    else
        printf("Received sum from server: %d\n", ntohl(received_sum));

    close(client_socket);

    return 0;
}
