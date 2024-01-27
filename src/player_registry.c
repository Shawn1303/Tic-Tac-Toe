#include "player_registry.h"
#include "debug.h"
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

/*
 * A player registry maintains a mapping from usernames to PLAYER objects.
 * Entries persist for as long as the server is running.
 */

/*
 * The PLAYER_REGISTRY type is a structure type that defines the state
 * of a player registry.  You will have to give a complete structure
 * definition in player_registry.c. The precise contents are up to
 * you.  Be sure that all the operations that might be called
 * concurrently are thread-safe.
 */
typedef struct player_registry {
	int player_count;
	int max_players;
	PLAYER **players;
	pthread_mutex_t mutex;
} PLAYER_REGISTRY;

/*
 * Initialize a new player registry.
 *
 * @return the newly initialized PLAYER_REGISTRY, or NULL if initialization
 * fails.
 */
PLAYER_REGISTRY *preg_init(void) {
	PLAYER_REGISTRY *preg;
	if(!(preg = malloc(sizeof(PLAYER_REGISTRY)))) {
		error("malloc failed");
		return NULL;
	}

	*preg = (PLAYER_REGISTRY) {
		.player_count = 0,
		.max_players = 0
		// .players = calloc(preg->max_players + 1, sizeof(PLAYER *))
	};

	if(!(preg->players = calloc(preg->max_players + 1, sizeof(PLAYER *)))) {
		error("calloc failed");
		free(preg);
		return NULL;
	}

	if(pthread_mutex_init(&preg->mutex, NULL)) {
		error("pthread_mutex_init failed");
		free(preg->players);
		free(preg);
		return NULL;
	}

	debug("%ld: Initialize player registry", pthread_self());
	return preg;
}

/*
 * Finalize a player registry, freeing all associated resources.
 *
 * @param cr  The PLAYER_REGISTRY to be finalized, which must not
 * be referenced again.
 */
void preg_fini(PLAYER_REGISTRY *preg) {
	for(int i = 0; i < preg->max_players; i++) {
		if(preg->players[i]) {
			player_unref(preg->players[i], "for closing player registry");
			preg->players[i] = NULL;
		}
	}
	free(preg->players);
	pthread_mutex_destroy(&preg->mutex);
	free(preg);
	debug("%ld: Finalize player registry", pthread_self());
}

/*
 * Register a player with a specified user name.  If there is already
 * a player registered under that user name, then the existing registered
 * player is returned, otherwise a new player is created.
 * If an existing player is returned, then its reference count is increased
 * by one to account for the returned pointer.  If a new player is
 * created, then the returned player has reference count equal to two:
 * one count for the pointer retained by the registry and one count for
 * the pointer returned to the caller.
 *
 * @param name  The player's user name, which is copied by this function.
 * @return A pointer to a PLAYER object, in case of success, otherwise NULL.
 *
 */
PLAYER *preg_register(PLAYER_REGISTRY *preg, char *name) {
	pthread_mutex_lock(&preg->mutex);
	debug("%ld: Register player %s", pthread_self(), name);
	int name_exists = 0;
	PLAYER *player;
	for(int i = 0; i < preg->max_players; i++) {
		if(preg->players[i]) {
			// debug("pname: %s", player_get_name(preg->players[i]));
			// debug("name: %s", name);
			if(!strcmp(player_get_name(preg->players[i]), name)) {
				name_exists = 1;
				player = preg->players[i];
				break;
			}
		}
		if(i == preg->player_count - 1) {
			break;
		}
	}

	if(name_exists) {
		debug("%ld: Player exists with that name", pthread_self());
		player_ref(player, "for new reference to existing player");
	} else {
		debug("%ld: Player with that name does not yet exist", pthread_self());
		if(!(player = player_create(name))) {
			error("player_create failed");
			pthread_mutex_unlock(&preg->mutex);
			return NULL;
		}
		if(preg->player_count < preg->max_players) {
			for(int i = 0; i < preg->max_players; i++) {
				if(!preg->players[i]) {
					preg->players[i] = player;
					break;
				}
			}
		} else {
			PLAYER **temp;
			if(!(temp = realloc(preg->players, (preg->max_players + 2) * sizeof(PLAYER *)))) {
				error("realloc failed");
				pthread_mutex_unlock(&preg->mutex);
				return NULL;
			} else {
				preg->players = temp;
				preg->max_players++;
				preg->players[preg->max_players - 1] = player;
				preg->players[preg->max_players] = NULL;
			}
		}
		player_ref(player, "for reference being retained by player registry");
	}

	preg->player_count++;

	pthread_mutex_unlock(&preg->mutex);

	return player;
}