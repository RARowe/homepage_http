#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>

extern uint8_t _binary_favico_ico_start[];
extern uint8_t _binary_favico_ico_end[];

int create_socket(int port) {
	int s;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("Unable to create socket");
		exit(EXIT_FAILURE);
	}
	const int enable = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
	}

	if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		perror("Unable to bind");
		exit(EXIT_FAILURE);
	}

	if (listen(s, 1) < 0) {
		perror("Unable to listen");
		exit(EXIT_FAILURE);
	}

	return s;
}


typedef struct {
	sem_t mutex;
	sem_t ready;
	int id, fd;
} ThreadCtx;

void *reply(void* context) {
	ThreadCtx* ctx = (ThreadCtx*) context;
	printf("thread %d ready!\n", ctx->id);
	const char resp[] =  "HTTP/1.1 200 OK\n"
		"Content-Length: 62\n"
		"Content-Type: text/plain; charset=utf-8\n"
		"\n"
		"Hello World! This page was served by the homepage http server.";
	const size_t n = strlen(resp);
	const size_t favico_size =
		_binary_favico_ico_end - _binary_favico_ico_start;
	char junk[8096];
	char *favico_resp = malloc(16 * 1024);
	int header_len = snprintf(favico_resp, 16 * 1024,
		"HTTP/1.1 200 OK\n"
		"Content-Length: %d\n"
		"Content-Type: image/x-icon\n"
		"\n", favico_size);
	for (int i = 0; i < favico_size; i++) {
		favico_resp[header_len + i] = (char)_binary_favico_ico_start[i];
	}

	while (1) {
		sem_post(&ctx->ready);
		sem_wait(&ctx->mutex);
		int client = ctx->fd;
		read(client, junk, 8096);

		if (strcasestr(junk, "favico")) {
			write(client, favico_resp, header_len + favico_size);
		} else {
			write(client, resp, n);
		}
		close(client);
	}
}


int main(int argc, char **argv)
{
	pthread_t thread[4];
	ThreadCtx tctx[4];
	for (long i = 0; i < 4; i++) {
		tctx[i].id = i;
		sem_init(&tctx[i].mutex, 0, 0);
		sem_init(&tctx[i].ready, 0, 0);
		int result = pthread_create(&thread[i], NULL, reply, (void*)&tctx[i]);
		if (result != 0) {
			fprintf(stderr, "Error creating thread: %d\n", result);
			return 1;
		}
	}
	int sock;

	/* Ignore broken pipe signals */
	signal(SIGPIPE, SIG_IGN);

	sock = create_socket(4433);

	long requests = 0;
	/* Handle connections */
	while(1) {
		struct sockaddr_in addr;
		unsigned int len = sizeof(addr);

		int client = accept(sock, (struct sockaddr*)&addr, &len);
		if (client < 0) {
			perror("Unable to accept");
			exit(EXIT_FAILURE);
		}


		// NOTE: Request may not be done when we change the fd
		sem_wait(&tctx[requests % 4].ready);
		tctx[requests % 4].fd = client;
		sem_post(&tctx[requests % 4].mutex);
		requests += 1;
	}

	close(sock);
}
