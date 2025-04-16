#include "utils.h"
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/_pthread/_pthread_t.h>
#include <unistd.h>

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
	pthread_t id;
	pthread_create(&id, NULL, receiveAndPrintIncomingData, clientSocket);
}

void threadedDataPrinting(int serverSocketFD) {
	while (true) {
		struct AcceptedSocket *clientSocket =
			acceptIncomingConnection(serverSocketFD);
		receivedConnectionsThreadedPrints(clientSocket);
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
		} else if (amountReceived <= 0) {
			printf("We will break");
			break;
		}
	}
	close(clientSocket->acceptedSocketFD);
	return NULL;
}
