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

void handle_request(char*, int, int);
void send_response(int, char*, int);
void replace_char(char*, char, char);
void query_db (int, char*, int);

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: ./http_server [server port] [DB port]\n");
        exit(1);
    }
    int SERVPORT = atoi(argv[1]);
	int DBPORT = atoi(argv[2]);
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
		handle_request(request, new_fd, DBPORT);
        close(new_fd);
    }

	close(sockfd);
	return 0;
}

void handle_request(char *request, int fd, int db_port) {
	int bytes, total_sent, sent,response_len, url_len;
	char* response;
	char* url;
	char* query;
	char buf[4096]; 
	char filepath[512] = "Webpage";
	char *not_found = "HTTP/1.0 404 Not Found\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>";
	char *not_implemented = "HTTP/1.0 501 Not Implemented\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>";
	char *bad_request = "HTTP/1.0 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>";
	struct stat path_stat;
	
	

	if (!(strstr(request, "HTTP/1.0") || strstr(request, "HTTP/1.1"))) {
		fprintf(stdout, "501 Not Implemented\r\n");
		send_response(fd, not_implemented, strlen(not_implemented));
		return;
	}

	if (!strstr(request, "GET")) {
		fprintf(stdout, "501 Not Implemented\r\n");
		send_response(fd, not_implemented, strlen(not_implemented));
		return;
	}

	if (strstr(request, "GET")) {
		url = strstr(request, "GET") + 4;
		url = strtok(url, " ");
		url_len = strlen(url);
		stat(url, &path_stat);
		if (strncmp("/", url, 1) != 0) {
			fprintf(stdout, "400 Bad Request\r\n");
			send_response(fd, bad_request, strlen(bad_request));
			return;
		}
		if (strstr(url, "/..")) {
			fprintf(stdout, "400 Bad Request\r\n");
			send_response(fd, bad_request, strlen(bad_request));
			return;
		}

		if (strcmp(url+url_len-1, "/") == 0) {
			strcat(url, "index.html");
		}
		if (S_ISDIR(path_stat.st_mode) && strcmp(url+url_len-1, "/") != 0) {
			strcat(url, "/index.html");
		}
		
		if (strstr(url, "?key=")) {
			query = strstr(url, "?key=") + 5;
			replace_char(query, '+', ' ');
			query_db(fd, query, db_port);
			return;
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

void replace_char(char *str, char oldChar, char newChar) {
    int i = 0;
    while (str[i] != '\0') {
        if (str[i] == oldChar) {
            str[i] = newChar;
        }
        i++;
    }
}

void query_db (int fd, char* query, int db_port) {
	int db_fd, bytes, addr_len;
	char buf[4096];
	struct sockaddr_in their_addr;
	struct sockaddr_in my_addr;
	char *not_found = "HTTP/1.0 404 Not Found\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>";
	char *ok = "HTTP/1.0 200 OK\r\n\r\n";

	if ((db_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("ERROR creating socket");
		exit(1);
	}

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(db_port);
	their_addr.sin_addr.s_addr = INADDR_ANY;

	bytes = sendto(db_fd, query, strlen(query), MSG_CONFIRM, (const struct sockaddr *)&their_addr, sizeof(their_addr));

	addr_len = sizeof(struct sockaddr);
	bytes = recvfrom(db_fd, buf, sizeof(buf), MSG_WAITALL, (struct sockaddr *)&their_addr, &addr_len);
	if (strcmp(buf, "File Not Found") == 0) {
		fprintf(stdout, "404 Not Found\r\n");
		send_response(fd, not_found, strlen(not_found));
		close(db_fd);
		return;
	}
	else {
		send_response(fd, ok, strlen(ok));
		send_response(fd, buf, bytes);
		memset(buf, 0, sizeof(buf));
	}

	do {
		bytes = recvfrom(db_fd, buf, sizeof(buf), MSG_WAITALL, (struct sockaddr *)&their_addr, &addr_len);
		if (strcmp(buf, "DONE") == 0) {
			break;
		}
		if (bytes < 0) {
			perror("ERROR reading from db");
			close(db_fd);
			exit(1);
		}
		if (bytes == 0) {
			break;
		}
		send_response(fd, buf, bytes);
		memset(buf, 0 , sizeof(buf));
	} while(1);

	fprintf(stdout, "200 OK\r\n");

	close(db_fd);
}
