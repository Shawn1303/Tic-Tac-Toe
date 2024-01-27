#include "server.h"
#include "jeux_globals.h"
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "debug.h"

/*
 * Client registry that should be used to track the set of
 * file descriptors of currently connected clients.
 */
// extern CLIENT_REGISTRY *client_registry;
// CLIENT_REGISTRY *client_registry;


/*
 * Thread function for the thread that handles a particular client.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This pointer must be freed once the file
 * descriptor has been retrieved.
 * @return  NULL
 *
 * This function executes a "service loop" that receives packets from
 * the client and dispatches to appropriate functions to carry out
 * the client's requests.  It also maintains information about whether
 * the client has logged in or not.  Until the client has logged in,
 * only LOGIN packets will be honored.  Once a client has logged in,
 * LOGIN packets will no longer be honored, but other packets will be.
 * The service loop ends when the network connection shuts down and
 * EOF is seen.  This could occur either as a result of the client
 * explicitly closing the connection, a timeout in the network causing
 * the connection to be closed, or the main thread of the server shutting
 * down the connection as part of graceful termination.
 */
void *jeux_client_service(void *arg) {
	int fd = *(int *)arg;
	free(arg);

	pthread_detach(pthread_self());

	// CLIENT_REGISTRY *cr = client_registry;
	debug("%ld: [%d] Starting client service", pthread_self(), fd);

	CLIENT *client;
	if(!(client = creg_register(client_registry, fd))) {
		error("Failed to register client");
		close(fd);
		return NULL;
	}

	JEUX_PACKET_HEADER *hdr;
	if(!(hdr = calloc(1, sizeof(JEUX_PACKET_HEADER)))) {
	// if(!(hdr = malloc(sizeof(JEUX_PACKET_HEADER)))) {
		error("Failed to allocate memory for packet header");
		creg_unregister(client_registry, client);
		close(fd);
		return NULL;
	}

	// memset(hdr, 0, sizeof(JEUX_PACKET_HEADER));
	// *hdr = (JEUX_PACKET_HEADER) {
	// 	.type = JEUX_LOGIN_PKT,
	// 	.id = 0,
	// 	.role = 0,
	// 	.size = 0,
	// };

	// client_send_packet(client, hdr, NULL);

	void *payload = NULL;

	int nack_flag = 0;
	int EOF_flag = 0;
	struct timespec ts;
	while(!(proto_recv_packet(fd, hdr, &payload))) {
		if(payload) {
			void *payload_tmp;
			if((payload_tmp = realloc(payload, ntohs(hdr->size) + 1))) {
				payload = payload_tmp;
			} 
			char *payload_str = (char *)payload;
			payload_str[ntohs(hdr->size)] = '\0';
			// debug("payload[ntohs(hdr->size)] = %c", payload_str[ntohs(hdr->size)]);
			// debug("payload[ntohs(hdr->size)] = %d", payload_str[ntohs(hdr->size)]);
			// debug("payload[ntohs(hdr->size) - 1] = %c", payload_str[ntohs(hdr->size) - 1]);
			// debug("payload[ntohs(hdr->size) - 1] = %d", payload_str[ntohs(hdr->size) - 1]);
			// debug("ntohs(hdr->size) = %d", ntohs(hdr->size));
		}
		switch(hdr->type) {
			case JEUX_LOGIN_PKT:
				if(payload) {
					debug("<= %u.%u: type=LOGIN, size=%u, id=%u, role=%u, payload=[%s]", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role, (char *)payload);
				} else {
					debug("<= %u.%u: type=LOGIN, size=%u, id=%u, role=%u, (no payload)", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role);
				}
				debug("%ld: [%d] LOGIN packet received", pthread_self(), fd);

				// debug("hdr->timestamp_sec = %u", hdr->timestamp_sec);
				// debug("hdr->timestamp_nsec = %u", hdr->timestamp_nsec);

				// uint32_t timestamp_sec_prev = hdr->timestamp_sec;
				// uint32_t timestamp_nsec_prev = hdr->timestamp_nsec; 


				if(!client_get_player(client)) {
					PLAYER *player;
					// debug("hdr->timestamp_sec = %u", hdr->timestamp_sec);
					// debug("hdr->timestamp_nsec = %u", hdr->timestamp_nsec);
					// debug("timestamp_sec_prev = %u", timestamp_sec_prev);
					// debug("timestamp_nsec_prev = %u", timestamp_nsec_prev);
					// debug("hiiiiiiiiiiiiiiiii");
					// if((player = player_create(payload))) {
					if((player = preg_register(player_registry, (char *)payload))) {
						// debug("hiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
						// debug("hdr->timestamp_sec = %u", hdr->timestamp_sec);
						// debug("hdr->timestamp_nsec = %u", hdr->timestamp_nsec);
						// debug("timestamp_sec_prev = %u", timestamp_sec_prev);
						// debug("timestamp_nsec_prev = %u", timestamp_nsec_prev);
						// struct timespec ts;
						// clock_gettime(CLOCK_MONOTONIC, &ts);
						if(client_login(client, player) == 0) {
							// *hdr = (JEUX_PACKET_HEADER) {
							// 	.type = JEUX_ACK_PKT,
							// 	.id = 0,
							// 	.role = 0,
							// 	.size = 0,
							// 	// .timestamp_sec = timestamp_sec_prev,
							// 	// .timestamp_nsec = timestamp_nsec_prev
							// 	.timestamp_sec = htonl((uint32_t)ts.tv_sec),
							// 	.timestamp_nsec = htonl((uint32_t)ts.tv_nsec)
							// };
							// hdr->type = JEUX_ACK_PKT;
							// hdr->id = 0;
							// hdr->role = 0;
							// hdr->size = 0;
							// hdr->timestamp_sec = htonl((uint32_t)ts.tv_sec);
							// hdr->timestamp_nsec = htonl((uint32_t)ts.tv_nsec);
							// if(client_send_ack(client, NULL, 0) < 0) {
							// debug("hdr->timestamp_sec = %u", hdr->timestamp_sec);
							// debug("hdr->timestamp_nsec = %u", hdr->timestamp_nsec);
							// debug("timestamp_sec_prev = %u", timestamp_sec_prev);
							// debug("timestamp_nsec_prev = %u", timestamp_nsec_prev);
							// if(client_send_packet(client, hdr, NULL) < 0) {
							// 	error("Failed to send ACK packet");
							// 	EOF_flag = 1;
							// }
							if(client_send_ack(client, NULL, 0) < 0) {
								error("Failed to send ACK packet");
								EOF_flag = 1;
							}
							// debug("=> %u.%u: type=ACK, size=%u, id=%u, role=%u, (no payload)", 
							// ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role);
							break;
						} else {
							nack_flag = 1;
						}
					} else {
						nack_flag = 1;
					}
				} else {
					debug("%ld: [%d] Already logged in (player %p [%s])", pthread_self(), fd, client_get_player(client), player_get_name(client_get_player(client)));
					nack_flag = 1;
				}
				break;
			case JEUX_USERS_PKT:
				if(payload) {
					debug("<= %u.%u: type=USERS, size=%u, id=%u, role=%u, payload=[%s]", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role, (char *)payload);
				} else {
					debug("<= %u.%u: type=USERS, size=%u, id=%u, role=%u, (no payload)", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role);
				}
				debug("%ld: [%d] USERS packet received", pthread_self(), fd);

				if(!client_get_player(client)) {
					debug("%ld: [%d] Login required", pthread_self(), fd);
					nack_flag = 1;
					break;
				}

				PLAYER** players_list = creg_all_players(client_registry);
				payload = NULL;
				int i = 0;
				PLAYER *curr_player = players_list[i];;
				debug("%ld: [%d] Users", pthread_self(), fd);
				while(curr_player) {
					//making the line
					int len = snprintf(NULL, 0, "%s\t%d\n", player_get_name(curr_player), player_get_rating(curr_player));
					char *line;
					if(!(line = malloc(len + 1))) {
						error("Failed to allocate memory for line");
						// EOF_flag = 1;
						nack_flag = 1;
						break;
					}
					line[len] = '\0';
					snprintf(line, len, "%s\t%d\n", player_get_name(curr_player), player_get_rating(curr_player));
					//combine the lines
					if(!payload) {
						payload = strdup(line);
					} else {
						char *temp;
						if(!(temp = realloc(payload, strlen((char *)payload) + strlen(line) + 2))) {
							error("Failed to reallocate memory for payload");
							// EOF_flag = 1;
							nack_flag = 1;
							break;
						}
						// temp[strlen(payload) + strlen(line)] = '\0';
						// payload = temp;
						// strcat((char *)payload, line);
						strcat(temp, "\n");
						// memcpy(temp, payload, strlen((char *)payload));
						strcpy(temp + strlen(temp), line);
						// temp[strlen((char *)payload) + strlen(line)] = '\0';
						payload = temp;
					}
					// debug("payload: %s", (char *)payload);
					free(line);
					players_list[i] = NULL;
					player_unref(curr_player, "for player removed from players list");
					i++;
					curr_player = players_list[i];
				}
				free(players_list);
				// debug("payload: %s", (char *)payload);
				if(client_send_ack(client, payload, strlen((char *)payload) + 1) < 0) {
					error("Failed to send ACK packet");
					EOF_flag = 1;
				}
				// debug("=> %u.%u: type=ACK, size=%u, id=%u, role=%u, payload=[%s]", 
				// ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role, (char *)payload);
				break;
			case JEUX_INVITE_PKT:
				if(payload) {
					debug("<= %u.%u: type=INVITE, size=%u, id=%u, role=%u, payload=[%s]", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role, (char *)payload);
				} else {
					debug("<= %u.%u: type=INVITE, size=%u, id=%u, role=%u, (no payload)", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role);
				}
				debug("%ld: [%d] INVITE packet received", pthread_self(), fd);

				if(!client_get_player(client)) {
					debug("%ld: [%d] Login required", pthread_self(), fd);
					nack_flag = 1;
					break;
				}

				debug("%ld: [%d] Invite '%s'", pthread_self(), fd, (char *)payload);

				CLIENT *target;
				if((target = creg_lookup(client_registry, (char *)payload))) {
					// debug("%ld: [%d] Make an invitation", pthread_self(), fd);
					// client_ref(client, "as source of new invitation");
					// client_ref(target, "as target of new invitation");
					int inv_ID;
					GAME_ROLE source_role;
					GAME_ROLE target_role;
					if(hdr->role == 1) {
						target_role = FIRST_PLAYER_ROLE;
						source_role = SECOND_PLAYER_ROLE;
					} else {
						target_role = SECOND_PLAYER_ROLE;
						source_role = FIRST_PLAYER_ROLE;
					} 
					if((inv_ID = client_make_invitation(client, target, source_role, target_role)) < 0) {
						debug("%ld: [%d] Failed to create invitation", pthread_self(), fd);
						client_unref(target, "after invitation attempt");
						// EOF_flag = 1;
						nack_flag = 1;
						break;
					}

					client_unref(target, "after invitation attempt");
					// debug("%ld: [%d] Add invitation as source", pthread_self(), fd);
					// struct timespec start_time;
					// clock_gettime(CLOCK_REALTIME, &start_time);
					// *hdr = (JEUX_PACKET_HEADER) {
					// 	.type = JEUX_ACK_PKT,
					// 	.size = 0,
					// 	.id = inv_ID,
					// 	.role = 0,
					// 	// .timestamp_sec = htonl((uint32_t)start_time.tv_sec),
					// 	// .timestamp_nsec = htonl(start_time.tv_nsec)
					// 	// .timestamp_sec = ntohl((uint32_t)start_time.tv_sec),
					// 	// .timestamp_nsec = ntohl((uint32_t)start_time.tv_nsec)
					// };
					// hdr->type = JEUX_ACK_PKT;
					// hdr->id = inv_ID;
					// hdr->role = 0;
					// hdr->size = 0;
					// JEUX_PACKET_HEADER *temphdr;
					// if(!(temphdr = calloc(1, sizeof(JEUX_PACKET_HEADER)))) {
					// 	error("Failed to allocate memory for ACK packet header");
					// 	EOF_flag = 1;
					// 	break;
					// }
					// *temphdr = (JEUX_PACKET_HEADER) {
					// 	.type = JEUX_ACK_PKT,
					// 	.size = 0,
					// 	.id = inv_ID,
					// 	.role = 0
					// };
					// if(client_send_packet(client, temphdr, NULL) < 0) {
					clock_gettime(CLOCK_MONOTONIC, &ts);
					// *hdr = (JEUX_PACKET_HEADER) {
					// 	.type = JEUX_ACK_PKT,
					// 	.id = inv_ID,
					// 	.role = 0,
					// 	.size = 0,
					// 	.timestamp_sec = htonl((uint32_t)ts.tv_sec),
					// 	.timestamp_nsec = htonl((uint32_t)ts.tv_nsec)
					// };
					// free(hdr);
					// hdr = malloc(sizeof(JEUX_PACKET_HEADER));
					*hdr = (JEUX_PACKET_HEADER) {
						.type = JEUX_ACK_PKT,
						.id = inv_ID,
						.role = 0,
						.size = 0,
						.timestamp_sec = htonl((uint32_t)ts.tv_sec),
						.timestamp_nsec = htonl((uint32_t)ts.tv_nsec)
					};
					if(client_send_packet(client, hdr, NULL) < 0) {
						error("Failed to send ACK packet");
						EOF_flag = 1;
						break;
					}
					// free(temphdr);
				} else {
					debug("%ld: [%d] No client logged in as user 'u'", pthread_self(), fd);
					nack_flag = 1;
					break;
				}

				break;
			case JEUX_REVOKE_PKT:
				if(payload) {
					debug("<= %u.%u: type=REVOKED, size=%u, id=%u, role=%u, payload=[%s]", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role, (char *)payload);
				} else {
					debug("<= %u.%u: type=REVOKED, size=%u, id=%u, role=%u, (no payload)", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role);
				}
				debug("%ld: [%d] REVOKED packet received", pthread_self(), fd);

				if(!client_get_player(client)) {
					debug("%ld: [%d] Login required", pthread_self(), fd);
					nack_flag = 1;
					break;
				}

				debug("%ld: [%d] Revoke '%d'", pthread_self(), fd, hdr->id);

				if(client_revoke_invitation(client, hdr->id) < 0) {
					// error("Failed to revoke invitation");
					// debug("%ld: [%d] )
					// EOF_flag = 1;
					nack_flag = 1;
					break;
				}

				if(client_send_ack(client, NULL, 0) < 0) {
					error("Failed to send ACK packet");
					EOF_flag = 1;
					break;
				}

				break;
			case JEUX_ACCEPT_PKT:
				if(payload) {
					debug("<= %u.%u: type=ACCEPTED, size=%u, id=%u, role=%u, payload=[%s]", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role, (char *)payload);
				} else {
					debug("<= %u.%u: type=ACCEPTED, size=%u, id=%u, role=%u, (no payload)", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role);
				}
				debug("%ld: [%d] ACCEPTED packet received", pthread_self(), fd);

				if(!client_get_player(client)) {
					debug("%ld: [%d] Login required", pthread_self(), fd);
					nack_flag = 1;
					break;
				}

				debug("%ld: [%d] Accept '%d'", pthread_self(), fd, hdr->id);

				char *strp = NULL;
				if(client_accept_invitation(client, hdr->id, &strp) < 0) {
					// error("Failed to accept invitation");
					// EOF_flag = 1;
					nack_flag = 1;
					break;
				}

				// debug("strp = %s", strp ? strp : "(null)");

				if(strp) {
					// *hdr = (JEUX_PACKET_HEADER) {
					// 	.type = JEUX_ACK_PKT,
					// 	.id = 0,
					// 	.role = 0,
					// 	.size = 0,
					// };
					if(client_send_ack(client, strp, strlen(strp) + 1) < 0) {
						error("Failed to send ACCEPTED packet");
						EOF_flag = 1;
						break;
					}
					free(strp);
					// if(client_send_packet(client, hdr, strp) < 0) {
					// 	error("Failed to send ACCEPTED packet");
					// 	EOF_flag = 1;
					// 	break;
					// }
				} else {
					if(client_send_ack(client, NULL, 0) < 0) {
						error("Failed to send ACCEPTED packet");
						EOF_flag = 1;
						break;
					}
				}


				// *hdr = (JEUX_PACKET_HEADER) {
				// 	.type = JEUX_ACK_PKT,
				// 	.id = 0,
				// 	.role = 0,
				// 	.size = 0,
				// };
				// if(client_send_packet(client, hdr, strp) < 0) {
				// 	error("Failed to send ACCEPTED packet");
				// 	EOF_flag = 1;
				// 	break;
				// }

				break;
			case JEUX_DECLINE_PKT:
				if(payload) {
					debug("<= %u.%u: type=DECLINED, size=%u, id=%u, role=%u, payload=[%s]", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role, (char *)payload);
				} else {
					debug("<= %u.%u: type=DECLINED, size=%u, id=%u, role=%u, (no payload)", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role);
				}
				debug("%ld: [%d] DECLINED packet received", pthread_self(), fd);

				if(!client_get_player(client)) {
					debug("%ld: [%d] Login required", pthread_self(), fd);
					nack_flag = 1;
					break;
				}

				debug("%ld: [%d] Decline '%d'", pthread_self(), fd, hdr->id);

				if(client_decline_invitation(client, hdr->id) < 0) {
					// error("Failed to decline invitation");
					// EOF_flag = 1;
					nack_flag = 1;
					break;
				}

				if(client_send_ack(client, NULL, 0) < 0) {
					error("Failed to send ACK packet");
					EOF_flag = 1;
					break;
				}

				break;
			case JEUX_MOVE_PKT:
				if(payload) {
					debug("<= %u.%u: type=MOVE, size=%u, id=%u, role=%u, payload=[%s]", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role, (char *)payload);
				} else {
					debug("<= %u.%u: type=MOVE, size=%u, id=%u, role=%u, (no payload)", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role);
				}
				debug("%ld: [%d] MOVE packet received", pthread_self(), fd);

				if(!client_get_player(client)) {
					debug("%ld: [%d] Login required", pthread_self(), fd);
					nack_flag = 1;
					break;
				}

				debug("%ld: [%d] Move '%d' (%s)", pthread_self(), fd, hdr->id, (char *)payload);

				if(client_make_move(client, hdr->id, payload) < 0) {
					// error("Failed to make move");
					// EOF_flag = 1;
					nack_flag = 1;
					break;
				}

				if(client_send_ack(client, NULL, 0) < 0) {
					error("Failed to send ACK packet");
					EOF_flag = 1;
					break;
				}

				break;
			case JEUX_RESIGN_PKT:
				if(payload) {
					debug("<= %u.%u: type=RESIGN, size=%u, id=%u, role=%u, payload=[%s]", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role, (char *)payload);
				} else {
					debug("<= %u.%u: type=RESIGN, size=%u, id=%u, role=%u, (no payload)", 
					ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), ntohs(hdr->size), hdr->id, hdr->role);
				}
				debug("%ld: [%d] RESIGN packet received", pthread_self(), fd);

				if(!client_get_player(client)) {
					debug("%ld: [%d] Login required", pthread_self(), fd);
					nack_flag = 1;
					break;
				}

				debug("%ld: [%d] Resign '%d'", pthread_self(), fd, hdr->id);

				if(client_resign_game(client, hdr->id) < 0) {
					// error("Failed to resign game");
					// EOF_flag = 1;
					nack_flag = 1;
					break;
				}

				if(client_send_ack(client, NULL, 0) < 0) {
					error("Failed to send ACK packet");
					EOF_flag = 1;
					break;
				}

				break;
			default:
				break;
		}

		if(payload) {
			free(payload);
			payload = NULL;
		}

		if(nack_flag) {
			nack_flag = 0;
			if(client_send_nack(client) < 0) {
				error("Failed to send NACK packet");
				break;
			}
		}

		if(EOF_flag) {
			break;
		}
	}
	if(payload) {
		free(payload);
		payload = NULL;
	}
	free(hdr);
	if(client_get_player(client)) {
		player_unref(client_get_player(client), "because server thread is discarding reference to logged in player");
		debug("%ld: [%d] Logging out client", pthread_self(), fd);
		client_logout(client);
	}
	creg_unregister(client_registry, client);
	debug("%ld: [%d] Ending client service", pthread_self(), fd);
	close(fd);
	return NULL;
}