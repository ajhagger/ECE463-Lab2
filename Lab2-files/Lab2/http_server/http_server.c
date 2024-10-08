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

char* handle_request(char*);

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
    int sockfd, new_fd, received, bytes;
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
	
	printf("Server listening on port %d\n", SERVPORT);
	while(1) {
		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd,
        (struct sockaddr *) &their_addr, &sin_size)) < 0) {
			perror("accept");
			continue;
		}
        received = 0;
        do {
            bytes = recv(new_fd, request+received, 1, MSG_WAITALL);
            if (bytes < 0)
                perror("ERROR reading request from socket");
            if (bytes == 0)
                break;
            if (strstr(request, "\r\n\r\n"))
                break;
            received += bytes;
        } while(1);
		token = strtok(request, "\r\n");
		fprintf(stdout, "%s", token);
		strcpy(response, handle_request(request));
        close(new_fd);
    }

	close(sockfd);

    return 0;
}

char* handle_request(char *request){
	char* response;
	char* url;
	char *not_implemented = "HTTP/1.0 501 Not Implemented\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>";
	char *bad_request = "HTTP/1.0 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>";
	fprintf(stdout, "Received request: \n%s\n", request);

	if (!strstr(request, "HTTP/1.0") || !strstr(request, "HTTP/1.1"))
		return not_implemented;

	if (strstr(request, "GET")) {
		url = strstr(request, "GET") + 4;
		if (strncmp("/", url, 1))
			return bad_request;
		if (strstr(url, ".."))
			return bad_request;
		if (strrchr(url, '/')) {

		}
		
		char* response = "HTTP/1.0 200 OK\r\n\r\n<html><body><h1>This is a valid get request</h1></body></html>";
		
		return response;
	}
	else
		return not_implemented;
}