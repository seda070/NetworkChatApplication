#include "clientList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Client* list = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void addClient(int fd, pthread_t threadId) {
    pthread_mutex_lock(&mutex);
    Client* newClient = (Client*)malloc(sizeof(Client));
    if (newClient == NULL) {
        perror("Malloc");
        pthread_mutex_unlock(&mutex);
        return;
    }
    newClient->fd = fd;
    newClient->threadId = threadId;
    newClient->next = list;
    list = newClient;
    pthread_mutex_unlock(&mutex);
}

void removeClient(int fd) {
    pthread_mutex_lock(&mutex);
    Client* current = list;
    Client* prev = NULL;
    while (current != NULL) {
        if (current->fd == fd) {
            if (prev == NULL) {
                list = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            pthread_mutex_unlock(&mutex);
            return;
        }
        prev = current;
        current = current->next;
    }
    pthread_mutex_unlock(&mutex);
}

Client* findClient(int fd) {
    pthread_mutex_lock(&mutex);
    Client* current = list;
    while (current != NULL) {
        if (current->fd == fd) {
            pthread_mutex_unlock(&mutex);
            return current;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

Client* findClientByName(const char* name) {
    pthread_mutex_lock(&mutex);
    Client* current = list;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            pthread_mutex_unlock(&mutex);
            return current;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

