#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

#define PORT 8068
#define MESS_LEN 8000
#define MAX_POLLS 10000
#define ROOT "./www"

struct pollfd pollfds[MAX_POLLS];
int count = 1;
int current_size = 0;

int fd, connection;
int quit = 1;
char message[MESS_LEN] = {0};


int main () {

	struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *results;
    int rc = getaddrinfo(NULL, "8068", &hints, &results);
    if (rc != 0) {
        perror("getaddrinfo failed");
    }

    struct addrinfo *r;
    	for (r = results; r != NULL; r = r->ai_next) {
             fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
             if (fd == -1)
                continue;  // failed

             int en = 1;
             if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(int)) == -1) {
                 perror("setsockopt reuseaddr"); // failure to se sock option
                 exit(0); 
             }

             if (ioctl(fd, FIONBIO, (char*) &en) == -1) {
                 perror("ioctl"); // failure to be non-blocking
                 exit(0);
             }
                          
                          
             if (bind(fd, r->ai_addr, r->ai_addrlen) == 0) {
                 break;
             } else {
                 perror("bind error"); // failure to bind socket
                 exit(0);
             }
                          
             close(fd);  // failed to match the socket
             fd = -1;
        }

        if (listen(fd, SOMAXCONN) == -1) {
            perror("listen error"); // failure to listen 
            close(fd);
            exit(0);
        }
            	
        /*
         *The poll function begins here.
         *It loops through all the connections
         *and parses the correct response or fails to poll
         */
        memset(pollfds, 0, sizeof(pollfds));
        pollfds[0].fd = fd;
        pollfds[0].events = POLLIN;

        while (quit) {
        	int num_readable = poll(pollfds, count, 100);
            if (num_readable == -1) {
            	perror("poll failed"); // failure to poll
            	exit(0);
            }

            if (pollfds[0].revents & POLLIN) {
                connection = accept(fd, NULL, NULL);
                if (connection == -1 && errno != EWOULDBLOCK) {
                	perror("accept fails"); // failure to accept connection to client
                	exit(0);
                }
            
                pollfds[count].fd = connection;
                pollfds[count].events = POLLIN;
                count++;
            }
            
            for (int i = 1; i < count; i++) {
                 if (pollfds[i].revents & POLLIN) {
                 	int client = pollfds[i].fd;
                 	int mess = recv(client, message, MESS_LEN, 0);
                 	if (mess == 0) {
                 		close(client); // failure to receive message
                 		pollfds[i].fd = -1;
                 	} else { 
                 	
                 	 	// Parse HTTP request
    					char *method = strtok(message, " ");
    					char *uri = strtok(NULL, " ");
    					char *version = strtok(NULL, "\n");

						// Sets up the go-to page to index.html					
    					if (strncmp(uri, "/", 1) == 0 && (strlen(uri) == 1)) {
    						uri = "/index.html";
    					}
    					
    					char path[MESS_LEN] = {0};
    					snprintf(path, MESS_LEN, "%s%s", ROOT, uri); //copies over the correct url with the www directory
    					    						
    					FILE *fp = fopen(path, "r");
    					if (fp == NULL) {
    						char *response = "HTTP/1.1 404 Not Found\n\n";
    						send(client, response, strlen(response), 0);
    						close(client);
						    pollfds[i].fd = -1; 
						    
    					} else {
    						fseek(fp, 0, SEEK_END);
    						int size = ftell(fp);
    						rewind(fp);
    						char response[MESS_LEN + size];
    						memset(response, 0, MESS_LEN + size*sizeof(char));
    						char *content_type;
    						if (strstr(path, ".html")) {
    							content_type = "text/html";
    						} else if (strstr(path, ".css")) {
								content_type = "text/css";
							} else if (strstr(path, ".xml")) {
								content_type = "text/xml";
    						} else if (strstr(path, ".png")) {
    							// This correctly loops through and reads all bytes of the image so that it prints correctly on the page
    							snprintf(response, MESS_LEN + size, "HTTP/1.1 200 OK\nContent-Type:");
    							content_type = "image/png";
    							strcat(response, content_type);
    							strcat(response, "\n\n");
    							int bytes;
    							while ((bytes = fread(response + strlen(response), 1, size, fp)) > 0) {
    							        send(client, response, bytes, 0);
    							}
    						} else if (strstr(path, ".jpg")) {
    							snprintf(response, MESS_LEN + size, "HTTP/1.1 200 OK\nContent-Type:");
    							content_type = "image/jpeg";
    							strcat(response, content_type);
    							strcat(response, "\n\n");
    							int bytes;
    							while ((bytes = fread(response + strlen(response), 1, size, fp)) > 0) {
    							        send(client, response, bytes, 0);
    							}
    						} else {
    							content_type = "text/plain"; //standard content-type
    						}
    						
    						snprintf(response, MESS_LEN + size, "HTTP/1.1 200 OK\nContent-Type: %s\nContent-Length: %d\n\n", content_type, size);
    					    fread(response + strlen(response), size, 1, fp);
							send(client, response, strlen(response), 0);
    				   	}
    				   fclose(fp);
    				   close(client);
    				   pollfds[i].fd = -1; //closes poll connection
    				   }
    			}
    	}
    }
    freeaddrinfo(results);
    return(0);
}
