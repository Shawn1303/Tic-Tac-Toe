#include "protocol.h"
#include "debug.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

/*
 * Send a packet, which consists of a fixed-size header followed by an
 * optional associated data payload.
 *
 * @param fd  The file descriptor on which packet is to be sent.
 * @param hdr  The fixed-size packet header, with multi-byte fields
 *   in network byte order
 * @param data  The data payload, or NULL, if there is none.
 * @return  0 in case of successful transmission, -1 otherwise.
 *   In the latter case, errno is set to indicate the error.
 *
 * All multi-byte fields in the packet are assumed to be in network byte order.
 */
int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data) {
	// debug("fd: %d", fd);
	// debug("proto_send_packet called");
	debug("=> %u.%u: type=%u, size=%u, id=%u, role=%u,", 
	ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), hdr->type, ntohs(hdr->size), hdr->id, hdr->role);
	// hdr->timestamp_sec, hdr->timestamp_nsec, hdr->type, hdr->size, hdr->id, hdr->role);
	// debug("hdr->type: %u", hdr->type);
	// debug("hdr->id: %u", hdr->id);
	// debug("hdr->role: %u", hdr->role);
	// debug("hdr->size: %u", hdr->size);
	// debug("hdr->timestamp_sec: %u", hdr->timestamp_sec);
	// debug("hdr->timestamp_nsec: %u", hdr->timestamp_nsec);
	// debug("hi");
	//convert to network byte order
	// debug("hdr->size: %d", hdr->size);
	// debug("hdr->size: %d", hdr->size);
	// hdr->size = htons(hdr->size);
	// hdr->timestamp_sec = htonl(hdr->timestamp_sec);
	// hdr->timestamp_nsec = htonl(hdr->timestamp_nsec);

	//write header
	ssize_t bytes_written = 0;
	size_t bytes_to_write = sizeof(JEUX_PACKET_HEADER);
	char *buffer = (char *)hdr;


	while(bytes_written < bytes_to_write) {
		ssize_t results = write(fd, buffer + bytes_written, bytes_to_write - bytes_written);
		if(results < 0) {
			// fprintf(stderr, "Error writing header to socket: %s\n", strerror(errno));
			error("Error writing header to socket: %s", strerror(errno));
			return -1;
			// break;
		} else {
			bytes_written += results;
		}
	}

	//reset them
	// debug("hdr->size: %d", hdr->size);
	// hdr->size = ntohs(hdr->size);
	// hdr->timestamp_sec = ntohl(hdr->timestamp_sec);
	// hdr->timestamp_nsec = ntohl(hdr->timestamp_nsec);
	// debug("hdr->size: %d", hdr->size);
	// hdr->size = htons(hdr->size);
	// hdr->size = ntohs(hdr->size);
	// debug("hdr->size: %d", hdr->size);
	// hdr->size = htons(hdr->size);
	// debug("hdr->size: %d", hdr->size);
	// hdr->timestamp_sec = htonl(hdr->timestamp_sec);
	// hdr->timestamp_nsec = htonl(hdr->timestamp_nsec);

	// debug("hdr->size: %d", hdr->size);
	// debug("htons(hdr->size): %d", htons(hdr->size));
	// debug("ntohs(hdr->size): %d", ntohs(hdr->size));
	// debug("payload: %s", (char *)data);

	//write payload
	if(data && ntohs(hdr->size) > 0) {
		// debug("hi");
		bytes_written = 0;
		// bytes_to_write = hdr->size;
		bytes_to_write = ntohs(hdr->size);
		buffer = (char *)data;
		// debug("data: %s", buffer);

		while(bytes_written < bytes_to_write) {
			ssize_t results = write(fd, buffer + bytes_written, bytes_to_write - bytes_written);
			// debug("results: %d", results);
			// debug("bytes_to_write: %d", bytes_to_write);
			if(results < 0) {
				// fprintf(stderr, "Error writing data to socket: %s\n", strerror(errno));
				error("Error writing data to socket: %s\n", strerror(errno));
				return -1;
			} else {
				bytes_written += results;
			}
		}
		debug("payload=[%s]", (char *)data);
	} else {
		debug("(no payload)");
	}
	buffer = NULL;

	return 0;
}

/*
 * Receive a packet, blocking until one is available.
 *
 * @param fd  The file descriptor from which the packet is to be received.
 * @param hdr  Pointer to caller-supplied storage for the fixed-size
 *   packet header.
 * @param datap  Pointer to a variable into which to store a pointer to any
 *   payload received.
 * @return  0 in case of successful reception, -1 otherwise.  In the
 *   latter case, errno is set to indicate the error.
 *
 * The returned packet has all multi-byte fields in network byte order.
 * If the returned payload pointer is non-NULL, then the caller has the
 * responsibility of freeing that storage.
 */
int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp) {
	// debug("proto_recv_packet called");
	// debug("bye");
	// debug("fd: %d", fd);
	//read header
	ssize_t bytes_read = 0;
	size_t bytes_to_read = sizeof(JEUX_PACKET_HEADER);
	char *buffer = (char *)hdr;

	while(bytes_read < bytes_to_read) {
		// debug("byeeeeeeeee");
		size_t results = read(fd, buffer + bytes_read, bytes_to_read - bytes_read);
		// debug("results from header: %d", results);
		// debug("buffer: %s", buffer);
		if(results < 0) {
			// fprintf(stderr, "Error reading header from socket: %s\n", strerror(errno));
			error("Error reading header from socket: %s\n", strerror(errno));
			return -1;
		} else if(results == 0) {
			// fprintf(stderr, "Socket closed, read EOF in header\n");
			// debug("Socket closed, read EOF in header\n");
			debug("%ld: EOF on fd: %d", pthread_self(), fd);
			return -1;
		} else {
			bytes_read += results;
		}
	}

	// debug("<= %u.%u: type=%u, size=%u, id=%u, role=%u,",
	// ntohl(hdr->timestamp_sec), ntohl(hdr->timestamp_nsec), hdr->type, ntohs(hdr->size), hdr->id, hdr->role);

	//convert to host byte order
	// hdr->size = ntohs(hdr->size);
	// hdr->timestamp_sec = ntohl(hdr->timestamp_sec);
	// hdr->timestamp_nsec = ntohl(hdr->timestamp_nsec);

	// debug("receiving");
	// debug("hdr->type: %u", hdr->type);
	// debug("hdr->id: %u", hdr->id);
	// debug("hdr->role: %u", hdr->role);
	// debug("hdr->size: %u", hdr->size);
	// debug("hdr->timestamp_sec: %u", hdr->timestamp_sec);
	// debug("hdr->timestamp_nsec: %u", hdr->timestamp_nsec);

	//read payload
	// if(hdr->size > 0) {
	if(ntohs(hdr->size) > 0) {
		// debug("hi");
		// if(!(*payloadp = malloc(hdr->size + 1))) {
		if(!(*payloadp = malloc(ntohs(hdr->size) + 1))) {
			// fprintf(stderr, "Error allocating memory for payload: %s\n", strerror(errno));
			error("Error allocating memory for payload: %s\n", strerror(errno));
			return -1;
		}
		// debug("hi");
		((char *)(*payloadp))[ntohs(hdr->size)] = '\0'; //null terminate the array

		bytes_read = 0;
		// bytes_to_read = hdr->size;
		bytes_to_read = ntohs(hdr->size);
		buffer = (char *)*payloadp;

		while(bytes_read < bytes_to_read) {
			ssize_t results = read(fd, buffer + bytes_read, bytes_to_read - bytes_read);
			// debug("results from payload: %d", results);
			// debug("bytes_to_read: %d", bytes_to_read);
			// debug("buffer: %s", buffer);
			if(results < 0) {
				// fprintf(stderr, "Error reading data: %s\n", strerror(errno));
				error("Error reading data: %s\n", strerror(errno));
				return -1;
			} else if(results == 0) {
				// fprintf(stderr, "Socket closed, read EOF in payload\n");
				// debug("Socket closed, read EOF in payload\n");
				debug("%ld: EOF on fd: %d", pthread_self(), fd);
				return -1;
			} else {
				bytes_read += results;
			}
		}
		buffer = NULL;
		// debug("payload: %s", (char *)(*payloadp));
		// debug("payload=[%s]", (char *)(*payloadp));
	} 
	// else {
	// 	debug("(no payload)");
	// }
	// debug("received packet");

	return 0;
}