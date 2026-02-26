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
#define QUESTION_TIME 15
#define TOTAL_QUESTIONS 3

/* Question structure */
typedef struct {
    char question[256];
    char correct;
} Question;

/* Client structure */
typedef struct {
    int sock;
    int score;
    int answered;
    char name[32];
} Client;

Client clients[MAX_CLIENTS];

/* Question bank */
Question quiz[TOTAL_QUESTIONS] = {

{
"Q1: What is the capital of Nepal?\n"
"A) Pokhara\nB) Kathmandu\nC) Lalitpur\nD) Biratnagar\n",
'B'
},

{
"Q2: Which protocol is connection-oriented?\n"
"A) UDP\nB) IP\nC) TCP\nD) FTP\n",
'C'
},

{
"Q3: Which port is used for HTTP?\n"
"A) 21\nB) 25\nC) 80\nD) 110\n",
'C'
}

};

int current_question = 0;
time_t start_time;
int quiz_started = 0;

/* Function to exit on error */
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
        sprintf(clients[i].name,"Player%d",i+1);
    }
}

/* Send message safely */
void send_safe(int sock,const char *msg)
{
    if(send(sock,msg,strlen(msg),0)<0)
        perror("Send failed");
}

/* Broadcast question */
void broadcast_question()
{
    printf("\nSending Question %d\n",current_question+1);

    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i].sock>0)
        {
            clients[i].answered=0;
            send_safe(clients[i].sock,quiz[current_question].question);
        }
    }

    start_time=time(NULL);
}

/* Send leaderboard */
void send_leaderboard()
{
    char board[BUFFER_SIZE];
    char line[128];

    strcpy(board,"\n===== FINAL LEADERBOARD =====\n");

    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i].sock>0)
        {
            sprintf(line,"%s : %d\n",clients[i].name,clients[i].score);
            strcat(board,line);
        }
    }

    strcat(board,"=============================\n");

    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i].sock>0)
            send_safe(clients[i].sock,board);
    }

    printf("%s",board);
}

/* Count active clients */
int active_clients()
{
    int count=0;

    for(int i=0;i<MAX_CLIENTS;i++)
        if(clients[i].sock>0)
            count++;

    return count;
}

/* Count answered clients */
int answered_clients()
{
    int count=0;

    for(int i=0;i<MAX_CLIENTS;i++)
        if(clients[i].sock>0 && clients[i].answered)
            count++;

    return count;
}

/* Remove client */
void remove_client(int i)
{
    close(clients[i].sock);
    clients[i].sock=0;
}

int main()
{
    int server_fd,new_socket,activity,max_sd;
    struct sockaddr_in address;
    socklen_t addrlen=sizeof(address);

    fd_set readfds;
    struct timeval timeout;

    int opt=1;

    server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd<0)
        error_exit("Socket failed");

    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(PORT);

    if(bind(server_fd,(struct sockaddr*)&address,sizeof(address))<0)
        error_exit("Bind failed");

    if(listen(server_fd,5)<0)
        error_exit("Listen failed");

    printf("Quiz Server started on port %d\n",PORT);

    init_clients();

    while(1)
    {
        FD_ZERO(&readfds);
        FD_SET(server_fd,&readfds);
        max_sd=server_fd;

        for(int i=0;i<MAX_CLIENTS;i++)
        {
            int sd=clients[i].sock;

            if(sd>0)
                FD_SET(sd,&readfds);

            if(sd>max_sd)
                max_sd=sd;
        }

        timeout.tv_sec=1;
        timeout.tv_usec=0;

        activity=select(max_sd+1,&readfds,NULL,NULL,&timeout);

        if(activity<0)
            error_exit("select error");

        /* New client connection */
        if(FD_ISSET(server_fd,&readfds))
        {
            new_socket=accept(server_fd,(struct sockaddr*)&address,&addrlen);

            printf("New client connected\n");

            for(int i=0;i<MAX_CLIENTS;i++)
            {
                if(clients[i].sock==0)
                {
                    clients[i].sock=new_socket;

                    if(!quiz_started)
                    {
                        quiz_started=1;
                        broadcast_question();
                    }

                    break;
                }
            }
        }

        /* Receive answers */
        for(int i=0;i<MAX_CLIENTS;i++)
        {
            int sd=clients[i].sock;

            if(sd>0 && FD_ISSET(sd,&readfds) && !clients[i].answered)
            {
                char ans;

                int bytes=recv(sd,&ans,1,0);

                if(bytes<=0)
                {
                    remove_client(i);
                }
                else
                {
                    clients[i].answered=1;

                    if(ans==quiz[current_question].correct)
                    {
                        clients[i].score+=10;
                        send_safe(sd,"Correct! +10\n");
                    }
                    else
                    {
                        clients[i].score-=5;
                        send_safe(sd,"Wrong! -5\n");
                    }
                }
            }
        }

        /* Move to next question */
        if(quiz_started)
        {
            int act=active_clients();
            int ans=answered_clients();

            if(ans==act || difftime(time(NULL),start_time)>=QUESTION_TIME)
            {
                current_question++;

                if(current_question<TOTAL_QUESTIONS)
                {
                    broadcast_question();
                }
                else
                {
                    send_leaderboard();
                    break;
                }
            }
        }
    }

    for(int i=0;i<MAX_CLIENTS;i++)
        if(clients[i].sock>0)
            close(clients[i].sock);

    close(server_fd);

    printf("Server shutdown\n");

    return 0;
}