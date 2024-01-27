// #include "invitation.h"
#include "client_registry.h"
#include "debug.h"
#include <stdlib.h>
#include <pthread.h>

/*
 * The INVITATION type is a structure type that defines the state of
 * an invitation.  You will have to give a complete structure
 * definition in invitation.c.  The precise contents are up to you.
 * Be sure that all the operations that might be called concurrently
 * are thread-safe.
 */
typedef struct invitation {
	CLIENT *source;
	GAME_ROLE source_role;
	CLIENT *target;
	GAME_ROLE target_role;
	INVITATION_STATE state;
	GAME *game;
	int reference_count;
	pthread_mutex_t mutex;
} INVITATION;

/*
 * Create an INVITATION in the OPEN state, containing reference to
 * specified source and target CLIENTs, which cannot be the same CLIENT.
 * The reference counts of the source and target are incremented to reflect
 * the stored references.
 *
 * @param source  The CLIENT that is the source of this INVITATION.
 * @param target  The CLIENT that is the target of this INVITATION.
 * @param source_role  The GAME_ROLE to be played by the source of this INVITATION.
 * @param target_role  The GAME_ROLE to be played by the target of this INVITATION.
 * @return a reference to the newly created INVITATION, if initialization
 * was successful, otherwise NULL.
 */
INVITATION *inv_create(CLIENT *source, CLIENT *target, GAME_ROLE source_role, GAME_ROLE target_role) {
	if(client_get_fd(source) == client_get_fd(target)) {
		debug("%ld: Source and target cannot be the same client", pthread_self());
		return NULL;
	}

	INVITATION *inv;
	if(!(inv = malloc(sizeof(INVITATION))))
		return NULL;
	
	*inv = (INVITATION) {
		.source = source,
		.source_role = source_role,
		.target = target,
		.target_role = target_role,
		.state = INV_OPEN_STATE,
		.game = NULL,
		.reference_count = 0
	};

	if (pthread_mutex_init(&inv->mutex, NULL) < 0) {
		error("Mutex initialization failed");
		free(inv);
		return NULL;
	}

	client_ref(source, "as source of new invitation");
	client_ref(target, "as target of new invitation");
	inv_ref(inv, "for newly created invitation");

	return inv;
}

/*
 * Increase the reference count on an invitation by one.
 *
 * @param inv  The INVITATION whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same INVITATION object that was passed as a parameter.
 */
INVITATION *inv_ref(INVITATION *inv, char *why) {
	pthread_mutex_lock(&inv->mutex);
	if(!inv) {
		error("NULL INV");
		pthread_mutex_unlock(&inv->mutex);
		return NULL;
	} 
	inv->reference_count++;
	debug("%ld: Increase reference count on invitation %p (%d -> %d) %s", pthread_self(), inv, inv->reference_count - 1, inv->reference_count, why);
	pthread_mutex_unlock(&inv->mutex);
	return inv;
}

/*
 * Decrease the reference count on an invitation by one.
 * If after decrementing, the reference count has reached zero, then the
 * invitation and its contents are freed.
 *
 * @param inv  The INVITATION whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 *
 */
void inv_unref(INVITATION *inv, char *why) {
	pthread_mutex_lock(&inv->mutex);
	if(!inv) {
		error("NULL INV");
		pthread_mutex_unlock(&inv->mutex);
		return;
	} 
	inv->reference_count--;
	debug("%ld: Decrease reference count on invitation %p (%d -> %d) %s", pthread_self(), inv, inv->reference_count + 1, inv->reference_count, why);
	if(inv->reference_count == 0) {
		debug("%ld: Free invitation %p", pthread_self(), inv);
		client_unref(inv->source, "because invitation is being freed");
		client_unref(inv->target, "because invitation is being freed");
		if(inv->game) {
			game_unref(inv->game, "because invitation is being freed");
		}
		pthread_mutex_unlock(&inv->mutex);
		// inv_close(inv, );
		pthread_mutex_destroy(&inv->mutex);
		// free(inv->game);
		free(inv);
		// inv = NULL;
		return;
	}
	pthread_mutex_unlock(&inv->mutex);
}

/*
 * Get the CLIENT that is the source of an INVITATION.
 * The reference count of the returned CLIENT is NOT incremented,
 * so the CLIENT reference should only be regarded as valid as
 * long as the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the CLIENT that is the source of the INVITATION.
 */
CLIENT *inv_get_source(INVITATION *inv) {
	// pthread_mutex_lock(&inv->mutex);
	// pthread_mutex_unlock(&inv->mutex);
	if(!inv)
		return NULL;
	return inv->source;
}

/*
 * Get the CLIENT that is the target of an INVITATION.
 * The reference count of the returned CLIENT is NOT incremented,
 * so the CLIENT reference should only be regarded as valid if
 * the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the CLIENT that is the target of the INVITATION.
 */
CLIENT *inv_get_target(INVITATION *inv) {
	// pthread_mutex_lock(&inv->mutex);
	// pthread_mutex_unlock(&inv->mutex);
	if(!inv)
		return NULL;
	return inv->target;
}

/*
 * Get the GAME_ROLE to be played by the source of an INVITATION.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME_ROLE played by the source of the INVITATION.
 */
GAME_ROLE inv_get_source_role(INVITATION *inv) {
	// pthread_mutex_lock(&inv->mutex);
	// pthread_mutex_unlock(&inv->mutex);
	if(!inv)
		return NULL_ROLE;
	return inv->source_role;
}

/*
 * Get the GAME_ROLE to be played by the target of an INVITATION.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME_ROLE played by the target of the INVITATION.
 */
GAME_ROLE inv_get_target_role(INVITATION *inv) {
	// pthread_mutex_lock(&inv->mutex);
	// pthread_mutex_unlock(&inv->mutex);
	if(!inv)
		return NULL_ROLE;
	return inv->target_role;
}

/*
 * Get the GAME (if any) associated with an INVITATION.
 * The reference count of the returned GAME is NOT incremented,
 * so the GAME reference should only be regarded as valid as long
 * as the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME associated with the INVITATION, if there is one,
 * otherwise NULL.
 */
GAME *inv_get_game(INVITATION *inv) {
	// pthread_mutex_lock(&inv->mutex);
	// pthread_mutex_unlock(&inv->mutex);
	if(!inv)
		return NULL;
	return inv->game;
}

/*
 * Accept an INVITATION, changing it from the OPEN to the
 * ACCEPTED state, and creating a new GAME.  If the INVITATION was
 * not previously in the the OPEN state then it is an error.
 *
 * @param inv  The INVITATION to be accepted.
 * @return 0 if the INVITATION was successfully accepted, otherwise -1.
 */
int inv_accept(INVITATION *inv) {
	pthread_mutex_lock(&inv->mutex);
	if(!inv) {
		error("NULL INV");
		pthread_mutex_unlock(&inv->mutex);
		return -1;
	}
	if(inv->state != INV_OPEN_STATE) {
		error("INV not in OPEN state");
		pthread_mutex_unlock(&inv->mutex);
		return -1;
	}
	inv->state = INV_ACCEPTED_STATE;
	inv->game = game_create();
	if(!inv->game) {
		error("Failed to create game");
		pthread_mutex_unlock(&inv->mutex);
		return -1;
	}
	pthread_mutex_unlock(&inv->mutex);
	return 0;
}

/*
 * Close an INVITATION, changing it from either the OPEN state or the
 * ACCEPTED state to the CLOSED state.  If the INVITATION was not previously
 * in either the OPEN state or the ACCEPTED state, then it is an error.
 * If INVITATION that has a GAME in progress is closed, then the GAME
 * will be resigned by a specified player.
 *
 * @param inv  The INVITATION to be closed.
 * @param role  This parameter identifies the GAME_ROLE of the player that
 * should resign as a result of closing an INVITATION that has a game in
 * progress.  If NULL_ROLE is passed, then the invitation can only be
 * closed if there is no game in progress.
 * @return 0 if the INVITATION was successfully closed, otherwise -1.
 */
int inv_close(INVITATION *inv, GAME_ROLE role) {
	pthread_mutex_lock(&inv->mutex);
	//if valid inv
	if(!inv) {
		error("NULL INV");
		pthread_mutex_unlock(&inv->mutex);
		return -1;
	}
	//check for its state
	if(inv->state != INV_OPEN_STATE && inv->state != INV_ACCEPTED_STATE) {
		error("INV not in OPEN or ACCEPTED state");
		pthread_mutex_unlock(&inv->mutex);
		return -1;
	}
	//if role is NULL and there is a game, don't close
	if(role == NULL_ROLE) {
		if(inv->game) {
			if(!game_is_over(inv->game)) {
				error("INV has game in progress");
				pthread_mutex_unlock(&inv->mutex);
				return -1;
			}
			// pthread_mutex_unlock(&inv->mutex);
			// return -1;
		}
	}
	//if role is not NULL and there is a game, close
	if(inv->game) {
		if(!game_is_over(inv->game)) {
			if(game_resign(inv->game, role) < 0) {
				pthread_mutex_unlock(&inv->mutex);
				return -1;
			}
		}
	}
	inv->state = INV_CLOSED_STATE;
	pthread_mutex_unlock(&inv->mutex);
	return 0;
}