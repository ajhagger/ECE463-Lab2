/* The code is subject to Purdue University copyright policies.
 * Do not share, distribute, or post online.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>

#define LISTEN_QUEUE 50 /* Max outstanding connection requests; listen() param */

#define DBADDR "127.0.0.1"

void handle_request(char*, int);
void send_response(int, char*, int);

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: ./http_server [server port] [DB port]\n");
        exit(1);
    }
    int SERVPORT = atoi(argv[1]);
	char* token;
    char *not_imp_msg = "HTTP/1.0 501 NotImplemented \r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>";
    char request[1000];
	char response[1000];
    int sockfd, new_fd, received, bytes, response_len, sent;
	struct sockaddr_in my_addr;
	struct sockaddr_in their_addr; /* client's address info */
	int sin_size;
	char dst[INET_ADDRSTRLEN];

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(SERVPORT);
	my_addr.sin_addr.s_addr = INADDR_ANY; /* bind to all local interfaces */
	bzero(&(my_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) < 0) {
		perror("bind");
		close(sockfd);
		exit(1);
	}

	if (listen(sockfd, LISTEN_QUEUE) < 0) {
		perror("listen");
		close(sockfd);
		exit(1);
	}
	
	while(1) {
		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd,
        (struct sockaddr *) &their_addr, &sin_size)) < 0) {
			perror("accept");
			continue;
		}
        received = 0;
		memset(request, 0, sizeof(request));
        do {
            bytes = recv(new_fd, request+received, 1, MSG_WAITALL);
            if (bytes < 0) {
                perror("ERROR reading request from socket");
			}
            if (bytes == 0) {
                break;
			}
			if (strstr(request, "\r\n\r\n")) {
				break;
			}
            received += bytes;
        } while(1);
		fprintf(stdout, "%s \"", inet_ntoa(their_addr.sin_addr));
		token = strtok(request, "\r\n");
		fprintf(stdout, "%s\" ", token);
		handle_request(request, new_fd);
        close(new_fd);
    }

	close(sockfd);
	return 0;
}

void handle_request(char *request, int fd) {
	int bytes, total_sent, sent,response_len, url_len;
	char* response;
	char* url;
	char buf[4096]; 
	char filepath[512] = "Webpage";
	char *not_found = "HTTP/1.0 404 Not Found\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>";
	char *not_implemented = "HTTP/1.0 501 Not Implemented\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>";
	char *bad_request = "HTTP/1.0 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>";
	

	if (!(strstr(request, "HTTP/1.0") || strstr(request, "HTTP/1.1"))) {
		fprintf(stdout, "501 Not Implemented\r\n");
		send_response(fd, not_implemented, strlen(not_implemented));
	}

	if (strstr(request, "GET")) {
		url = strstr(request, "GET") + 4;
		url = strtok(url, " ");
		url_len = strlen(url);
		if (strncmp("/", url, 1) != 0) {
			fprintf(stdout, "400 Bad Request\r\n");
			send_response(fd, bad_request, strlen(bad_request));
		}
		if (strstr(url, "/..")) {
			fprintf(stdout, "400 Bad Request\r\n");
			send_response(fd, bad_request, strlen(bad_request));
		}

		if (strcmp(url+url_len-1, "/") == 0) {
			strcat(url, "index.html");
		}
		
		char* response = "HTTP/1.0 200 OK\r\n\r\n";
		strcat(filepath, url);
		FILE* fptr;
		fptr = fopen(filepath, "rb");

		if (fptr == NULL) {
			fprintf(stdout, "404 Not Found \r\n");
			send_response(fd, not_found, strlen(not_found));
			return;
		}
		fprintf(stdout, "200 OK\r\n");
		send_response(fd, response, strlen(response));

		do {
			bytes = fread(buf, 1, sizeof(buf), fptr);
			if (bytes < 0) {
				perror("Error reading data from file");
				exit(1);
			}
			if (bytes == 0) {
				break;
			}

			send_response(fd, buf, bytes);
			memset(buf, 0, sizeof(buf));
		} while(1);
		fclose(fptr);
	}
}

void send_response(int fd, char* response, int response_len) {
	int sent = 0;
	int bytes;

	do {
		bytes = write(fd, response + sent, response_len - sent);
		if (bytes < 0)
			perror("ERROR writing response to socket");
		if (bytes == 0)
			break;
		sent += bytes;
	} while (1);
}
