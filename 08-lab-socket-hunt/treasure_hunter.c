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

	if (argc < 5) {
		fprintf(stderr, "Usage: %s server port level seed\n", argv[0]);
		return 1;
	}

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
	printf("\n");

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

	printf("\n");

	if (bytes_sent == -1) {
		perror("sendto\n");
	} else {
		printf("Send %zd bytes to %s:%s\n", bytes_sent, server, port_str);
	}

	printf("\n");

	// Receive response using recvfrom
	unsigned char response[256];
	struct sockaddr_storage sender_addr;
	socklen_t sender_addr_len = sizeof(sender_addr);
	ssize_t bytes_received = recvfrom(sockfd, response, sizeof(response), 0, (struct sockaddr *)&sender_addr, &sender_addr_len);
	printf("\n");
	if (bytes_received == -1) {
		perror("recvfrom");
	} else {
		printf("Received %zd bytes from server\n", bytes_received);
		print_bytes(response, bytes_received);

		unsigned char chunklen = response[0];

		if (chunklen == 0) {
			printf("Hunt is over. All chunks received\n");
			close(sockfd);
			return 0;
		} else if (chunklen > 127) {
			switch (chunklen) {
				case 0x81: printf("Error: Message sent from an unexpected port.\n"); break;
				case 0x82: printf("Error: Message sent to the wrong port.\n"); break;
				case 0x83: printf("Error: Incorrect message length.\n"); break;
				case 0x84: printf("Error: Incorrect nonce value.\n"); break;
				case 0x85: printf("Error: Server failed to bind to address/port.\n"); break;
				case 0x86: printf("Error: No remote port available for binding.\n"); break;
				case 0x87: printf("Error: Bad level or initial byte in request.\n"); break;
				case 0x88: printf("Error: Bad user ID in request.\n"); break;
				case 0x89: printf("Error: Unknown error occurred.\n"); break;
				case 0x8a: printf("Error: Wrong address family used.\n"); break;
				default: printf("Error: Unrecognized error code.\n"); break;
        	}
			close(sockfd);
			return 1;
		}

		// Extract the treasure chunk
		char chunk[128];
		memcpy(chunk, &response[1], chunklen);
		// Null-terminate to print as a string
		chunk[chunklen] = '\0';

		// Extract the op-code, op-param, and nonce
		unsigned char opcode = response[chunklen + 1];
		unsigned char opparam = ntohs(*(unsigned short *)&response[chunklen + 2]);
		unsigned int nonce = ntohl(*(unsigned int *)&response[chunklen + 4]);

		// Print extracted values for verification
		printf("\n");
		printf("Chunk Length: %x\n", chunklen);
		printf("Treasure Chunk: %s\n", chunk);
		printf("Op Code: %x\n", opcode);
		printf("Op Param: %x\n", opparam);  // Ignored for level 0
		printf("Nonce: %x\n", nonce);

		unsigned int next_nonce = nonce + 1;
		unsigned char follow_up_message[4];
		next_nonce = htonl(next_nonce);
		memcpy(follow_up_message, &next_nonce, sizeof(next_nonce));

		// printf("\n");
		print_bytes(follow_up_message, 4);

		ssize_t bytes_sent = sendto(sockfd, follow_up_message, sizeof(follow_up_message), 0, remote_addr, sizeof(struct sockaddr_storage));

		if (bytes_sent == - 1) {
			perror("sendto");
			close(sockfd);
			return 1;
		}
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
