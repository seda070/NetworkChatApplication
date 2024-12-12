#ifndef CLIENTLIST_H
#define CLIENTLIST_H

#define USERNAME_LEN 50

#include <pthread.h>
#include <arpa/inet.h>



typedef struct Client {
    int fd;
    pthread_t threadId;
    char name[USERNAME_LEN];
    struct Client* next;
} Client;

extern Client *list;
extern pthread_mutex_t mutex;

void addClient(int fd, pthread_t threadId);
void removeClient(int fd);
Client* findClient(int fd);
Client* findClientByName(const char* name);

#endif //CLIENTLIST_H
