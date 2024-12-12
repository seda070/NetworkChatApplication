#include "clientList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/uio.h>

#define BUF_SIZE 1024
#define BACKLOGS 7
#define DEF_PORT 8080
#define USERNAMELEN 32
#define HISTORY_FILE "history.txt"

pthread_mutex_t histFileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clientListLock = PTHREAD_MUTEX_INITIALIZER;

FILE* histFile = NULL;
int activeClients = 0;

void* clientHandler(void* arg);
int handleJOIN(int clientfd, char* buf);
void handleMSG(int clientfd, char* buf);
void handleDIRECT(int clientfd, char* buf);
void handleQUIT(int clientfd);
void broadCastMessage(const char* message, int fd);
void addToHistory(const char* sender, const char* msg);
void sendHistory(int clientfd);

int main(int argc, char* argv[]) {
    int port = DEF_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port < 0 || port > 65535) {
            printf("Invalid port number. Using default '8080' port.\n");
            port = DEF_PORT;
        }
    }

    histFile = fopen(HISTORY_FILE, "w+");
    if (histFile == NULL) {
        perror("Error opening history.txt.\n");
        exit(EXIT_FAILURE);
    }

    int servfd = socket(AF_INET, SOCK_STREAM, 0);
    if (servfd < 0) {
        perror("Socket.\n");
        fclose(histFile);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(servfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        fclose(histFile);
        close(servfd);
        exit(EXIT_FAILURE);
    }

    if (listen(servfd, BACKLOGS) < 0) {
        perror("listen");
        fclose(histFile);
        close(servfd);
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d. Ready to accept connections.\n", port);
    while (1) {
        int* clientfd = (int*) malloc(sizeof(int));
        if (clientfd == NULL) {
            perror("malloc.\n");
            fclose(histFile);
            close(servfd);
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in clientaddr;
        socklen_t clientAddrLen = sizeof(clientaddr);

        if ((*clientfd = accept(servfd, (struct sockaddr*)&clientaddr, &clientAddrLen)) < 0) {
            perror("accept.\n");
            free(clientfd);
            continue;
        }

        pthread_mutex_lock(&clientListLock);
        activeClients++;
        pthread_mutex_unlock(&clientListLock);

        pthread_t threadId;
        if (pthread_create(&threadId, NULL, clientHandler, (void*) clientfd) != 0) {
            perror("pthread_create");
            close(*clientfd);
            free(clientfd);
            
            pthread_mutex_lock(&clientListLock);
            activeClients--;
            pthread_mutex_unlock(&clientListLock);

            continue;
        }

        addClient(*clientfd, threadId);
        pthread_detach(threadId);
    }


    pthread_mutex_lock(&clientListLock);
    Client* current = list;
    while (current != NULL) {
        close(current->fd);
        current = current->next;
    }
    pthread_mutex_unlock(&clientListLock);

    fclose(histFile);
    close(servfd);
    return 0;
}


void* clientHandler(void* arg) {
    int clientfd = *((int*) arg);
    free(arg);
    Client* currClient = findClient(clientfd);
    if (currClient == NULL) {
        printf("Cannot find corresponding client's node\n");
        close(clientfd);
        return NULL;
    }

    char buf[BUF_SIZE / 2];
    ssize_t recvBytes = 0;
    memset(buf, 0, sizeof(buf));

    while (1) {
        if ((recvBytes = recv(clientfd, buf, sizeof(buf), 0)) <= 0) {
            if (recvBytes == 0) {
                printf("Client has closed connection.\n");
            } else {
                perror("recv.\n");
            }
            removeClient(clientfd);
            close(clientfd);

            pthread_mutex_lock(&clientListLock);
            activeClients--;
            pthread_mutex_unlock(&clientListLock);

            return NULL;
        }
        buf[recvBytes] = '\0';

        if (strncmp(buf, "JOIN ", strlen("JOIN ")) == 0) {
            if (handleJOIN(clientfd, buf) == 1) {
                break;
            }
            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf), "Enter your name after command JOIN.\n");
            send(clientfd, buf, strlen(buf), 0);
            memset(buf, 0, sizeof(buf));
        }
    }

    while (1) {
        memset(buf, 0, BUF_SIZE);
        if ((recvBytes = recv(clientfd, buf, BUF_SIZE - 1, 0)) <= 0) {
            if (recvBytes == 0) {
                printf("Connection with %s is lost.\n", currClient->name);
            } else {
                perror("recv");
            }
            close(clientfd);
            removeClient(clientfd);

            pthread_mutex_lock(&clientListLock);
            activeClients--;
            if (activeClients == 0) {
                pthread_mutex_unlock(&clientListLock);
                exit(EXIT_SUCCESS);
            }
            pthread_mutex_unlock(&clientListLock);

            return NULL;
        }
        buf[recvBytes] = '\0';
        printf("Received message from client %s: %s\n", currClient->name, buf);

        if (strncmp(buf, "MSG ", strlen("MSG ")) == 0) {
            handleMSG(clientfd, buf);
        } else if (strncmp(buf, "DIRECT ", strlen("DIRECT ")) == 0) {
            handleDIRECT(clientfd, buf);
        } else if (strncmp(buf, "QUIT", strlen("QUIT")) == 0) {
            handleQUIT(clientfd);
            break;
        }
    }

    close(clientfd);
    removeClient(clientfd);
    return NULL;
}

int handleJOIN(int clientfd, char* buf) {
    char* bufcpy = buf + strlen("JOIN");
    while (*bufcpy == ' ' && *bufcpy != '\0') {
        ++bufcpy;
    }

    char username[USERNAMELEN];
    memset(username, 0, USERNAMELEN);
    strncpy(username, bufcpy, USERNAMELEN - 1);
    username[USERNAMELEN - 1] = '\0';

    if (username[0] == '\0') {
        return 0;
    }

    Client* currClient = findClient(clientfd);
    if (currClient == NULL) {
        return 0;
    }

    pthread_mutex_lock(&clientListLock);
    strncpy(currClient->name, username, USERNAMELEN - 1);
    currClient->name[USERNAMELEN - 1] = '\0';
    pthread_mutex_unlock(&clientListLock);

    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "INFO:%s has joined.\n", currClient->name);
    broadCastMessage(msg, clientfd);
    addToHistory("SERVER", msg);
    snprintf(msg, sizeof(msg), "Welcome %s. Here is the recent chat history.\n--------HISTORY--------\n", currClient->name);
    send(clientfd, msg, strlen(msg), 0);
    sendHistory(clientfd);
    return 1;
}

void handleMSG(int clientfd, char* buf) {
    char* bufcpy = buf + strlen("MSG");
    while (*bufcpy == ' ' && *bufcpy != '\0') {
        ++bufcpy;
    }

    if (*bufcpy == '@') {
        char username[USERNAMELEN];
        char* message;
        memset(username, 0, USERNAMELEN);
        sscanf(bufcpy + 1, "%31s", username);
        message = strchr(bufcpy, ' ') + 1;

        if (username[0] == '\0' || message == NULL) {
            return;
        }

        Client* specClient = findClientByName(username);
        if (specClient) {
            char msg[BUF_SIZE];
            snprintf(msg, sizeof(msg), "%s to %s: %s\n", findClient(clientfd)->name, specClient->name, message);
            broadCastMessage(msg, clientfd);
            addToHistory(findClient(clientfd)->name, message);
        } else {
            char msg[BUF_SIZE];
            snprintf(msg, sizeof(msg), "A client with name %s has not been found.\n", username);
            broadCastMessage(msg, -1);
            addToHistory("SERVER", msg);
        }
    } else {
        char msg[BUF_SIZE];
        snprintf(msg, sizeof(msg), "%s: %s\n", findClient(clientfd)->name, bufcpy);
        broadCastMessage(msg, clientfd);
        addToHistory(findClient(clientfd)->name, bufcpy);
    }
}

void handleDIRECT(int clientfd, char* buf) {
    char* username = strtok(buf + strlen("DIRECT "), " ");
    char* message = strtok(NULL, "");

    if (username == NULL || message == NULL) {
        char errMsg[BUF_SIZE];
        snprintf(errMsg, sizeof(errMsg), "Error: DIRECT command should be in the format 'DIRECT <username> <message>'\n");
        send(clientfd, errMsg, strlen(errMsg), 0);
        return;
    }

    Client* specClient = findClientByName(username);
    if (specClient) {
        char msg[BUF_SIZE];
        snprintf(msg, sizeof(msg), "Direct message from %s: %s\n", findClient(clientfd)->name, message);
        send(specClient->fd, msg, strlen(msg), 0);
        addToHistory(findClient(clientfd)->name, message);
    } else {
        char errMsg[BUF_SIZE];
        snprintf(errMsg, sizeof(errMsg), "Error: Client with username %s not found.\n", username);
        send(clientfd, errMsg, strlen(errMsg), 0);
    }
}

void handleQUIT(int clientfd) {
    pthread_mutex_lock(&clientListLock);

    Client* currClient = findClient(clientfd);
    if (currClient) {
        char msg[BUF_SIZE];
        snprintf(msg, sizeof(msg), "%s has left.\n", currClient->name);
        broadCastMessage(msg, clientfd);
        addToHistory("SERVER", msg);
        removeClient(clientfd);
    }

    pthread_mutex_unlock(&clientListLock);
}

void broadCastMessage(const char* message, int fd) {
    pthread_mutex_lock(&clientListLock);

    Client* currClient = list;
    while (currClient) {
        if (currClient->fd != fd) {
            send(currClient->fd, message, strlen(message), 0);
        }
        currClient = currClient->next;
    }

    pthread_mutex_unlock(&clientListLock);
}

void addToHistory(const char* sender, const char* msg) {
    pthread_mutex_lock(&histFileLock);

    fprintf(histFile, "%s: %s\n", sender, msg);
    fflush(histFile);

    pthread_mutex_unlock(&histFileLock);
}

void sendHistory(int clientfd) {
    pthread_mutex_lock(&histFileLock);

    fseek(histFile, 0, SEEK_SET);
    char line[BUF_SIZE];
    while (fgets(line, sizeof(line), histFile)) {
        send(clientfd, line, strlen(line), 0);
    }

    pthread_mutex_unlock(&histFileLock);
}

    