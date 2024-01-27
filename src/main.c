#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "debug.h"
#include "protocol.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"
#include "csapp.h"

#ifdef DEBUG
int _debug_packets_ = 1;
#endif

static void terminate(int status);
static void sigHandler(int sig);
// static void echo(int connfd);
static void *thread(void *vargp);

/*
 * "Jeux" game server.
 *
 * Usage: jeux <port>
 */
int main(int argc, char *argv[])
{
	int pOption = 0;
	// int hOption = 0;
	// int dOption = 0;
	// unsigned short port = 0;
	char* port = NULL;
	// Option processing should be performed here.
	for(int i = 1; i < argc; i++) {
		if(!strcmp(argv[i], "-p")) {
			pOption = 1;
			if(i + 1 < argc) {
				// const char *str = argv[i + 1];
				// char* endptr;
				// port = atoi(argv[i + 1]);
				port = argv[i + 1];
				// port = strtol(str, &endptr, 10);
				i++;
			}
			// debug("hi");
			//  else {
			// 	fprintf(stderr, "Usage: bin/jeux -p <port>\n");
			// 	exit(EXIT_FAILURE);
			// }
		// } else if(!strcmp(argv[i], "-h")) {
		// 	hOption = 1;
		// } else if(!strcmp(argv[i], "-d")) {
		// 	dOption = 1;
		}
	}
	// Option '-p <port>' is required in order to specify the port number
	// on which the server should listen.
	// debug("pOption: %d", pOption);
	// debug("port: %s", port);
	if(!pOption || !port) {
		// fprintf(stderr, "Usage: bin/jeux -p <port>\n");
		error("Usage: bin/jeux -p <port>\n");
		exit(EXIT_FAILURE);
	}
	// debug("hi");

	// if(!strcpy(argv[1], "-p")){
	// 	fprintf(stderr, "Usage: bin/jeux -p <port>\n");
	// 	exit(EXIT_FAILURE);
	// } else {
	// 	port = atoi(argv[2]);
	// }

	// Perform required initializations of the client_registry and
	// player_registry.
	client_registry = creg_init();
	player_registry = preg_init();

	// TODO: Set up the server socket and enter a loop to accept connections
	// on this socket.  For each connection, a thread should be started to
	// run function jeux_client_service().  In addition, you should install
	// a SIGHUP handler, so that receipt of SIGHUP will perform a clean
	// shutdown of the server.

	//setup SIGHUP
	struct sigaction sa1;
	sa1.sa_handler = sigHandler;
	sa1.sa_flags = 0;
	if(sigemptyset(&sa1.sa_mask)) {
		// fprintf(stderr, "sigemptyset: %s\n", strerror(errno));
		error("sigemptyset: %s\n", strerror(errno));
		terminate(EXIT_FAILURE);
	}
	if(sigaction(SIGHUP, &sa1, NULL)) {
		// fprintf(stderr, "sigaction: %s\n", strerror(errno));
		error("sigaction: %s\n", strerror(errno));
		terminate(EXIT_FAILURE);
	}

	//SIGPIPE
	struct sigaction sa2;
    sa2.sa_handler = sigHandler;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0;

    // Set up the signal handler for SIGPIPE
    if (sigaction(SIGPIPE, &sa2, NULL) == -1) {
        perror("sigaction");
        return -1;
    }

	//setup server socket
	// int listenfd, *connfdp;
	int listenfd, *connfdp, connfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr; /* Enough space for any address */
	// char client_hostname[MAXLINE], client_port[MAXLINE];
	pthread_t tid;

	if((listenfd = open_listenfd(port)) < 0) {
		fprintf(stderr, "bind: %s\n", strerror(errno));
		// error("Open_listenfd: %s\n", strerror(errno));
		terminate(EXIT_FAILURE);
	}

	debug("%ld: Jeux server listening on port %s\n", pthread_self(), port);

	while(1) {
		clientlen = sizeof(struct sockaddr_storage);
		// connfdp = malloc(sizeof(int));
		// if((*connfdp = accept(listenfd, (SA *)&clientaddr, &clientlen)) < 0) {
		if((connfd = accept(listenfd, (SA *)&clientaddr, &clientlen)) < 0) {
			// fprintf(stderr, "Accept: %s\n", strerror(errno));
			error("Accept: %s\n", strerror(errno));
			// terminate(EXIT_FAILURE);
			free(connfdp);
			continue;
		} 
		else {
			if(!(connfdp = malloc(sizeof(int)))) {
				// fprintf(stderr, "malloc: %s\n", strerror(errno));
				error("malloc: %s\n", strerror(errno));
				terminate(EXIT_FAILURE);
			}
			*connfdp = connfd;
		}
		// getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE,
		// client_port, MAXLINiE, 0);
		// printf("Connected to (%s, %s)\n", client_hostname, client_port);
		// echo(connfd);
		// Close(connfd);
		// debug("Connection accepted");
		// if(pthread_create(&tid, NULL, jeux_client_service, connfdp)) {
		if(pthread_create(&tid, NULL, thread, connfdp)) {
			// debug("errno: %d", errno);
			// fprintf(stderr, "pthread_create: %s\n", strerror(errno));
			error("pthread_create: %s\n", strerror(errno));
			terminate(EXIT_FAILURE);
		}
	}

	// fprintf(stderr, "You have to finish implementing main() "
	// 				"before the Jeux server will function.\n");

	// terminate(EXIT_FAILURE);
	// terminate(EXIT_SUCCESS);
	return 0;
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status)
{
	// Shutdown all client connections.
	// This will trigger the eventual termination of service threads.
	creg_shutdown_all(client_registry);

	debug("%ld: Waiting for service threads to terminate...", pthread_self());
	creg_wait_for_empty(client_registry);
	debug("%ld: All service threads terminated.", pthread_self());

	// Finalize modules.
	creg_fini(client_registry);
	preg_fini(player_registry);

	debug("%ld: Jeux server terminating", pthread_self());
	exit(status);
}

void sigHandler(int sig) {
	if(sig == SIGHUP) {
		// debug("terminating server...");
		terminate(EXIT_SUCCESS);
	}
	if(sig == SIGPIPE) {
		// debug("SIGPIPE");
		debug("Failed to send end-of-game notification");
	}
	// debug("sigHandler: %d", sig);
}

// void echo(int connfd)
// {
// 	size_t n;
// 	char buf[MAXLINE];
// 	rio_t rio;

// 	Rio_readinitb(&rio, connfd);
// 		while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
// 		printf("server received %d bytes\n", (int)n);
// 		Rio_writen(connfd, buf, n);
// 	}
// }

void *thread(void *vargp)
{
	// int connfd = *((int *)vargp);
	// pthread_detach(pthread_self());
	// debug("hi");
	// echo(connfd);
	jeux_client_service(vargp);
	// debug("closing connection...");
	// close(*((int *)vargp));
	// free(vargp);
	return NULL;
} 