#ifndef _UTILS_
#define _UTILS_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

struct AcceptedSocketNode *insertAcceptedClient(struct AcceptedSocketNode *head,
												struct AcceptedSocket *client);

struct AcceptedSocketNode *removeClient(struct AcceptedSocketNode *head,
										int socket_fd);

int CreateTCPIpv4Socket();

struct sockaddr_in *CreateIPv4Address(char *ip, int port);

struct AcceptedSocket *acceptIncomingConnection(int serverSocketFD);

void *receiveAndPrintIncomingData(void *arg);

void threadedDataPrinting(int serverSocketFD);

void receivedConnectionsThreadedPrints(struct AcceptedSocket *clientSocket);

void broadcastIncomingMessage(char *buffer, int socketFD);

#endif
