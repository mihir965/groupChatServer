#include "utils.h"
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/_pthread/_pthread_t.h>
#include <sys/socket.h>
#include <unistd.h>

struct AcceptedSocket acceptedSockets[10];
int acceptedSocketsIndex = 0;

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
}

void threadedDataPrinting(int serverSocketFD) {
	while (true) {
		struct AcceptedSocket *clientSocket =
			acceptIncomingConnection(serverSocketFD);

		acceptedSockets[acceptedSocketsIndex++] = *clientSocket;

		receivedConnectionsThreadedPrints(clientSocket);
	}
}

void broadcastIncomingMessage(char *buffer, int socketFD) {
	for (int i = 0; i < acceptedSocketsIndex; i++) {
		if (acceptedSockets[i].acceptedSocketFD == socketFD) {
			continue;
		} else {
			send(acceptedSockets[i].acceptedSocketFD, buffer, strlen(buffer),
				 0);
		}
	}
}

void *receiveAndPrintIncomingData(void *arg) {
	struct AcceptedSocket *clientSocket = (struct AcceptedSocket *)arg;

	char buffer[1024];

	while (true) {
		ssize_t amountReceived =
			recv(clientSocket->acceptedSocketFD, buffer, 1024, 0);

		if (amountReceived > 0) {
			buffer[amountReceived] = 0;
			printf("Response is %s", buffer);

			broadcastIncomingMessage(buffer, clientSocket->acceptedSocketFD);
		} else if (amountReceived <= 0) {
			printf("We will break");
			break;
		}
	}
	close(clientSocket->acceptedSocketFD);
	return NULL;
}
