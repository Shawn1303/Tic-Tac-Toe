#include "game.h"
#include "debug.h"
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

/*
 * A GAME represents the current state of a game between participating
 * players.  So that a GAME object can be passed around 
 * without fear of dangling references, it has a reference count that
 * corresponds to the number of references that exist to the object.
 * A GAME object will not be freed until its reference count reaches zero.
 */

/*
 * The GAME type is a structure type that defines the state of a game.
 * You will have to give a complete structure definition in game.c.
 * The precise contents are up to you.  Be sure that all the operations
 * that might be called concurrently are thread-safe.
 */
typedef struct game {
	// char game_state[31];
	char *game_state;
	int game_board[9];
	int box_indexes[9];
	// char box_indexes[9];
	int game_terminated;
	GAME_ROLE winner;
	GAME_ROLE current_player;
	int ref_count;
	pthread_mutex_t mutex;
} GAME;
// {' ', ' ', ' ', '\n', '-', '-', '-', '-', '-', '\n',' ', ' ', ' ', '\n', '-', '-', '-', '-', '-', '\n', ' ', ' ', ' ', '\n', '\0'}
/*
 * The GAME_MOVE type is a structure type that defines a move in a game.
 * The details are up to you.  A GAME_MOVE is immutable.
 */
typedef struct game_move {
	int moveBox;
	GAME_ROLE player;
} GAME_MOVE;

/*
 * Create a new game in an initial state.  The returned game has a
 * reference count of one.
 *
 * @return the newly created GAME, if initialization was successful,
 * otherwise NULL.
 */
GAME *game_create(void) {
	GAME *game = malloc(sizeof(GAME));
	if (game == NULL) {
		debug("game_create: malloc failed");
		return NULL;
	}
	
	*game = (GAME) {
		// .game_state = {' ', ' ', ' ', '\n', '-', '-', '-', '-', '-', '\n',' ', ' ', ' ', '\n', '-', '-', '-', '-', '-', '\n', ' ', ' ', ' ', '\n', '\0'},
		.game_terminated = 0,
		.current_player = FIRST_PLAYER_ROLE,
		.winner = NULL_ROLE,
		.ref_count = 0
	};

	game->game_state = calloc(31, sizeof(char));
	strcpy(game->game_state, " | | \n-----\n | | \n-----\n | | \n");

	// if(!(game->game_state)) {
	// 	debug("game_create: calloc failed");
	// 	free(game);
	// 	return NULL;
	// }

	// char *game_state = " | | \n-----\n | | \n-----\n | | \n";
	// game->game_state = game_state;

	for(int i = 0; i < 9; i++) {
		game->game_board[i] = 0;
	}

	int j = 0;
	for(int i = 0; i < 3; i++) {
		game->box_indexes[i] = j;
		// game->box_indexes[i] = &(game->game_state[i]);
		// game->box_indexes[i] = game->game_state[i];
		// debug("game_state[i] = %d into %d", game->game_state[i], i);
		game->box_indexes[i + 3] = j + 12;
		// game->box_indexes[i + 3] = &(game->game_state[i + 12]);
		// game->box_indexes[i + 3] = game->game_state[i + 12];
		// debug("game_state[i+12] = %d into %d", game->game_state[i + 12], i + 3);
		game->box_indexes[i + 6] = j + 24;
		// game->box_indexes[i + 6] = &(game->game_state[i + 24]);
		// game->box_indexes[i + 6] = game->game_state[i + 24];
		// debug("game_state[i+24] = %d into %d", game->game_state[i + 24], i + 6);

		j += 2;
	}

	// for(int i = 0; i < 9; i++) {
	// 	debug("box_indexes[%d] = %d", i, game->box_indexes[i]);
	// 	debug("game_state[%d] = %d", game->box_indexes[i], game->game_state[game->box_indexes[i]]);
	// }
	// debug("game_state: %s", game->game_state);

	if (pthread_mutex_init(&game->mutex, NULL) != 0) {
		debug("game_create: pthread_mutex_init failed");
		free(game);
		return NULL;
	}

	game_ref(game, "for newly created game");

	return game;
}

/*
 * Increase the reference count on a game by one.
 *
 * @param game  The GAME whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same GAME object that was passed as a parameter.
 */
GAME *game_ref(GAME *game, char *why) {
	pthread_mutex_lock(&game->mutex);
	if(!game) {
		error("game_ref: game is NULL");
		pthread_mutex_unlock(&game->mutex);
		return NULL;
	}
	game->ref_count++;
	debug("%ld: Increase reference count on game %p (%d -> %d) %s", 
	pthread_self(), game, game->ref_count - 1, game->ref_count, why);
	pthread_mutex_unlock(&game->mutex);
	return game;
}

/*
 * Decrease the reference count on a game by one.  If after
 * decrementing, the reference count has reached zero, then the
 * GAME and its contents are freed.
 *
 * @param game  The GAME whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 */
void game_unref(GAME *game, char *why) {
	pthread_mutex_lock(&game->mutex);
	if(!game) {
		error("game_ref: game is NULL");
		pthread_mutex_unlock(&game->mutex);
		return;
	}
	game->ref_count--;
	debug("%ld: Decrease reference count on game %p (%d -> %d) %s",
	pthread_self(), game, game->ref_count + 1, game->ref_count, why);
	if (game->ref_count == 0) {
		free(game->game_state);
		pthread_mutex_unlock(&game->mutex);
		pthread_mutex_destroy(&game->mutex);
		debug("Freeing game %p", game);
		free(game);
		return;
	}
	pthread_mutex_unlock(&game->mutex);
}

/*
 * Apply a GAME_MOVE to a GAME.
 * If the move is illegal in the current GAME state, then it is an error.
 *
 * @param game  The GAME to which the move is to be applied.
 * @param move  The GAME_MOVE to be applied to the game.
 * @return 0 if application of the move was successful, otherwise -1.
 */
int game_apply_move(GAME *game, GAME_MOVE *move) {
	pthread_mutex_lock(&game->mutex);
	if(!game) {
		error("game_apply_move: game is NULL");
		pthread_mutex_unlock(&game->mutex);
		return -1;
	}
	if(!move) {
		error("game_apply_move: move is NULL");
		pthread_mutex_unlock(&game->mutex);
		return -1;
	}
	if(game->current_player != move->player) {
		error("game_apply_move: move is out of turn");
		pthread_mutex_unlock(&game->mutex);
		return -1;
	}
	// debug("game_apply_move: move->moveBox = %d", move->moveBox);
	// debug("game_apply_move: game->game_board[move->moveBox] = %d", game->game_board[move->moveBox]);
	if(game->game_board[move->moveBox - 1]) {
		error("game_apply_move: move is illegal");
		pthread_mutex_unlock(&game->mutex);
		return -1;
	}
	switch(move->player){
		case FIRST_PLAYER_ROLE:
		// X
			// game->game_board[move->moveBox] = -1;
			game->game_board[move->moveBox - 1] = -1;
			// debug("move->moveBox = %d", move->moveBox);
			// debug("game->box_indexes[move->moveBox] = %d", game->box_indexes[move->moveBox - 1]);
			// debug("game->game_state[game->box_indexes[move->moveBox]] = %c", game->game_state[game->box_indexes[move->moveBox]]);
			debug("Apply move %d<-X to game %p", move->moveBox, game);
			game->game_state[game->box_indexes[move->moveBox - 1]] = 'X';
			// debug("game->game_state[game->box_indexes[move->moveBox]] = %c", game->game_state[game->box_indexes[move->moveBox]]);
			// *(game->box_indexes[move->moveBox]) = 'X';
			// game->box_indexes[move->moveBox] = 'X';
			game->current_player = SECOND_PLAYER_ROLE;
			break;
		case SECOND_PLAYER_ROLE:
		// O
			game->game_board[move->moveBox - 1] = 1;
			// debug("move->moveBox = %d", move->moveBox);
			// debug("game->box_indexes[move->moveBox] = %d", game->box_indexes[move->moveBox]);
			// debug("game->game_state[game->box_indexes[move->moveBox]] = %c", game->game_state[game->box_indexes[move->moveBox]]);
			debug("Apply move %d<-O to game %p", move->moveBox, game);
			game->game_state[game->box_indexes[move->moveBox - 1]] = 'O';
			// debug("game->game_state[game->box_indexes[move->moveBox]] = %c", game->game_state[game->box_indexes[move->moveBox]]);
			// *(game->box_indexes[move->moveBox]) = 'O';
			// game->box_indexes[move->moveBox] = 'O';
			game->current_player = FIRST_PLAYER_ROLE;
			break;
		case NULL_ROLE:
			error("game_apply_move: move is NULL_ROLE");
			pthread_mutex_unlock(&game->mutex);
			return -1;
	}

	//check for row wins
	for(int i = 0; i < 9; i+= 3) {
		if((game->game_board[i] + game->game_board[i + 1] + game->game_board[i + 2]) == 3) {
			game->winner = SECOND_PLAYER_ROLE;
		} else if ((game->game_board[i] + game->game_board[i + 1] + game->game_board[i + 2]) == -3) {
			game->winner = FIRST_PLAYER_ROLE;
		}
	}
	//check for column wins
	for(int i = 0; i < 3; i++) {
		if((game->game_board[i] + game->game_board[i + 3] + game->game_board[i + 6]) == 3) {
			game->winner = SECOND_PLAYER_ROLE;
		} else if ((game->game_board[i] + game->game_board[i + 3] + game->game_board[i + 6]) == -3) {
			game->winner = FIRST_PLAYER_ROLE;
		}
	}
	//check for left to right diagonal wins
	if((game->game_board[0] + game->game_board[4] + game->game_board[8]) == 3) {
		game->winner = SECOND_PLAYER_ROLE;
	} else if ((game->game_board[0] + game->game_board[4] + game->game_board[8]) == -3) {
		game->winner = FIRST_PLAYER_ROLE;
	}
	//check for right to left diagonal wins
	if((game->game_board[2] + game->game_board[4] + game->game_board[6]) == 3) {
		game->winner = SECOND_PLAYER_ROLE;
	} else if ((game->game_board[2] + game->game_board[4] + game->game_board[6]) == -3) {
		game->winner = FIRST_PLAYER_ROLE;
	}

	int tie = 0;
	//check for draw
	for(int i = 0; i < 9; i++) {
		if(game->game_board[i] == 0) {
			break;
		} else if (i == 8) {
			// game->winner = NULL_ROLE;
			tie = 1;
		}
	}

	if(game->winner == NULL_ROLE && tie == 1) {
		game->winner = NULL_ROLE;
		game->current_player = NULL_ROLE;
		game->game_terminated = 1;
	} else if(game->winner != NULL_ROLE) {
		game->current_player = NULL_ROLE;
		game->game_terminated = 1;
	}
	// debug("game->game_state: %s", game->game_state);
	// int spaces = 0;
	// // char *currentChar = game->game_state[0];
	// int currIndex = 0;
	// while(game->game_state[currIndex] != '\0') {
	// 	if(game->game_state[currIndex] == ' ') {
	// 		spaces++;
	// 	}
	// 	if(spaces == move->moveBox) {
	// 		switch(move->player){
	// 			case FIRST_PLAYER_ROLE:
	// 				game->game_board[move->moveBox] = -1;
	// 				game->game_state[currIndex] = 'X';
	// 				break;
	// 			case SECOND_PLAYER_ROLE:
	// 				game->game_board[move->moveBox] = 1;
	// 				game->game_state[currIndex] = 'O';
	// 				break;
	// 		}
	// 		break;
	// 	}
	// 	currIndex++;
	// }
	pthread_mutex_unlock(&game->mutex);
	return 0;
}

/*
 * Submit the resignation of the GAME by the player in a specified
 * GAME_ROLE.  It is an error if the game has already terminated.
 *
 * @param game  The GAME to be resigned.
 * @param role  The GAME_ROLE of the player making the resignation.
 * @return 0 if resignation was successful, otherwise -1.
 */
int game_resign(GAME *game, GAME_ROLE role) {
	pthread_mutex_lock(&game->mutex);
	if(!game) {
		error("game_resign: game is NULL");
		pthread_mutex_unlock(&game->mutex);
		return -1;
	}

	if(game->game_terminated) {
		error("game_resign: game is already terminated");
		pthread_mutex_unlock(&game->mutex);
		return -1;
	}
	game->current_player = NULL_ROLE;
	game->winner = role == FIRST_PLAYER_ROLE ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE;
	game->game_terminated = 1;
	pthread_mutex_unlock(&game->mutex);
	return 0;
}

/*
 * Get a string that describes the current GAME state, in a format
 * appropriate for human users.  The returned string is in malloc'ed
 * storage, which the caller is responsible for freeing when the string
 * is no longer required.
 *
 * @param game  The GAME for which the state description is to be
 * obtained.
 * @return  A string that describes the current GAME state.
 */
char *game_unparse_state(GAME *game) {
	if(game->current_player == FIRST_PLAYER_ROLE) {
		int len = snprintf(NULL, 0, "%sX to move", game->game_state);
		char *str = malloc(len + 1);
		// snprintf(str, len + 1, "%s X to move", game->game_state);
		if(str != NULL) {
			snprintf(str, len + 1, "%sX to move", game->game_state);
			str[len] = '\0'; // Add null terminator
		}
		return str;
	} else if(game->current_player == SECOND_PLAYER_ROLE) {
		int len = snprintf(NULL, 0, "%sO to move", game->game_state);
		char *str = malloc(len + 1);
		if(str != NULL) {
			snprintf(str, len + 1, "%sO to move", game->game_state);
			str[len] = '\0'; // Add null terminator
		}
		return str;
	}
	return strdup(game->game_state);
}

/*
 * Determine if a specifed GAME has terminated.
 *
 * @param game  The GAME to be queried.
 * @return 1 if the game is over, 0 otherwise.
 */
int game_is_over(GAME *game) {
	return game->game_terminated;
}

/*
 * Get the GAME_ROLE of the player who has won the game.
 *
 * @param game  The GAME for which the winner is to be obtained.
 * @return  The GAME_ROLE of the winning player, if there is one.
 * If the game is not over, or there is no winner because the game
 * is drawn, then NULL_PLAYER is returned.
 */
GAME_ROLE game_get_winner(GAME *game) {
	return game->winner;
}

/*
 * Attempt to interpret a string as a move in the specified GAME.
 * If successful, a GAME_MOVE object representing the move is returned,
 * otherwise NULL is returned.  The caller is responsible for freeing
 * the returned GAME_MOVE when it is no longer needed.
 * Refer to the assignment handout for the syntax that should be used
 * to specify a move.
 *
 * @param game  The GAME for which the move is to be parsed.
 * @param role  The GAME_ROLE of the player making the move.
 * If this is not NULL_ROLE, then it must agree with the role that is
 * currently on the move in the game.
 * @param str  The string that is to be interpreted as a move.
 * @return  A GAME_MOVE described by the given string, if the string can
 * in fact be interpreted as a move, otherwise NULL.
 */
GAME_MOVE *game_parse_move(GAME *game, GAME_ROLE role, char *str) {
	pthread_mutex_lock(&game->mutex);
	if(!game) {
		error("game_parse_move: game is NULL");
		pthread_mutex_unlock(&game->mutex);
		return NULL;
	}
	if(!str) {
		error("game_parse_move: str is NULL");
		pthread_mutex_unlock(&game->mutex);
		return NULL;
	}
	// if(!game->game_terminated) {
	// 	error("game_parse_move: game is not terminated");
	// 	pthread_mutex_unlock(&game->mutex);
	// 	return NULL;
	// }
	if(role != NULL_ROLE) {
		if(role != game->current_player) {
			error("game_parse_move: role is not current player");
			pthread_mutex_unlock(&game->mutex);
			return NULL;
		}
	}

	debug("game_parse_move: str = %s", str);

	char num = str[0];

	if(num < '1' || num > '9' || strlen(str) != 1) {
		error("game_parse_move: num is not between 1 and 9");
		pthread_mutex_unlock(&game->mutex);
		return NULL;
	}

	GAME_MOVE *move;
	if(!(move = malloc(sizeof(GAME_MOVE)))) {
		error("game_parse_move: malloc failed");
		pthread_mutex_unlock(&game->mutex);
		return NULL;
	}

	*move = (GAME_MOVE) {
		.moveBox = num - '0',
		.player = role
	};

	pthread_mutex_unlock(&game->mutex);

	return move;
}

/*
 * Get a string that describes a specified GAME_MOVE, in a format
 * appropriate to be shown to human users.  The returned string should
 * be in a format from which the GAME_MOVE can be recovered by applying
 * game_parse_move() to it.  The returned string is in malloc'ed storage,
 * which it is the responsibility of the caller to free when it is no
 * longer needed.
 *
 * @param move  The GAME_MOVE whose description is to be obtained.
 * @return  A string describing the specified GAME_MOVE.
 */
char *game_unparse_move(GAME_MOVE *move) {
	if(!move) {
		error("game_unparse_move: move is NULL");
		return NULL;
	}

	char *str;
	if(!(str = malloc(sizeof(char) * 2))) {
		error("game_unparse_move: malloc failed");
		return NULL;
	}


	str[0] = move->moveBox + '0';
	str[1] = '\0';

	// debug("game_unparse_move: str = %s", str);

	return str;
}