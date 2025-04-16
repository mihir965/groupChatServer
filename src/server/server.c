#include "../../include/utils.h"
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/_types/_socklen_t.h>
#include <sys/_types/_ssize_t.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
	int serverSocketFD = CreateTCPIpv4Socket();
	struct sockaddr_in *serverAddress = CreateIPv4Address("", 2000);

	int result = bind(serverSocketFD, (const struct sockaddr *)serverAddress,
					  sizeof(*serverAddress));

	if (result == 0) {
		printf("Socket was bound successfully");
	}

	int listenResult = listen(
		serverSocketFD, 10); // Queues all the connections that are coming to
							 // the serverSocketFD. We are allowing 10 such
							 // connections before the next functions are run
	// The second parameter in the listen function is basically called the
	// backlog, which is the number of connection requests the OS will queue for
	// the socekt.

	// Now we want to get the client socket file descriptor, basicallyt the ont
	// that we created in the client.c. Both these file descritors are the same
	// and will allow us to write to and from it.

	threadedDataPrinting(serverSocketFD);

	shutdown(serverSocketFD, SHUT_RDWR);

	return 0;
}
