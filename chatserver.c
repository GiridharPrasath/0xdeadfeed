#include<stdio.h>
#include<arpa/inet.h>
#include<string.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<sys/epoll.h>
#include<errno.h>
#include<fcntl.h>
#include<unistd.h>

# define MAX_CLIENTS 100
# define BUFFER_SIZE 100
# define PORT_NO 7000
# define MAXEVENTS 128

typedef struct conninfo {
    int sockfd;
    struct sockaddr_in address;
    char username[20];
}
Conninfo;

void setnonblocking(int sock)
{
	int opts;
	opts = fcntl(sock, F_GETFL);

	if(opts < 0) {
		perror("fcntl(sock, GETFL)");
		exit(1);
	}

	opts = opts | O_NONBLOCK;

	if(fcntl(sock, F_SETFL, opts) < 0) {
		perror("fcntl(sock, SETFL, opts)");
		exit(1);
	}
}

int initserver(Conninfo * server) 
{
    if ((server -> sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket error\n");
        exit(EXIT_SUCCESS);
    }
    printf("Socket Created\n");
    server -> address.sin_family = AF_INET;
    server -> address.sin_addr.s_addr = INADDR_ANY;
    server -> address.sin_port = htons(PORT_NO);
    if (bind(server -> sockfd, (const struct sockaddr * ) & server -> address, sizeof(server -> address)) < 0) {
        perror("Bind failed");
        exit(EXIT_SUCCESS);
    }
    int val = 1;
    socklen_t vallen = sizeof(val);
    if (setsockopt(server -> sockfd, SOL_SOCKET, SO_REUSEADDR, (void * ) & val, vallen) < 0) {
        perror("sock readdir failed");
        exit(EXIT_SUCCESS);
    }
    if (listen(server -> sockfd, 5) < 0) {
        perror("Listen error");
        exit(EXIT_SUCCESS);
    }
    printf("Listening on port: %d \n", htons(server -> address.sin_port));
    puts("Waiting for incoming connections");
    return server -> sockfd;
}

int main(void) 
{
    struct epoll_event event,events[3];
    puts("Starting server...");
    Conninfo server;
    Conninfo clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].sockfd = 0;
    }
    int sockfd = initserver( & server);
    setnonblocking(sockfd);
    char sendline[BUFFER_SIZE];
    int epollfd = epoll_create(0xbeef);
    event.data.fd = sockfd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event) < 0) {
        perror("Epoll error");
        exit(EXIT_SUCCESS);
    }
    int fd;
    for (;;) {
        int nfds, i, newsockfd,n ;
        nfds = epoll_wait(epollfd, events, MAXEVENTS, -1);
        for (i = 0; i < nfds; i++) {
            fd = events[i].data.fd;
            if (fd == sockfd) {
                socklen_t addrlen = (socklen_t) sizeof clients[i].address;
                int acceptfd = accept(sockfd, (struct sockaddr * ) & clients[i].address, &addrlen);
                printf("Accepted %s\n", inet_ntoa(clients[i].address.sin_addr));
                setnonblocking(acceptfd);
                event.data.fd = acceptfd;
				event.events = EPOLLIN;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, acceptfd, &event);
                if (acceptfd < 0) {
                    if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
                        perror("accept");
                }
                continue;
            }
            else if(events[i].events & EPOLLIN) {
				if((newsockfd = events[i].data.fd) < 0) 
				    continue;
				if((n = read(newsockfd, sendline, BUFFER_SIZE)) < 0) {
					if(errno == ECONNRESET) {
						close(newsockfd);
						events[i].data.fd = -1;
					} else {
						printf("readline error");
					}
				} else if(n == 0) {
					perror("Epoll error");
					events[i].data.fd = -1;
				}
				printf("received data: %s \n", sendline);
				memset(sendline , 0 ,sizeof(sendline));
			}
			else {
                write(sockfd, "server message", 10);
            }
        }
    }
}
