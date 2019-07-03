#include<stdio.h>
#include<arpa/inet.h>
#include<string.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<sys/epoll.h>
#include<errno.h>
#include<fcntl.h>
#include <stdbool.h>
#include<unistd.h>
#include<iostream>
#include "chatmessage.pb.h"

# define MAX_CLIENTS 100
# define BUFFER_SIZE 100
# define PORT_NO 7000
# define MAXEVENTS 128

using namespace std; 

typedef struct conninfo {
    char username[20];
    struct sockaddr_in address;
    struct conninfo *next;
    struct conninfo *prev;
}
Conninfo;

void PrintMessage(const chatApp::Message& msg) {

    printf("reading....\n");

    cout << "Host: " << msg.hostname() << " " << msg.sourceip() << endl;

    if (msg.has_destip()) {
        cout << "  Destination IP: " << msg.destip() << endl;
    }

    cout << "Message length: " << msg.messagelength() << endl;

    switch (msg.opt()) {
        case chatApp::Message::LISTCLIENTS:
            cout << "  LISTCLIENTS ";
            break;

        case chatApp::Message::CHAT:
            cout << "  CHAT ";
            break;

        case chatApp::Message::RESPONSE:
            cout << "  RESPONSE ";
            break;
    }

    cout << "Done" << endl;
}
Conninfo *head;

Conninfo* create_conn_info() {
    head = (Conninfo *) malloc(sizeof(Conninfo));
    if(head != NULL) {
        head->next = NULL;
        head->prev = NULL;
    }
    return head;
}

void add_conn_info(Conninfo** newconnect) {
    if(head == NULL) {
        head = create_conn_info();
        (*newconnect) = head;
    }
    else {
    (*newconnect) = (Conninfo *)malloc(sizeof (Conninfo));
    (*newconnect)->next = head;
    (*newconnect)->prev = NULL;
    head->prev = (*newconnect);               
    head = (*newconnect);
    }
}

void delete_conn_info(Conninfo **del) 
{ 
    (*del) = head;
    head = head->next;
    if(head != NULL) {
        head->prev = NULL;
    }
    free((*del)); 
    return; 
} 

bool isconnected(Conninfo **cur, char *ip) {
    while((*cur)->next != NULL) {
        if(!(strcmp(inet_ntoa((*cur)->address.sin_addr), ip))) {
            return true;
        }
            (*cur)=(*cur)->next;
    }
    return false;
}

/*char *search_list(Conninfo *connect) {

}*/
void printList(Conninfo *connect)  
{ 
    while (connect != NULL)  
    {  
        printf("Connected %s \n",(inet_ntoa(connect->address.sin_addr)));
        connect = connect->next;
    }
}  
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
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    
    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket error\n");
        exit(EXIT_SUCCESS);
    }

    printf("Socket Created\n");
    server -> address.sin_family = AF_INET;
    server -> address.sin_addr.s_addr = INADDR_ANY;
    server -> address.sin_port = htons(PORT_NO);
    int val = 1;
    socklen_t vallen = sizeof(val);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void * ) & val, vallen) < 0) {
        perror("sock readdir failed");
        exit(EXIT_SUCCESS);
    }

    if (bind(sockfd, (const struct sockaddr * ) & server -> address, sizeof(server -> address)) < 0) {
        perror("Bind failed");
        exit(EXIT_SUCCESS);
    }

    if (listen(sockfd, 5) < 0) {
        perror("Listen error");
        exit(EXIT_SUCCESS);
    }

    printf("Listening on port: %d \n", htons(server -> address.sin_port));
    puts("Waiting for incoming connections");
    return sockfd;
}

int main(void) 
{

    chatApp::Message msg;
    head = NULL;
    struct epoll_event event,events[3];
    puts("Starting server...");
    Conninfo server;
    Conninfo *clients;
    int sockfd = initserver( &server);
    setnonblocking(sockfd);
    char sendline[BUFFER_SIZE];
    char *cliaddr; 
    int epollfd = epoll_create(0xbeef);
    event.data.fd = sockfd;
    event.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event) < 0) {
        perror("Epoll error");
        exit(EXIT_SUCCESS);
    }


    int fd;
    
    for (;;) {
        int nfds, newsockfd,n ;
        nfds = epoll_wait(epollfd, events, MAXEVENTS, -1);
        for(int i =0;i<nfds;i++) {
        fd = events[i].data.fd;     
        
        if (fd == sockfd) {
            add_conn_info(&clients);
            
            socklen_t addrlen = (socklen_t) sizeof(const struct sockaddr_in);
            int acceptfd = accept(sockfd, (struct sockaddr * ) & clients->address, &addrlen);    
            if(acceptfd < 0){
                perror("Accept failed");
                continue;
            }
            
            cliaddr = inet_ntoa(clients->address.sin_addr);
            
            if(isconnected(&clients, cliaddr)) {
                delete_conn_info(&clients);
                perror("Already connected");
                close(acceptfd);
                continue;
            }
            printList(head); 
            printf("Accepted %s\n", inet_ntoa(clients->address.sin_addr));
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
					printf("Disconnected from %s", inet_ntoa(clients->address.sin_addr));
					delete_conn_info(&clients);
					close(newsockfd);
				} else {
					printf("readline error");
				}
				continue;
			} else if(n == 0) {
				printf("Disconnected from %s\n", inet_ntoa(clients->address.sin_addr));
		        delete_conn_info(&clients);
				close(newsockfd);
				continue;
			}
            if(!msg.ParseFromString(&sendline[BUFFER_SIZE])) {
                perror("Failed to read from client");
            }
            PrintMessage(msg);
			//printf("received data: %s\n", sendline);
			memset(&sendline , 0 ,sizeof(sendline));
		}
		else if(events[i].events & EPOLLOUT){
            write(events[i].data.fd, "server message", 10);
            }   
        }
    }
}
