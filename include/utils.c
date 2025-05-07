#include "utils.h"
#include "blocking_io.h"
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_ssize_t.h>
#include <sys/select.h>
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

struct AcceptedSocketNode *head = NULL;
unsigned socket_size = sizeof(struct AcceptedSocket);
pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;

void threadedDataPrinting(int serverSocketFD) {
	fd_set current_sockets, ready_sockets;
	FD_ZERO(&current_sockets);
	FD_SET(serverSocketFD, &current_sockets); // We add the serverSocketFD to
											  // the current_sockets fd_set
	int maxfd = serverSocketFD;
	while (true) {
		ready_sockets = current_sockets;
		if (select(maxfd + 1, &ready_sockets, NULL, NULL, NULL) < 0) {
			perror("Select Error");
			exit(EXIT_FAILURE);
		}

		for (int i = 0; i <= maxfd; i++) {
			if (!FD_ISSET(i, &ready_sockets))
				continue;
			if (i == serverSocketFD) {
				struct AcceptedSocket *clientSocket =
					acceptIncomingConnection(serverSocketFD);
				if (clientSocket->acceptedSocketFD >= 0) {
					FD_SET(clientSocket->acceptedSocketFD, &current_sockets);
					if (clientSocket->acceptedSocketFD > maxfd)
						maxfd = clientSocket->acceptedSocketFD;
					pthread_mutex_lock(&clients_lock);
					head = insertAcceptedClient(head, clientSocket);
					pthread_mutex_unlock(&clients_lock);
				}
				free(clientSocket);
			} else {
				char buffer[4096];
				ssize_t amountReceived = recv(i, buffer, sizeof(buffer) - 1, 0);
				if (amountReceived > 0) {
					buffer[amountReceived] = 0;
					// printf("Response is %s\n", buffer);
					bio_read_4k();
					broadcastIncomingMessage(buffer, i);
				} else if (amountReceived <= 0) {
					close(i);
					FD_CLR(i, &current_sockets);
					pthread_mutex_lock(&clients_lock);
					head = removeClient(head, i);
					pthread_mutex_unlock(&clients_lock);
				}
			}
		}
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
