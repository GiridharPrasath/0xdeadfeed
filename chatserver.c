#include<stdio.h>

#include<arpa/inet.h>

#include<string.h>

#include<sys/socket.h>

#include<stdlib.h>

#include<sys/epoll.h>

#include<errno.h>

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

int initserver(Conninfo * server) 
{
    memset(server, 0, sizeof(Conninfo));
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
    struct epoll_event event, events[MAX_CLIENTS];
    puts("Starting server...");
    Conninfo server;
    Conninfo clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].sockfd = 0;
    }
    int sockfd = initserver( & server);
    int epollfd = epoll_create(0xbeef);
    event.data.fd = sockfd;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, & event) < 0) {
        perror("Epoll error");
        exit(EXIT_SUCCESS);
    }
    int fd, acceptfd;
    for (;;) {
        int nfds, i;
        nfds = epoll_wait(epollfd, events, MAXEVENTS, -1);
        for (i = 0; i < nfds; i++) {
            fd = events[i].data.fd;
            if (fd == sockfd) {
                printf("Listening\n");
                socklen_t * addrlen = (socklen_t *) sizeof clients[i];
                while (!((acceptfd = accept(sockfd, (struct sockaddr * ) & clients[i], addrlen)) < 0)) {
                    printf("Accepted %s\n", inet_ntoa(clients[i].address.sin_addr));
                    events[i].events = EPOLLIN | EPOLLOUT | EPOLLET;
                    events[i].data.fd = acceptfd;
                    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, acceptfd, events) < 0) {
                        perror("epoll_ctl: add");
                        exit(EXIT_FAILURE);
                    }
                }
                if (acceptfd < 0) {
                    if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
                        perror("accept");
                }
                continue;
            } else {
                printf("not listening");
            }
        }
    }
}
