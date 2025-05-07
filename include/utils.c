#include "utils.h"
#include "blocking_io.h"
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

struct JobQueue {
	Job **ring;
	size_t cap, head, tail, count;
	bool shutting_down;
	pthread_mutex_t lock;
	pthread_cond_t not_empty, not_full;
};

pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;

JobQueue *jq_init(size_t capacity) {
	JobQueue *q = malloc(sizeof *q);
	q->ring = calloc(capacity, sizeof(Job *));
	q->cap = capacity;
	q->head = q->tail = q->count = 0;
	q->shutting_down = false;
	pthread_mutex_init(&q->lock, NULL);
	pthread_cond_init(&q->not_empty, NULL);
	pthread_cond_init(&q->not_full, NULL);
	return q;
}

void jq_enqueue(JobQueue *q, Job *j) {
	pthread_mutex_lock(&q->lock);
	while (q->count == q->cap && !q->shutting_down)
		pthread_cond_wait(&q->not_full, &q->lock);

	if (!q->shutting_down) {
		q->ring[q->tail] = j;
		q->tail = (q->tail + 1) % q->cap;
		q->count++;

		// Signalling that the queue is not empty
		pthread_cond_signal(&q->not_empty);
	}

	pthread_mutex_unlock(&q->lock);
}

Job *jq_dequeue(JobQueue *q) {
	pthread_mutex_lock(&q->lock);
	while (q->count == 0 && !q->shutting_down) {
		pthread_cond_wait(&q->not_empty, &q->lock);
	}
	if (q->count == 0 && q->shutting_down) {
		pthread_mutex_unlock(&q->lock);
		return NULL;
	}
	Job *j = q->ring[q->head];
	q->head = (q->head + 1) % q->cap;
	q->count--;
	pthread_cond_signal(&q->not_full);
	pthread_mutex_unlock(&q->lock);
	return j;
}

void jq_shutdown(JobQueue *q) {
	pthread_mutex_lock(&q->lock);
	q->shutting_down = true;
	pthread_cond_broadcast(&q->not_empty);
	pthread_cond_broadcast(&q->not_full);
	pthread_mutex_unlock(&q->lock);
}

void jq_destroy(JobQueue *q) {
	free(q->ring);
	pthread_mutex_destroy(&q->lock);
	pthread_cond_destroy(&q->not_empty);
	pthread_cond_destroy(&q->not_full);
	free(q);
}

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
static JobQueue *g_q = NULL;
static pthread_t *g_workers = NULL;
static int g_nw = 0;

static void *worker_fn(void *arg) {
	JobQueue *q = arg;
	for (;;) {
		Job *job = jq_dequeue(q);
		if (!job)
			break;

		bio_read_4k();

		pthread_mutex_lock(&clients_lock);
		struct AcceptedSocketNode *c = head;
		while (c) {
			int dst = c->data->acceptedSocketFD;
			if (dst != job->sender_fd)
				send(dst, job->msg, job->len, 0);
			c = c->next;
		}
		pthread_mutex_unlock(&clients_lock);
		free(job->msg);
		free(job);
	}
	return NULL;
}

static void spawn_workers(JobQueue *q, int n) {
	g_q = q;
	g_nw = n;
	g_workers = malloc(sizeof(pthread_t) * n);
	for (int i = 0; i < n; i++) {
		pthread_create(&g_workers[i], NULL, worker_fn, q);
	}
}

static void join_workers(void) {
	for (int i = 0; i < g_nw; i++)
		pthread_join(g_workers[i], NULL);
	free(g_workers);
}

void threadedDataPrinting(int serverSocketFD) {

	JobQueue *q = jq_init(256); // capacity 256
	spawn_workers(q, 4);		// 4 worker threads

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
					pthread_mutex_lock(
						&clients_lock); // Making sure that shared resources are
										// not getting race conditions
					head = insertAcceptedClient(head, clientSocket);
					pthread_mutex_unlock(&clients_lock);
				}
				free(clientSocket);
			} else {
				char buffer[4096];
				ssize_t amountReceived = recv(i, buffer, sizeof(buffer) - 1, 0);
				if (amountReceived > 0) {
					Job *job = malloc(sizeof *job);
					job->msg = malloc(amountReceived);
					memcpy(job->msg, buffer, amountReceived);
					job->len = (size_t)amountReceived;
					job->sender_fd = i;
					jq_enqueue(q, job);
				} else {
					close(i);
					FD_CLR(i, &current_sockets);
					pthread_mutex_lock(&clients_lock);
					head = removeClient(head, i);
					pthread_mutex_unlock(&clients_lock);
				}
			}
		}
	}
	jq_shutdown(q);
	join_workers();
	jq_destroy(q);
}

// void broadcastIncomingMessage(char *buffer, int socketFD) {
// 	struct AcceptedSocketNode *temp = head;
// 	while (temp != NULL) {
// 		if (temp->data->acceptedSocketFD == socketFD) {
// 			temp = temp->next;
// 			continue;
// 		} else {
// 			send(temp->data->acceptedSocketFD, buffer, strlen(buffer), 0);
// 		}
// 		temp = temp->next;
// 	}
// }
