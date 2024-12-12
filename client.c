#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>

#define BUF_SIZE 512
#define USERNAME_LEN 50
#define DEFAULT_PORT 8080

int clientfd = 0;
char buf[BUF_SIZE];
pthread_t threadId;

void* receiveMessages(void* arg);
void handleServerMessage(const char* message);
void printHelp();
void cleanup(int signo);

int main(int argc, char* argv[]) {
    struct sockaddr_in servAddr;
    int port = 0;

    if (argc < 3) {
        printf("Usage: %s <IP_ADDRESS> <PORT>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number %d.\n", port);
        port = DEFAULT_PORT;
    }

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket");
        exit(EXIT_FAILURE);
    }

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, argv[1], &servAddr.sin_addr) <= 0) {
        perror("inet_pton");
        close(clientfd);
        exit(EXIT_FAILURE);
    }

    if (connect(clientfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) == -1) {
        perror("Connect");
        close(clientfd);
        exit(EXIT_FAILURE);
    }

    // Setup signal handler
    if (signal(SIGINT, cleanup) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    printf("Enter 'JOIN <yourname>' to join the conversation.\n");
    if (fgets(buf, BUF_SIZE, stdin) == NULL) {
        perror("fgets");
        close(clientfd);
        exit(EXIT_FAILURE);
    }
    buf[strcspn(buf, "\n")] = '\0';

    ssize_t sendBytes = send(clientfd, buf, strlen(buf), 0);
    if (sendBytes <= 0) {
        if (sendBytes == 0) {
            printf("Server has closed connection.\n");
        } else {
            perror("Send");
        }
        close(clientfd);
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&threadId, NULL, receiveMessages, (void*)&clientfd) != 0) {
        perror("pthread_create");
        close(clientfd);
        exit(EXIT_FAILURE);
    }

    while (1) {
        memset(buf, 0, BUF_SIZE);
        if (fgets(buf, BUF_SIZE, stdin) == NULL) {
            perror("fgets");
            break;
        }
        buf[strcspn(buf, "\n")] = '\0';

        if (strncmp(buf, "QUIT", strlen("QUIT")) == 0) {
            sendBytes = send(clientfd, buf, strlen(buf), 0);
            if (sendBytes <= 0) {
                if (sendBytes == 0) {
                    printf("Server has closed connection.\n");
                } else {
                    perror("Send QUIT");
                }
                close(clientfd);
                exit(EXIT_FAILURE);
            }
            break;
        } else if (strncmp(buf, "DIRECT ", strlen("DIRECT ")) == 0 ||
                   strncmp(buf, "MSG @", strlen("MSG @")) == 0 ||
                   strncmp(buf, "MSG ", strlen("MSG ")) == 0) {
            sendBytes = send(clientfd, buf, strlen(buf), 0);
            if (sendBytes <= 0) {
                if (sendBytes == 0) {
                    printf("Server has closed connection.\n");
                } else {
                    perror("Send message");
                }
                close(clientfd);
                exit(EXIT_FAILURE);
            }
        } else if (strncmp(buf, "/help", strlen("/help")) == 0) {
            printHelp();
        } else {
            printf("Invalid command. Use the following commands:\n");
            printHelp();
        }
    }

    cleanup(0);
    return 0;
}

void* receiveMessages(void* arg) {
    int sockfd = *(int*)arg;
    char buffer[BUF_SIZE];
    ssize_t numBytes;

    while ((numBytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[numBytes] = '\0';
        printf("%s", buffer);
    }

    if (numBytes == 0) {
        printf("Server has closed the connection.\n");
    } else if (numBytes < 0){
        perror("recv");
    }

    close(sockfd);
    exit(EXIT_SUCCESS);
}

void handleServerMessage(const char* message) {
    printf("%s\n", message);
}

void printHelp() {
    printf("\n--- Chat Application Help ---\n");
    printf("Commands:\n");
    printf("1. Type your message and press Enter to send a message to everyone.\n");
    printf("2. Use 'DIRECT <username> <message>' to send a private message.\n");
    printf("3. Use 'MSG <message>' to send a simple message.\n");
    printf("4. Use 'MSG @<username> <message>' to send a nominal message.\n");
    printf("5. Type 'QUIT' to exit the chat.\n");
    printf("6. Type '/help' to display this help menu.\n");
    printf("-----------------------------\n\n");
}

void cleanup(int signo) {
    if (signo == SIGINT) {
        printf("\nCaught SIGINT, exiting...\n");
    }

    pthread_cancel(threadId);
    pthread_join(threadId, NULL);

    if (clientfd > 0) {
        close(clientfd);
    }

    exit(EXIT_SUCCESS);
}
