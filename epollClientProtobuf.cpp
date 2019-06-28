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
#include <iostream>
#include <fstream>
#include "chatmessage.pb.h"

#define PORT    7000
#define MAXBUF  256
#define noproc  2

using namespace std;

char    hostname[MAXBUF];
char    *IP_ADDR;
struct  hostent *host_entry;
struct  sockaddr_in addr,client;

/* This function fills in a message based on user input. */
void PromptForMessage(chatApp::Message* msg,int opt) {
    uint8_t size,option;
    string  text,ip;

    cout << "HOST: " << hostname <<endl;
    msg->set_hostname(hostname);

    cout << "IP: " << IP_ADDR << endl;
    msg->set_sourceip(IP_ADDR);

    cout << "Enter your message or (blank to finish)" << endl;
    cin.ignore(256, '\n');
    getline(cin, text);
    if(!text.empty()){
        msg->set_texttosend(text);
    }

    size = text.length();
    cout << "Storing " << size << " bytes" <<endl;
    msg->set_messagelength(text.length());

    switch (opt) {
        case 1:
            msg->set_opt(chatApp::Message::LISTCLIENTS);
            break;

        case 2:
            msg->set_opt(chatApp::Message::CHAT);
            break;

        default:
            msg->set_opt(chatApp::Message::LISTCLIENTS);
    }
}

void PrintMessage(const chatApp::Message& msg) {

    cout << "reading...." << endl;

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

int main(int argc, char*argv[]){

    if (gethostname(hostname,MAXBUF) < 0) {
        cerr << "Could not get host name" << endl;
        return 0;
    }
    if((host_entry = gethostbyname(hostname)) == NULL){
        cerr << "Could not get host's IP address" << endl;
        return 0;
    }
    IP_ADDR = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); 

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    chatApp::Message    msg;

    std::ofstream   stream_buf;
    std::ifstream   recv_buf;
    std::string     send_buf;

    struct      epoll_event events[4];
    struct      epoll_event ev = {0};

    uint8_t     sockfd,connfd,i,max = 0;
    uint8_t     id = getpid();
    uint16_t    epfd = epoll_create(10);
    uint8_t     opt;
    
    uint_fast8_t        buffer[MAXBUF];

    ev.data.fd = 0;
    /* fd for stdin */
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
    //inet_pton(AF_INET, "10.145.0.107", &addr.sin_addr); 
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

    cout << "Enter your option" << endl << "1.List all clients" << endl << "2.Chat" << endl;

    for (;;) {

        uint8_t nready = epoll_wait(epfd,events,1,10000);

        for (uint8_t k = 0; k < nready; k++) {
            cout << events[k].data.fd << endl;
            if (events[k].data.fd == 0) {
                /* stdin is ready */
                cin >> opt;
                PromptForMessage(&msg, opt);
                cout << "Message recorded." << endl;
                if (!msg.SerializeToString(&send_buf)) {
                    cerr << "Failed to write address book." << endl;
                    return -1;
                }
                cout << "Parse success!" << endl;
                //stream_buf.read(send_buf,sizeof(msg));    
                int sentbytes = send(sockfd,(void *)&send_buf,send_buf.length(),0);
                cout << "size of message :" << sizeof(msg) << endl << " size of buffer"<< send_buf.length() << endl;
                //memset(&buffer,0,MAXBUF);
                if (sentbytes < 0) {
                    cout << "Nothing sent" << endl;
                }
                cout << sentbytes << " sent!" << endl;
            } else{
                //socket is ready
                if (events[k].events & EPOLLIN) {

                    memset(&buffer,0,MAXBUF);
                    uint16_t nbytes = read(events[k].data.fd, buffer, MAXBUF);
                    if (nbytes <= 0) {
                        close(events[k].data.fd);
                        cout << "Data not recieved yet... or server is down!" << endl;
                        continue;
                    } else {
                        cout << "recieved:" << buffer << endl;
                    }
                } else if (events[k].events == 0) {
                    cout << "Socket closing" << endl;
                    epoll_ctl(epfd,EPOLL_CTL_DEL,events[k].data.fd,&events[k]);
                }
            }
        }

        if (errno == ECONNRESET) {
            cout << "Server terminated.." << endl;
            close(sockfd);
        }
    }

    return 0;
    google::protobuf::ShutdownProtobufLibrary();
}

