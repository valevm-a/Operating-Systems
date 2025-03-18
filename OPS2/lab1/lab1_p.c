#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define MAX_SELNUMB 36
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t cont = 0;

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void sigchld_handler(int sig)
{
    pid_t pid;
    for (;;)
    {
        pid = waitpid(0, NULL, WNOHANG);
        if (pid == 0)
            return;
        if (pid <= 0)
        {
            if (errno == ECHILD)
                return;
            ERR("waitpid");
        }
    }
}


void child_work(int M, int write_pipe, int read_pipe)
{
    fprintf(stderr, "[%d]: I have %d$ and I'm going to play roullete.\n", getpid(), M);

    pid_t pid = getpid();
    srand(time(NULL) * pid);
    //int cont = 1;

    while (M > 0)
    {
        sleep(2);
        //cont = 0;

        int bet_amount = 1 + rand() % M;
        int selected_number = rand() % (MAX_SELNUMB + 1);
        if (TEMP_FAILURE_RETRY(write(write_pipe, &bet_amount, 1)) < 0)
        ERR("write");
        if (TEMP_FAILURE_RETRY(write(write_pipe, &selected_number, 1)) < 0)
        ERR("write");

        int lucky_number;
        if (TEMP_FAILURE_RETRY(read(read_pipe, &lucky_number, 1)) < 0)
        ERR("read");

        if (lucky_number == selected_number)
        {
            fprintf(stderr, "[%d]: Whoa, I won %d!\n", getpid(), bet_amount*35);
            M += bet_amount*35;
        }
        else
            M -= bet_amount;

    } 

        fprintf(stderr, "[%d]: I'm broke.\n", getpid());
}

void parent_work(int n, int* read_pipes, int* write_pipes, pid_t* chld_pids)
{
    int players_left = n;

    for (int i = 0; i < n; i++)
    {
        close(read_pipes[2 * i + 1]);
        close(write_pipes[2 * i]);
    }
    
    while (players_left > 0)
    {
        for (int i = 0; i < n; i++)
        {
            if (read_pipes[2 * i] != 0)
            {
                int bet_amount, selected_number;

                ssize_t bytes_read;
                bytes_read = TEMP_FAILURE_RETRY(read(read_pipes[2 * i], &bet_amount, 1));
                if (bytes_read != 1)
                {
                    if (TEMP_FAILURE_RETRY(close(read_pipes[2 * i])))
                        ERR("close");
                    if (TEMP_FAILURE_RETRY(close(write_pipes[2 * i + 1])))
                        ERR("close");
                    read_pipes[2 * i] = 0;
                    write_pipes[2 * i + 1] = 0;
                    players_left--;
                    continue;
                }
                if (TEMP_FAILURE_RETRY(read(read_pipes[2 * i], &selected_number, 1)) < 0)
                    ERR("read");
                fprintf(stderr, "Croupier: [%d] placed %d on a number %d.\n", chld_pids[i], bet_amount, selected_number);
            }
        }
        if (players_left > 0)
        {
        srand(time(NULL) * getpid());
        int lucky_number = rand() % MAX_SELNUMB + 1;
        fprintf(stderr, "Croupier: %d is the lucky number.\n", lucky_number);

        for (int i = 0; i < n; i++)
        {
            if (write_pipes[2 * i + 1] != 0)
            {
                if (TEMP_FAILURE_RETRY(write(write_pipes[2 * i + 1], &lucky_number, 1)) < 0)
                ERR("write");
                }
            }
        }
        }

    fprintf(stderr, "Croupier: Casino always wins.\n");
}

void create_pipes(int n, int* pl_pipes, int* dl_pipes)
{
    for (int i = 0; i < n; i++)
    {
        if (pipe(pl_pipes + (2 * i)) < 0)
            ERR("Pipe:");
        if (pipe(dl_pipes + (2 * i)) < 0)
            ERR("Pipe:");
    }
}

void create_children(int n, int M, int* pl_pipes, int* dl_pipes, pid_t* chld_pids)
{
    pid_t pid;

    for (int i = 0; i < n; i++)
    {
        if ((pid = fork()) < 0)
            ERR("Fork:");
        if (!pid)
        {
            close(pl_pipes[2 * i]);
            close(dl_pipes[2 * i + 1]);

            for (int j = 0; j < 2 * n; j++) {
                if (j != 2 * i && j != 2 * i + 1) {
                    close(pl_pipes[j]);
                    close(dl_pipes[j]);
                }
            }

            child_work(M, pl_pipes[2 * i + 1], dl_pipes[2 * i]);
            close(pl_pipes[2 * i + 1]);
            close(dl_pipes[2 * i]);
            exit(EXIT_SUCCESS);
        }
        else
        {
            chld_pids[i] = pid;
        }
    }
}

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s 0<n 0<M\n", name);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int n, M;
    if (argc < 3)
        usage(argv[0]);
    n = atoi(argv[1]);
    M = atoi(argv[2]);
    if (n <= 0 || M <= 0)
        usage(argv[0]);

    int pl_pipes[2 * n];
    int dl_pipes[2 * n];
    pid_t chld_pids[n];

    sethandler(SIG_IGN, SIGPIPE);
    sethandler(SIG_IGN, SIGCHLD);

    create_pipes(n, pl_pipes, dl_pipes);
    create_children(n, M, pl_pipes, dl_pipes, chld_pids);
    parent_work(n, pl_pipes, dl_pipes, chld_pids);

    /*for (int i = 0; i < 2 * n; i++)
    {
        if (dl_pipes[i] && TEMP_FAILURE_RETRY(close(dl_pipes[i])))
            ERR("close");
    }*/

    /*for (int i = 0; i < 2 * n; i++)
    {
        if (pl_pipes[i] && TEMP_FAILURE_RETRY(close(pl_pipes[i])))
            ERR("close");
    }*/

    while (wait(NULL) > 0);
    return EXIT_SUCCESS;
}

