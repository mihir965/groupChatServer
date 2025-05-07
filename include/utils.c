#include "utils.h"
#include "blocking_io.h"
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct AcceptedSocketNode *insertAcceptedClient(struct AcceptedSocketNode *head,
												struct AcceptedSocket *client) {
	struct AcceptedSocketNode *newClient =
		(struct AcceptedSocketNode *)malloc(sizeof(struct AcceptedSocketNode));

	newClient->data = malloc(sizeof(struct AcceptedSocket));
	newClient->next = head;

	memcpy(newClient->data, client, sizeof(struct AcceptedSocket));

	return newClient;
}

struct AcceptedSocketNode *removeClient(struct AcceptedSocketNode *head,
										int socket_fd) {
	struct AcceptedSocketNode *cur = head;
	struct AcceptedSocketNode *prev = NULL;
	while (cur) {
		if (cur->data->acceptedSocketFD == socket_fd) {
			if (prev) {
				prev->next = cur->next;
			} else {
				head = cur->next;
			}
			free(cur->data);
			free(cur);
			break;
		}
		prev = cur;
		cur = cur->next;
	}
	return head;
}

int CreateTCPIpv4Socket() {
	return socket(AF_INET, SOCK_STREAM, 0);
}

struct sockaddr_in *CreateIPv4Address(char *ip, int port) {
	struct sockaddr_in *address = malloc(sizeof(
		struct sockaddr_in)); // We are basically calling the malloc function to
							  // tell the user process that is the heap, to
							  // allocate this much amount of memory for the
							  // address. We need to persist the address.
							  // Whatever that we create in the function is
							  // created on the stack of the process' virtual
							  // memory and we need to tell to allocate this
							  // much amount of memory
	address->sin_family = AF_INET;
	address->sin_port = htons(port);

	if (strlen(ip) == 0) {
		address->sin_addr.s_addr = INADDR_ANY;
	} else {
		inet_pton(AF_INET, ip, &address->sin_addr.s_addr);
	}
	return address;
}

struct AcceptedSocket *acceptIncomingConnection(int serverSocketFD) {
	struct sockaddr_in clientAddress;
	socklen_t clientAddressLen = sizeof(clientAddress);
	int clientSocketFD = accept(
		serverSocketFD, (struct sockaddr *)&clientAddress, &clientAddressLen);
	struct AcceptedSocket *acceptedSocket =
		malloc(sizeof(struct AcceptedSocket));
	acceptedSocket->address = clientAddress;
	acceptedSocket->acceptedSocketFD = clientSocketFD;
	acceptedSocket->connectionAcceptedFully = clientSocketFD > 0;
	if (!acceptedSocket->connectionAcceptedFully) {
		acceptedSocket->error = clientSocketFD;
	}
	return acceptedSocket;
}

void receivedConnectionsThreadedPrints(struct AcceptedSocket *clientSocket) {

	// Basically were creating a thread for every accepted client. Each thread
	// will run the receiveAndPrintIncomingData funciton to print the incoming
	// message. We will also use the same thread to broadcast the message of
	// each client to every other client

	pthread_t id;
	pthread_create(&id, NULL, receiveAndPrintIncomingData, clientSocket);
	pthread_detach(id);
}

struct AcceptedSocketNode *head = NULL;
unsigned socket_size = sizeof(struct AcceptedSocket);
pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;

void threadedDataPrinting(int serverSocketFD) {
	while (true) {
		struct AcceptedSocket *clientSocket =
			acceptIncomingConnection(serverSocketFD);
		pthread_mutex_lock(&clients_lock);
		head = insertAcceptedClient(head, clientSocket);
		pthread_mutex_unlock(&clients_lock);
		receivedConnectionsThreadedPrints(clientSocket);
		// free(clientSocket);
	}
}

void broadcastIncomingMessage(char *buffer, int socketFD) {
	pthread_mutex_lock(&clients_lock);
	struct AcceptedSocketNode *temp = head;
	while (temp != NULL) {
		if (temp->data->acceptedSocketFD == socketFD) {
			temp = temp->next;
			continue;
		} else {
			send(temp->data->acceptedSocketFD, buffer, strlen(buffer), 0);
		}
		temp = temp->next;
	}
	pthread_mutex_unlock(&clients_lock);
}

void *receiveAndPrintIncomingData(void *arg) {
	struct AcceptedSocket *clientSocket = (struct AcceptedSocket *)arg;

	char buffer[1024];

	while (true) {
		ssize_t amountReceived =
			recv(clientSocket->acceptedSocketFD, buffer, 1024, 0);

		bio_read_4k();

		if (amountReceived > 0) {
			buffer[amountReceived] = 0;
			printf("Response is %s", buffer);
			broadcastIncomingMessage(buffer, clientSocket->acceptedSocketFD);
		} else if (amountReceived <= 0) {
			pthread_mutex_lock(&clients_lock);
			head = removeClient(head, clientSocket->acceptedSocketFD);
			pthread_mutex_unlock(&clients_lock);
			break;
		}
	}
	close(clientSocket->acceptedSocketFD);
	return NULL;
}
