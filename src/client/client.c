#include "../../include/utils.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/_types/_ssize_t.h>
#include <unistd.h>

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

	while (true) {
		ssize_t charCount = getline(&line, &lineSize, stdin);
		if (charCount > 0) {
			if (strcmp(line, "exit\n") == 0) {
				break;
			}
			ssize_t amountSent = send(socket_fd, line, charCount, 0);
		}
	}

	close(socket_fd);

	return 0;
}
