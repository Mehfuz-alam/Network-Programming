// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 512

void error_exit(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main()
{
    int sock;
    struct sockaddr_in server;
    char buffer[BUFFER_SIZE];
    char answer;

    sock=socket(AF_INET,SOCK_STREAM,0);

    if(sock<0)
        error_exit("Socket failed");

    server.sin_family=AF_INET;
    server.sin_port=htons(PORT);

    if(inet_pton(AF_INET,"127.0.0.1",&server.sin_addr)<=0)
        error_exit("Invalid address");

    if(connect(sock,(struct sockaddr*)&server,sizeof(server))<0)
        error_exit("Connection failed");

    printf("Connected to Quiz Server\n");

    while(1)
    {
        int bytes=recv(sock,buffer,BUFFER_SIZE-1,0);

        if(bytes<=0)
            break;

        buffer[bytes]='\0';

        printf("\n%s\n",buffer);

        /* Check if question */
        if(strstr(buffer,"A)")!=NULL)
        {
            printf("Enter answer: ");
            scanf(" %c",&answer);

            send(sock,&answer,1,0);
        }
    }

    printf("Disconnected from server\n");

    close(sock);

    return 0;
}