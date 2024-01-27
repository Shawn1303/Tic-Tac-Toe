#include "client_registry.h"
// #include "client.h"
#include <semaphore.h>
#include "debug.h"
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

/*
 * The CLIENT_REGISTRY type is a structure that defines the state of a
 * client registry.  You will have to give a complete structure
 * definition in client_registry.c.  The precise contents are up to
 * you.  Be sure that all the operations that might be called
 * concurrently are thread-safe.
 */
typedef struct client_registry {
	int client_thread_counts;
	// CLIENT *firstClient;
	CLIENT **clients;
	int fd[1024];
	pthread_mutex_t mutex;
	sem_t semaphore;
	int waiting_shutdown;
} CLIENT_REGISTRY;

/*
 * Initialize a new client registry.
 *
 * @return  the newly initialized client registry, or NULL if initialization
 * fails.
 */
CLIENT_REGISTRY *creg_init() {
	// debug("%ld: Initialize client registry", pthread_self());
	CLIENT_REGISTRY *cr;
	if(!(cr = malloc(sizeof(CLIENT_REGISTRY)))) {
		error("malloc failed");
		return NULL;
	}

	*cr = (CLIENT_REGISTRY) {
		.client_thread_counts = 0,
		// .firstClient = NULL,
		.clients = calloc(MAX_CLIENTS, sizeof(CLIENT *)),
		.waiting_shutdown = 0
	};

	if(!cr->clients) {
		error("calloc failed");
		free(cr);
		return NULL;
	}

	if (pthread_mutex_init(&cr->mutex, NULL) < 0) {
		error("Mutex initialization failed");
		free(cr->clients);
		free(cr);
		return NULL;
	}

	if (sem_init(&cr->semaphore, 0, 0) < 0) {
		error("Semaphore initialization failed");
		pthread_mutex_destroy(&cr->mutex);
		free(cr->clients);
		free(cr);
		return NULL;
	}

	for(int i = 0; i < 1024; i++) {
		cr->fd[i] = -1;
	}
	
	debug("%ld: Initialize client registry", pthread_self());
	return cr;
}

/*
 * Finalize a client registry, freeing all associated resources.
 * This method should not be called unless there are no currently
 * registered clients.
 *
 * @param cr  The client registry to be finalized, which must not
 * be referenced again.
 */
void creg_fini(CLIENT_REGISTRY *cr) {
	// debug("%ld: Finalize client registry", pthread_self());
	if(cr->client_thread_counts) {
		return;
	}

	pthread_mutex_destroy(&cr->mutex);
	sem_destroy(&cr->semaphore);
	free(cr->clients);
	free(cr);
	debug("%ld: Finalize client registry", pthread_self());
}

/*
 * Register a client file descriptor.
 * If successful, returns a reference to the the newly registered CLIENT,
 * otherwise NULL.  The returned CLIENT has a reference count of one.
 *
 * @param cr  The client registry.
 * @param fd  The file descriptor to be registered.
 * @return a reference to the newly registered CLIENT, if registration
 * is successful, otherwise NULL.
 */
CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd) {
	// debug("%ld: Register client fd %d", pthread_self(), fd);
	pthread_mutex_lock(&cr->mutex);
	//check if full
	if(cr->client_thread_counts == MAX_CLIENTS) {
		error("max clients reached");
		pthread_mutex_unlock(&cr->mutex);
		return NULL;
	}
	//check if fd is not registered
	if(cr->fd[fd] != -1) {
		error("fd already registered");
		pthread_mutex_unlock(&cr->mutex);
		return NULL;
	}
	//create client
	CLIENT *client;
	if(!(client = client_create(cr, fd))) {
		error("client_create failed");
		pthread_mutex_unlock(&cr->mutex);
		return NULL;
	}
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(!(cr->clients[i])) {
			cr->clients[i] = client;
			break;
		}
	}
	//increment client reference count
	// client_ref(client, "for newly created client");
	//increment client count
	cr->client_thread_counts++;
	//register fd
	cr->fd[fd] = 1;
	debug("%ld: Register client fd %d (total connected: %d)", pthread_self(), fd, cr->client_thread_counts);

	pthread_mutex_unlock(&cr->mutex);

	return client;
}

/*
 * Unregister a CLIENT, removing it from the registry.
 * The client reference count is decreased by one to account for the
 * pointer discarded by the client registry.  If the number of registered
 * clients is now zero, then any threads that are blocked in
 * creg_wait_for_empty() waiting for this situation to occur are allowed
 * to proceed.  It is an error if the CLIENT is not currently registered
 * when this function is called.
 *
 * @param cr  The client registry.
 * @param client  The CLIENT to be unregistered.
 * @return 0  if unregistration succeeds, otherwise -1.
 */
int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client) {
	// debug("%ld: Unregister client fd %d", pthread_self(), client_get_fd(client));
	pthread_mutex_lock(&cr->mutex);
	//check if client is registered
	int fd = client_get_fd(client);
	if(cr->fd[fd] != 1) {
		error("client not registered");
		pthread_mutex_unlock(&cr->mutex);
		return -1;
	}

	//remove client from array
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(cr->clients[i]) {
			if(client_get_fd(cr->clients[i]) == client_get_fd(client)) {
				cr->clients[i] = NULL;
				//decrement client reference count
				// client_unref(client, "client is being unregistered");
				break;
			}
		}
		if(i == MAX_CLIENTS - 1) {
			error("client not found");
			pthread_mutex_unlock(&cr->mutex);
			return -1;
		}
	}

	//decrement client count
	cr->client_thread_counts--;
	//unregister fd
	cr->fd[fd] = -1;
	debug("%ld: Unregister client fd %d (total connected: %d)", pthread_self(), fd, cr->client_thread_counts);
	client_unref(client, "because client is being unregistered");

	//if total connected is 0, allow threads to proceed
	if(!(cr->client_thread_counts) && cr->waiting_shutdown) {
		if(sem_post(&cr->semaphore) < 0) {
			error("sem_post failed");
			pthread_mutex_unlock(&cr->mutex);
			return -1;
			// terminate(EXIT_FAILURE);
		}
	}

	pthread_mutex_unlock(&cr->mutex);

	return 0;
}

/*
 * Given a username, return the CLIENT that is logged in under that
 * username.  The reference count of the returned CLIENT is
 * incremented by one to account for the reference returned.
 *
 * @param cr  The registry in which the lookup is to be performed.
 * @param user  The username that is to be looked up.
 * @return the CLIENT currently registered under the specified
 * username, if there is one, otherwise NULL.
 */
CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user) {
	pthread_mutex_lock(&cr->mutex);
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(cr->clients[i]) {
			PLAYER *player;
			//get player from client
			if((player = client_get_player(cr->clients[i]))) {
				//check if player name matches
				if(!(strcmp(player_get_name(player), user))) {
					//increment client reference count
					client_ref(cr->clients[i], "for reference being returned by creg_lookup()");
					pthread_mutex_unlock(&cr->mutex);
					return cr->clients[i];
				}
			}
		}
		if(i == MAX_CLIENTS - 1) {
			error("client not found");
			pthread_mutex_unlock(&cr->mutex);
			return NULL;
		}
	}
	pthread_mutex_unlock(&cr->mutex);
	return NULL;
}

/*
 * Return a list of all currently logged in players.  The result is
 * returned as a malloc'ed array of PLAYER pointers, with a NULL
 * pointer marking the end of the array.  It is the caller's
 * responsibility to decrement the reference count of each of the
 * entries and to free the array when it is no longer needed.
 *
 * @param cr  The registry for which the set of usernames is to be
 * obtained.
 * @return the list of players as a NULL-terminated array of pointers.
 */
PLAYER **creg_all_players(CLIENT_REGISTRY *cr) {
	pthread_mutex_lock(&cr->mutex);
	PLAYER **players;
	if(!(players = malloc(sizeof(PLAYER *) * (cr->client_thread_counts + 1)))) {
		error("malloc failed");
		pthread_mutex_unlock(&cr->mutex);
		return NULL;
	}

	int client_counted = 0;
	int player_counted = 0;
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(cr->clients[i]) {
			PLAYER *player;
			//get player from client
			if((player = client_get_player(cr->clients[i]))) {
				//increment player reference count
				player_ref(player, "for reference being added to players list");
				players[player_counted] = player;
				player_counted++;
			}
			client_counted++;
			if(client_counted == cr->client_thread_counts) {
				players[player_counted] = NULL;
				break;
			}
		}
	}

	//realloc player array
	// PLAYER **newPlayers;
	// if(!(newPlayers = realloc(players, sizeof(PLAYER *) * (player_counted)))) {
	// 	error("realloc failed");
	pthread_mutex_unlock(&cr->mutex);
	// 	return players;
	// }

	// return newPlayers;
	return players;
}

/*
 * A thread calling this function will block in the call until
 * the number of registered clients has reached zero, at which
 * point the function will return.  Note that this function may be
 * called concurrently by an arbitrary number of threads.
 *
 * @param cr  The client registry.
 */
void creg_wait_for_empty(CLIENT_REGISTRY *cr) {
	cr->waiting_shutdown = 1;
	if(!(cr->client_thread_counts)) {
		if(sem_post(&cr->semaphore) < 0) {
			error("sem_post failed");
			return;
		}
	}

	if(sem_wait(&cr->semaphore) < 0) {
		error("sem_wait failed");
		return;
	}
}

/*
 * Shut down (using shutdown(2)) all the sockets for connections
 * to currently registered clients.  The clients are not unregistered
 * by this function.  It is intended that the clients will be
 * unregistered by the threads servicing their connections, once
 * those server threads have recognized the EOF on the connection
 * that has resulted from the socket shutdown.
 *
 * @param cr  The client registry.
 */
void creg_shutdown_all(CLIENT_REGISTRY *cr) {
	for(int i = 0; i < 1024; i++) {
		if(cr->fd[i] == 1) {
			shutdown(cr->fd[i], SHUT_RDWR);
			creg_unregister(cr, cr->clients[i]);
			close(cr->fd[i]);
		}
	}
}