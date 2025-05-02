#include "../../include/utils.h"
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_pthread/_pthread_t.h>
#include <sys/_types/_ssize_t.h>
#include <sys/socket.h>
#include <unistd.h>

void *printBroadcastMessages(void *arg) {

	int socket_fd = (int)(intptr_t)arg;

	char buffer[1024];

	while (true) {
		ssize_t amountRecieved = recv(socket_fd, buffer, 1024, 0);
		if (amountRecieved > 0) {
			buffer[amountRecieved] = 0;
			printf("Response is %s", buffer);
		}
	}
}

void createThreadForListeningAndPrint(int socket_fd) {
	pthread_t id;
	pthread_create(&id, NULL, printBroadcastMessages,
				   (void *)(intptr_t)socket_fd);
}

int main() {

	int socket_fd = CreateTCPIpv4Socket();

	struct sockaddr_in *address = CreateIPv4Address("127.0.0.1", 2000);
	int result =
		connect(socket_fd, (const struct sockaddr *)address, sizeof(*address));

	if (result == 0) {
		printf("Connection is successful\n");
	}

	char *line = NULL;
	size_t lineSize = 0;
	printf("Send your message, ... type exit to close the connection\n");

	createThreadForListeningAndPrint(socket_fd);

	while (true) {
		ssize_t charCount = getline(&line, &lineSize, stdin);
		if (charCount > 0) {
			if (strcmp(line, "exit\n") == 0) {
				break;
			}
			ssize_t amountSent = send(socket_fd, line, charCount, 0);
			// printf("%zd", amountSent);
			if (amountSent == 1) {
				printf("Nothing was sent\n");
			}
		}
	}

	close(socket_fd);

	return 0;
}
