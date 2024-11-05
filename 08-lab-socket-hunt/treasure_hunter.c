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

	// Checkpoint 1
	unsigned char message[8];
	bzero(message, 8); 	// initialize buf to 0
	message[0] = 0;
	message[1] = level;

	unsigned int user_id = htonl(USERID);
	memcpy(&message[2], &user_id, sizeof(user_id));

	unsigned short seed_network = htons(seed);
	memcpy(&message[6], &seed_network, sizeof(seed_network));

	// Checkpoint 2
    struct addrinfo hints, *res, *rp;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	char remote_ip[INET6_ADDRSTRLEN];
    unsigned short remote_port;
	unsigned short local_port = 0;
	
	int s = getaddrinfo(server, port_str, &hints, &res);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		return 1;
	}

	int sockfd;
	struct sockaddr_storage remote_addr_ss, local_addr_ss;
	struct sockaddr *remote_addr = (struct sockaddr *)&remote_addr_ss;
    struct sockaddr *local_addr = (struct sockaddr *)&local_addr_ss;

	for (rp = res; rp != NULL; rp = rp->ai_next) {
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd == -1) continue;
		memcpy(remote_addr, rp->ai_addr, sizeof(struct sockaddr_storage));

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

		// Checkpoint 3
		unsigned char chunklen = response[0];
		if (chunklen == 0) {
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

		// Checkpoint 7
        if (opcode == 1) {
            remote_port = opparam;
            populate_sockaddr(remote_addr, rp->ai_family, remote_ip, remote_port);
        } else if (opcode == 2) { // Checkpoint 8
			local_port = opparam;
			close(sockfd);

			sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (sockfd == -1) {
				perror("socket creation error after op-code 2\n");
				freeaddrinfo(res);
				return 1;
			}

			populate_sockaddr(local_addr, rp->ai_family, NULL, local_port);
			if (bind(sockfd, local_addr, sizeof(struct sockaddr_storage)) < 0) {
                perror("bind error after op-code 2\n");
                freeaddrinfo(res);
                close(sockfd);
                return 1;
            }
		} else if (opcode == 3) { // Checkpoint 9
			unsigned int nonce_sum = 0;
			for (int i = 0; i < opparam; i++) {
				struct sockaddr_storage temp_addr;
				socklen_t temp_addr_len = sizeof(temp_addr);

				ssize_t temp_bytes_received = recvfrom(sockfd, NULL, 0, 0, (struct sockaddr *)&temp_addr, &temp_addr_len);
				if (temp_bytes_received == -1) {
					perror("recvfrom error for op-code 3\n");
                    freeaddrinfo(res);
                    close(sockfd);
                    return 1;
				}

				struct sockaddr_in *temp_in = (struct sockaddr_in *)&temp_addr;
				unsigned short temp_port = ntohs(temp_in->sin_port);
				nonce_sum += temp_port;
			}

			nonce = nonce_sum;
        }

		unsigned int next_nonce = htonl(nonce + 1);
		unsigned char follow_up_message[4];
		memcpy(follow_up_message, &next_nonce, sizeof(next_nonce));

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
