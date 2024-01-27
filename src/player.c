#include "player.h"
#include "debug.h"
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>

typedef struct player {
	char *username;
	int rating;
	int reference_count;
	pthread_mutex_t mutex;
} PLAYER;

/*
 * Create a new PLAYER with a specified username.  A private copy is
 * made of the username that is passed.  The newly created PLAYER has
 * a reference count of one, corresponding to the reference that is
 * returned from this function.
 *
 * @param name  The username of the PLAYER.
 * @return  A reference to the newly created PLAYER, if initialization
 * was successful, otherwise NULL.
 */
PLAYER *player_create(char *name) {
	PLAYER *player;
	// debug("sizeof(name) = %zu", strlen(name));
	// name[strlen(name)] = '\0';
	// debug("Creating player [%s]", name);
	// debug("name[strlen(name)] = [%c]", name[strlen(name) - 1]);
	// debug("name[strlen(name)] = [%d]", name[strlen(name)]);

	if(!(player = malloc(sizeof(PLAYER))))
		return NULL;

	*player = (PLAYER) {
		// .username = name,
		.rating = PLAYER_INITIAL_RATING,
		.reference_count = 0,
	};

	// Allocate memory for the name parameter and copy the string
    player->username = strdup(name);

	if (pthread_mutex_init(&player->mutex, NULL) < 0) {
		error("Mutex initialization failed");
		free(player);
		return NULL;
	}

	player_ref(player, "for newly created player");

	return player;
}

/*
 * Increase the reference count on a player by one.
 *
 * @param player  The PLAYER whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same PLAYER object that was passed as a parameter.
 */
PLAYER *player_ref(PLAYER *player, char *why) {
	pthread_mutex_lock(&player->mutex);
	if(!player) {
		error("player_ref: NULL player");
		pthread_mutex_unlock(&player->mutex); 
		return NULL;
	}
	player->reference_count++;
	debug("%ld: Increase reference count on player [%s] (%d -> %d) %s",
		pthread_self(), player->username, player->reference_count - 1, player->reference_count, why);
	pthread_mutex_unlock(&player->mutex);
	return player;
}

/*
 * Decrease the reference count on a PLAYER by one.
 * If after decrementing, the reference count has reached zero, then the
 * PLAYER and its contents are freed.
 *
 * @param player  The PLAYER whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 *
 */
void player_unref(PLAYER *player, char *why) {
	pthread_mutex_lock(&player->mutex);
	if(!player) {
		error("player_unref: NULL player");
		pthread_mutex_unlock(&player->mutex);
		return;
	}
	player->reference_count--;
	debug("%ld: Decrease reference count on player [%s] (%d -> %d) %s",
		pthread_self(), player->username, player->reference_count + 1, player->reference_count, why);
	if(player->reference_count == 0) {
		free(player->username);
		pthread_mutex_unlock(&player->mutex);
		pthread_mutex_destroy(&player->mutex);
		debug("Free player %p", player);
		free(player);
	}
	pthread_mutex_unlock(&player->mutex);
}

/*
 * Get the username of a player.
 *
 * @param player  The PLAYER that is to be queried.
 * @return the username of the player.
 */
char *player_get_name(PLAYER *player) {
	// pthread_mutex_lock(&player->mutex);
	// pthread_mutex_unlock(&player->mutex);
	return player->username;
}

/*
 * Get the rating of a player.
 *
 * @param player  The PLAYER that is to be queried.
 * @return the rating of the player.
 */
int player_get_rating(PLAYER *player){
	// pthread_mutex_lock(&player->mutex);
	// pthread_mutex_unlock(&player->mutex);
	return player->rating;
}

/*
 * Post the result of a game between two players.
 * To update ratings, we use a system of a type devised by Arpad Elo,
 * similar to that used by the US Chess Federation.
 * The player's ratings are updated as follows:
 * Assign each player a score of 0, 0.5, or 1, according to whether that
 * player lost, drew, or won the game.
 * Let S1 and S2 be the scores achieved by player1 and player2, respectively.
 * Let R1 and R2 be the current ratings of player1 and player2, respectively.
 * Let E1 = 1/(1 + 10**((R2-R1)/400)), and
 *     E2 = 1/(1 + 10**((R1-R2)/400))
 * Update the players ratings to R1' and R2' using the formula:
 *     R1' = R1 + 32*(S1-E1)
 *     R2' = R2 + 32*(S2-E2)
 *
 * @param player1  One of the PLAYERs that is to be updated.
 * @param player2  The other PLAYER that is to be updated.
 * @param result   0 if draw, 1 if player1 won, 2 if player2 won.
 */
void player_post_result(PLAYER *player1, PLAYER *player2, int result){
	debug("%ld: Post result(%s, %s, %d)", pthread_self(), player1->username, player2->username, result);
	double S1, S2, E1, E2, R1, R2;
    //scores
    switch(result) {
        case 0:
            S1 = 0.5;
            S2 = 0.5;
            break;
        case 1:
            S1 = 1;
            S2 = 0;
            break;
        case 2:
            S1 = 0;
            S2 = 1;
            break;
    }
    // debug("S1 = %f, S2 = %f", S1, S2);
    //current ratings
	pthread_mutex_lock(&player1->mutex);
	pthread_mutex_lock(&player2->mutex);
    R1 = player_get_rating(player1);
    R2 = player_get_rating(player2);
    // debug("R1 = %f, R2 = %f", R1, R2);
    //expected scores
    E1 = 1/(1 + pow(10, ((R2-R1)/400.0)));
    E2 = 1/(1 + pow(10, ((R1-R2)/400.0)));
    // debug("E1 = %f, E2 = %f", E1, E2);
    //update ratings
    player1->rating += (int)(32*(S1-E1));
    player2->rating += (int)(32*(S2-E2));
	pthread_mutex_unlock(&player1->mutex);
	pthread_mutex_unlock(&player2->mutex);
    // debug("R1' = %d, R2' = %d", player1->rating, player2->rating);
	// int S1, S2, E1, E2, R1, R2;
	// //scores
	// switch(result) {
	// 	case 0:
	// 		S1 = 0.5;
	// 		S2 = 0.5;
	// 		break;
	// 	case 1:
	// 		S1 = 1;
	// 		S2 = 0;
	// 		break;
	// 	case 2:
	// 		S1 = 0;
	// 		S2 = 1;
	// 		break;
	// }
	// debug("S1 = %d, S2 = %d", S1, S2);
	// //current ratings
	// R1 = player_get_rating(player1);
	// R2 = player_get_rating(player2);
	// debug("R1 = %d, R2 = %d", R1, R2);
	// //expected scores
	// E1 = 1/(1 + pow(10, ((R2-R1)/400)));
	// E2 = 1/(1 + pow(10, ((R1-R2)/400)));
	// debug("E1 = %d, E2 = %d", E1, E2);
	// //update ratings
	// player1->rating += (int)(32*(S1-E1));
	// player2->rating += (int)(32*(S2-E2));
	// debug("R1' = %d, R2' = %d", player1->rating, player2->rating);
}
