#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <poll.h>
#include <arpa/inet.h>
#include "chatmessage.pb-c.h"

#define PORT    7000
#define MAXBUF  256
#define noproc  2

char    hostname[MAXBUF];
char    *IP_ADDR;
struct  hostent *host_entry;
struct  sockaddr_in addr,client;

/* This function fills in a message based on user input. */ 
void PromptForMessage(ChatApp__Message* msg,int opt) {
    uint8_t size,option;
    char text[MAXBUF],ip[MAXBUF];
    printf("HOST: %s\n",hostname);
    msg->hostname = hostname;

    printf("IP address: %s\n",IP_ADDR);
    msg->sourceip = IP_ADDR;

    size_t cur_len = 0;
    if(opt != 1){

        printf("Enter destination ip address or (blank to finish)\n");

        cur_len = read(0,ip,MAXBUF);
        printf("length %d\n",cur_len);
        if (cur_len > 0){
            msg->destip = ip;
        }
    }
    printf("Enter your message or (blank to finish)\n");

    cur_len = read(0,text,MAXBUF);
    if (cur_len > 0){
        msg->texttosend = text;
    }
    printf("text length %d\n",cur_len);
    msg->messagelength = cur_len;

    printf("option %d\n",opt);
    switch (opt) {
        case 1:
            msg->opt = 0;
            break;

        case 2:
            printf("selected to CHAT\n");
            msg->opt = 1;
            break;

        default:
            msg->opt = 0;
    }
}

void PrintMessage(const ChatApp__Message* msg) {

    printf("Hostname: %s (%s)", msg->hostname, msg->sourceip);

    if (msg->destip != '\0') {
        printf("Destination IP: %s\n",msg->destip);
    }

    if (msg->texttosend != '\0') {
        printf("Text: %s\n",msg->texttosend);
    }
    printf("Message length: %d\n", msg->messagelength);

    switch (msg->opt) {
        case 0:
            printf("  LISTCLIENTS ");
            break;

        case 1:
            printf( "  CHAT ");
            break;

        case 2:
            printf("RESPONSE");
            break;
    }

}
int main(int argc, char*argv[]){

    if (gethostname(hostname,MAXBUF) < 0) {
        perror("Could not get host name");
        return 0;
    }
    if((host_entry = gethostbyname(hostname)) == NULL){
        perror("Could not get host's IP address");
        return 0;
    }
    IP_ADDR = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); 

    struct      epoll_event events[4];
    struct      epoll_event ev = {0};

    uint8_t     sockfd,connfd,i,max = 0;
    uint8_t     id = getpid();
    uint16_t    epfd = epoll_create(10);
    uint8_t     opt;

    uint_fast8_t        buffer[MAXBUF];
    void *      buf;

    ev.data.fd = 0;     /* fd for stdin */
    ev.events = EPOLLIN;
    epoll_ctl(epfd,EPOLL_CTL_ADD,ev.data.fd,&ev);

    sockfd  = socket(AF_INET,SOCK_STREAM,0);
    if (sockfd < 0) {
        printf("Socket creation error");
        exit(0);
    }
    printf("Socket created for id: %d \n",id);

    addr.sin_family = AF_INET;
    addr.sin_port   = htons(PORT);
    //    inet_pton(AF_INET, "10.145.0.245", &addr.sin_addr); 
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr); 

    if ((connfd = connect(sockfd,(struct sockaddr*)&addr,sizeof(addr))) < 0) {
        printf("connection error \n Client couldn't connect.\n");
        exit(0);
    }
    printf("%d connected to server!\n",id);

    struct epoll_event sock_event ={0};

    sock_event.data.fd  = sockfd;
    sock_event.events   = EPOLLIN;

    epoll_ctl(epfd,EPOLL_CTL_ADD,sock_event.data.fd,&sock_event);

    unsigned len;
    ChatApp__Message  msg = CHAT_APP__MESSAGE__INIT;
    for (;;) {

    printf("Enter your option \n 1.List all clients \n 2.Chat \n");
        uint8_t nready = epoll_wait(epfd,events,1,10000);

        for (uint8_t k = 0; k < nready; k++) {
            if (events[k].data.fd == 0) {
                /* stdin is ready */
                scanf("%hhd", &opt);
                PromptForMessage(&msg, opt);
                len = chat_app__message__get_packed_size(&msg);
                buf = malloc(len);
                chat_app__message__pack(&msg,buf);

                fprintf(stderr,"Writing %d serialized bytes\n",len); // See the length of message
                fwrite(buf,len,1,stdout); // Write to stdout to allow direct command line piping


                printf("\nMessage recorded.\n");
                int sentbytes = send(sockfd,buf,len,0);
                if (sentbytes < 0) {
                    printf("Nothing sent.\n");
                }
                printf("%d sent!\n",sentbytes);

                free(buf); // Free the allocated serialized buffer
            } else{
                //socket is ready
                if (events[k].events & EPOLLIN) {

                    memset(&buffer,0,MAXBUF);
                    uint16_t nbytes = read(events[k].data.fd, buffer, MAXBUF);
                    if (nbytes <= 0) {
                        close(events[k].data.fd);
                        printf("Data not recieved yet... or server is down!\n");
                        continue;
                    } else {
                        printf("recieved:\t %s \n", buffer);
                    }
                } else if (events[k].events == 0) {
                    printf("Socket closing\n");
                    epoll_ctl(epfd,EPOLL_CTL_DEL,events[k].data.fd,&events[k]);
                }
            }
        }

        if (errno == ECONNRESET) {
            printf("Server terminated..\n");
            close(sockfd);
        }
    }

    return 0;
    //google::protobuf::ShutdownProtobufLibrary();
}

