// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define BUFFER_SIZE 512
#define QUESTION_TIME 10

typedef struct {
    int sock;
    int score;
    int answered;
    char name[32];
} Client;

Client clients[MAX_CLIENTS];

char question[] =
"Q1: What is the capital of Nepal?\n"
"A) Pokhara\n"
"B) Kathmandu\n"
"C) Lalitpur\n"
"D) Biratnagar\n";

char correct_answer = 'B';

time_t start_time;
int quiz_started = 0;

/* Function to handle fatal errors */
void error_exit(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/* Initialize clients */
void init_clients()
{
    for(int i=0;i<MAX_CLIENTS;i++)
    {
        clients[i].sock = 0;
        clients[i].score = 0;
        clients[i].answered = 0;
        sprintf(clients[i].name, "Player%d", i+1);
    }
}

/* Send message safely */
void send_safe(int sock, const char *msg)
{
    if(send(sock, msg, strlen(msg), 0) < 0)
    {
        perror("Send failed");
    }
}

/* Broadcast leaderboard */
void send_leaderboard()
{
    char board[BUFFER_SIZE];
    char line[128];

    strcpy(board, "\n=== Leaderboard ===\n");

    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i].sock > 0)
        {
            sprintf(line, "%s : %d\n", clients[i].name, clients[i].score);
            strcat(board, line);
        }
    }

    strcat(board, "====================\n");

    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i].sock > 0)
        {
            send_safe(clients[i].sock, board);
        }
    }
}

/* Remove disconnected client */
void remove_client(int index)
{
    printf("%s disconnected\n", clients[index].name);
    close(clients[index].sock);
    clients[index].sock = 0;
}

int main()
{
    int server_fd, new_socket, activity, max_sd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    fd_set readfds;
    struct timeval timeout;

    /* Create socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0)
        error_exit("Socket failed");

    /* Allow reuse */
    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        error_exit("setsockopt failed");

    /* Configure address */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    /* Bind */
    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        error_exit("Bind failed");

    /* Listen */
    if(listen(server_fd, 5) < 0)
        error_exit("Listen failed");

    printf("Quiz Server started on port %d\n", PORT);

    init_clients();

    while(1)
    {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        /* Add client sockets */
        for(int i=0;i<MAX_CLIENTS;i++)
        {
            int sd = clients[i].sock;
            if(sd > 0)
                FD_SET(sd, &readfds);

            if(sd > max_sd)
                max_sd = sd;
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if(activity < 0)
            error_exit("select error");

        /* New connection */
        if(FD_ISSET(server_fd, &readfds))
        {
            new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);

            if(new_socket < 0)
                perror("Accept failed");
            else
            {
                printf("New client connected\n");

                for(int i=0;i<MAX_CLIENTS;i++)
                {
                    if(clients[i].sock == 0)
                    {
                        clients[i].sock = new_socket;
                        clients[i].answered = 0;

                        send_safe(new_socket, question);

                        if(!quiz_started)
                        {
                            quiz_started = 1;
                            start_time = time(NULL);
                            printf("Quiz started\n");
                        }

                        break;
                    }
                }
            }
        }

        /* Receive answers */
        for(int i=0;i<MAX_CLIENTS;i++)
        {
            int sd = clients[i].sock;

            if(sd > 0 && FD_ISSET(sd, &readfds))
            {
                char ans;
                int bytes = recv(sd, &ans, 1, 0);

                if(bytes <= 0)
                {
                    remove_client(i);
                }
                else if(!clients[i].answered)
                {
                    clients[i].answered = 1;

                    if(ans == correct_answer)
                    {
                        clients[i].score += 10;
                        send_safe(sd, "Correct answer!\n");
                    }
                    else
                    {
                        send_safe(sd, "Wrong answer!\n");
                    }
                }
            }
        }

        /* Timer check */
        if(quiz_started && difftime(time(NULL), start_time) >= QUESTION_TIME)
        {
            printf("Time up\n");
            send_leaderboard();
            break;
        }
    }

    /* Cleanup */
    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i].sock > 0)
            close(clients[i].sock);
    }

    close(server_fd);

    printf("Server shutdown\n");

    return 0;
}