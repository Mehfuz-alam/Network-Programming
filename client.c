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

    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
        error_exit("Socket creation failed");

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if(inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) <= 0)
        error_exit("Invalid address");

    /* Connect */
    if(connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
        error_exit("Connection failed");

    printf("Connected to quiz server\n");

    /* Receive question */
    int bytes = recv(sock, buffer, BUFFER_SIZE, 0);
    if(bytes <= 0)
        error_exit("Failed to receive question");

    buffer[bytes] = '\0';

    printf("\n%s\n", buffer);

    /* Get answer */
    printf("Enter answer (A/B/C/D): ");
    scanf(" %c", &answer);

    if(answer!='A' && answer!='B' && answer!='C' && answer!='D')
    {
        printf("Invalid input\n");
        close(sock);
        return 0;
    }

    /* Send answer */
    if(send(sock, &answer, 1, 0) < 0)
        error_exit("Send failed");

    /* Receive result and leaderboard */
    while((bytes = recv(sock, buffer, BUFFER_SIZE-1, 0)) > 0)
    {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }

    printf("\nDisconnected from server\n");

    close(sock);

    return 0;
}