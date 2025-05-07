#ifndef _UTILS_
#define _UTILS_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_endian.h>
#include <sys/socket.h>
#include <unistd.h>

struct AcceptedSocket {
	int acceptedSocketFD;
	struct sockaddr_in address;
	int error;
	bool connectionAcceptedFully;
};

struct AcceptedSocketNode {
	struct AcceptedSocket *data;
	struct AcceptedSocketNode *next;
};

typedef struct {
	int sender_fd;
	size_t len;
	char *msg;
} Job;

typedef struct JobQueue JobQueue;

JobQueue *jq_init(size_t capacity);
void jq_enqueue(JobQueue *q, Job *j); // This will block if full
Job *jq_dequeue(JobQueue *q); // This will block if empty, NULL on shutdown
void jq_shutdown(JobQueue *q);
void jq_destroy(JobQueue *q);

extern pthread_mutex_t clients_lock;

struct AcceptedSocketNode *insertAcceptedClient(struct AcceptedSocketNode *head,
												struct AcceptedSocket *client);

struct AcceptedSocketNode *removeClient(struct AcceptedSocketNode *head,
										int socket_fd);

int CreateTCPIpv4Socket();

struct sockaddr_in *CreateIPv4Address(char *ip, int port);

struct AcceptedSocket *acceptIncomingConnection(int serverSocketFD);

// void *receiveAndPrintIncomingData(void *arg);
void receiveAndPrintIncomingData(int client_socket_fd);

void threadedDataPrinting(int serverSocketFD);

void receivedConnectionsThreadedPrints(int client_socket_fd);

void broadcastIncomingMessage(char *buffer, int socketFD);

#endif
