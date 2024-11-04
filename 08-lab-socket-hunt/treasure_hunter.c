// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1823703846

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "sockhelper.h"

int verbose = 0;

void print_bytes(unsigned char *bytes, int byteslen);

/*
port, level, and seed are numerical values and should be stored as type int
keep a string verision of port as well (needed for getaddrinfo())

store each argument provided on command line to main() in variables
*/
int main(int argc, char *argv[]) {
	char *server = argv[1];

	char *port_str = argv[2];
	int port = atoi(port_str);

	int level = atoi(argv[3]);
	int seed = atoi(argv[4]);

	printf("Server: %s\n", server);
	printf("Port (string): %s\n", port_str);
	printf("Port (int): %d\n", port);
	printf("Level: %d\n", level);
	printf("Seed: %d\n", seed);

	unsigned char message[8];

	message[0] = 0;
	message[1] = (unsigned char)level;

	unsigned int user_id = htonl(USERID);
	memcpy(&message[2], &user_id, sizeof(user_id));

	unsigned short seed_network = htons(seed);
	memcpy(&message[6], &seed_network, sizeof(seed_network));

	print_bytes(message, 8);

    struct addrinfo hints, *res, *rp;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	
	int s = getaddrinfo(server, port_str, &hints, &res);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		return 1;
	}

	int sockfd;

	struct sockaddr_storage remote_addr_ss;
	struct sockaddr *remote_addr = (struct sockaddr *)&remote_addr_ss;

	for (rp = res; rp != NULL; rp = rp->ai_next) {
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd == -1) continue;

		memcpy(remote_addr, rp->ai_addr, sizeof(struct sockaddr_storage));
		break;
	}

	// Send message using sendto
	ssize_t bytes_sent = sendto(sockfd, message, sizeof(message), 0, remote_addr, sizeof(struct sockaddr_storage));
	if (bytes_sent == -1) {
		perror("sendto");
	} else {
		printf("Send %zd bytes to %s:%s\n", bytes_sent, server, port_str);
	}

	// Receive response using recvfrom
	unsigned char response[256];
	struct sockaddr_storage sender_addr;
	socklen_t sender_addr_len = sizeof(sender_addr);
	ssize_t bytes_received = recvfrom(sockfd, response, sizeof(response), 0, (struct sockaddr *)&sender_addr, &sender_addr_len);
	if (bytes_received == -1) {
		perror("recvfrom");
	} else {
		printf("Received %zd bytes from server\n", bytes_received);
		print_bytes(response, bytes_received);
	}

	// Clean up
	freeaddrinfo(res);
	close(sockfd);

	return 0;
}

/*
see what is in a given message that is about to be sent or has just been received

called by providing:
- pointer to a memory location ex. an array of unsigned char
- a length

then prints the hexadecimal value for each byte, as well as the ASCII character equivalent for values less than 128 (very similar to memprint())
*/
void print_bytes(unsigned char *bytes, int byteslen) {
	int i, j, byteslen_adjusted;

	if (byteslen % 8) {
		byteslen_adjusted = ((byteslen / 8) + 1) * 8;
	} else {
		byteslen_adjusted = byteslen;
	}
	for (i = 0; i < byteslen_adjusted + 1; i++) {
		if (!(i % 8)) {
			if (i > 0) {
				for (j = i - 8; j < i; j++) {
					if (j >= byteslen_adjusted) {
						printf("  ");
					} else if (j >= byteslen) {
						printf("  ");
					} else if (bytes[j] >= '!' && bytes[j] <= '~') {
						printf(" %c", bytes[j]);
					} else {
						printf(" .");
					}
				}
			}
			if (i < byteslen_adjusted) {
				printf("\n%02X: ", i);
			}
		} else if (!(i % 4)) {
			printf(" ");
		}
		if (i >= byteslen_adjusted) {
			continue;
		} else if (i >= byteslen) {
			printf("   ");
		} else {
			printf("%02X ", bytes[i]);
		}
	}
	printf("\n");
	fflush(stdout);
}
