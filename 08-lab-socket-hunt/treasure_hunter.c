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
	// Checkpoint 0
	char *server = argv[1];
	char *port_str = argv[2];
	int port = atoi(port_str);
	int level = atoi(argv[3]);
	int seed = atoi(argv[4]);

	// printf("server: %s\n", server);
	// printf("port (string): %s\n", port_str);
	// printf("port (int): %d\n", port);
	// printf("level: %d\n", level);
	// printf("seed: %d\n", seed);

	// Checkpoint 1
	unsigned char message[8];

	// initialize buf to 0
	bzero(message, 8);

	message[0] = 0;
	message[1] = level;

	unsigned int user_id = htonl(USERID);
	memcpy(&message[2], &user_id, sizeof(user_id));

	unsigned short seed_network = htons(seed);
	memcpy(&message[6], &seed_network, sizeof(seed_network));

	// print_bytes(message, 8);
	// printf("\n");

	// Checkpoint 2
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

		char remote_ip[INET6_ADDRSTRLEN];
		unsigned short remote_port;
	    parse_sockaddr((struct sockaddr *)rp->ai_addr, remote_ip, &remote_port);
		populate_sockaddr(remote_addr, rp->ai_family, remote_ip, remote_port);
		break;
	}

	// Send message using sendto
	ssize_t bytes_sent = sendto(sockfd, message, sizeof(message), 0, remote_addr, sizeof(struct sockaddr_storage));
	if (bytes_sent == -1) {
		perror("sendto error\n");
		freeaddrinfo(res);
		close(sockfd);
		return 1;
	}
	// else {
	// 	printf("Sent %zd bytes to %s: %s\n", bytes_sent, server, port_str);
	// }

	char treasure[1024] = {0};
    int treasure_length = 0;

	while (1) {
		// Receive response using recvfrom
		unsigned char response[256];
		struct sockaddr_storage sender_addr;
		socklen_t sender_addr_len = sizeof(sender_addr);
		ssize_t bytes_received = recvfrom(sockfd, response, sizeof(response), 0, (struct sockaddr *)&sender_addr, &sender_addr_len);

		if (bytes_received == -1) {
			perror("recvfrom");
			freeaddrinfo(res);
			close(sockfd);
			return 1;
		}
		// else {
		// 	printf("Received %zd bytes from server\n", bytes_received);
		// 	printf("\n");
		// 	print_bytes(response, bytes_received);
		// 	printf("\n");
		// }

		// Checkpoint 3
		unsigned char chunklen = response[0];
		if (chunklen == 0) {
			// printf("The hunt is over. All chunks received.\n");
			break;
		} else if (chunklen > 127) {
			printf("Error code received: 0x%x\n", chunklen);
			freeaddrinfo(res);
			close(sockfd);
			return 1;
		}

		char chunk[128];
		memcpy(chunk, &response[1], chunklen);
		chunk[chunklen] = '\0';
		
		// Append chunk to treasure
		strcat(treasure, chunk);
		treasure_length += chunklen;

		unsigned char opcode = response[chunklen + 1];
		unsigned short opparam = ntohs(*(unsigned short *)&response[chunklen + 2]);
		unsigned int nonce = ntohl(*(unsigned int *)&response[chunklen + 4]);

		// printf("Chunk Length: %x\n", chunklen);
		// printf("Treasure Chunk: %s\n", chunk);
		// printf("Op Code: %x\n", opcode);
		// printf("Op Param: %x\n", opparam);  // Ignored for level 0
		// printf("Nonce: %x\n", nonce);
		
		unsigned int next_nonce = htonl(nonce + 1);
		unsigned char follow_up_message[4];
		memcpy(follow_up_message, &next_nonce, sizeof(next_nonce));

		// print_bytes(follow_up_message, 4);  // Debug: Ensure correct format of follow-up message
        bytes_sent = sendto(sockfd, follow_up_message, sizeof(follow_up_message), 0, remote_addr, sizeof(struct sockaddr_storage));

        if (bytes_sent == -1) {
            perror("sendto error on follow-up");
            freeaddrinfo(res);
            close(sockfd);
            return 1;
		}
	}

	printf("%s\n", treasure);

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
