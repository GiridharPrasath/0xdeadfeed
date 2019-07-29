#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
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

#define PORT        7000
#define MAXBUF      1024
#define noproc      2
#define header_size 16

char    hostname[MAXBUF];
char    *IP_ADDR,*peerIp, *activeClients;
struct  hostent *host_entry;
struct  sockaddr_in addr,client;

typedef enum message_type_e {
    LIVE_USER_BROADCAST = 1,
    P2P_MESSAGE         = 2,
} message_type_t;

typedef struct message_header_s {
    uint32_t             len;
    uint32_t             type;
    uint32_t             sender_id;
    uint32_t             receiver_id;
} message_header_t;

typedef struct socket_cookie_s {
    uint32_t             fd;

    uint8_t             *pending_write_buffer;
    uint32_t             pending_write_buffer_len;

    uint8_t             *pending_read_buffer;
    uint32_t             pending_read_buffer_len;
} socket_cookie_t;


/* This function fills in a message based on user input. */ 
static void
PromptForMessage(ChatApp__Message*  msg,
                 int                opt)
{
    uint8_t size,option;
    char text[MAXBUF],ip[MAXBUF];
    printf("==================================\n");
    printf("HOST: %s\n",hostname);
    msg->hostname = hostname;

    printf("IP address: %s\n",IP_ADDR);
    msg->sourceip = IP_ADDR;

    size_t cur_len = 0;
    if(opt != 1){
        if(peerIp != '\0' && peerIp != NULL){
            msg->destip = peerIp; 
        }
        else{
            printf("Enter destination ip address or (blank to finish)\n");

            cur_len = read(0,ip,MAXBUF);
            printf("length %ld\n",cur_len);
            if (cur_len > 0){
                msg->destip = ip;
                peerIp = ip;
            }

        }
    }
    printf("Enter your message or (blank to finish)\n");
    // msg->n_texttosend = 1;
    // msg->texttosend = malloc (sizeof (int) * msg->n_texttosend);
    size_t nread = 0;
    nread = read(0,text,MAXBUF);
    // if (nread > 0){
    //     msg->texttosend[0] = text;
    // }
    if (nread > 0){
        msg->texttosend = text;
    }
    printf("text length %ld\n",nread);
    msg->messagelength = nread;
    if(opt != 0){
        msg->has_opt = 1;
        printf("option %d\n",opt);
    }
    switch (opt) {
        case 1:
            msg->opt = CHAT_APP__MESSAGE__OPTION__LISTCLIENTS;
            break;

        case 2:
            msg->opt = CHAT_APP__MESSAGE__OPTION__CHAT;
            break;

        default:
            msg->opt = CHAT_APP__MESSAGE__OPTION__LISTCLIENTS;
    }

    printf("----------------------------------\n");
}

static void
parseMessage(const ChatApp__Message* msg)
{

    printf("---------------___-------------------\n");
    printf("Hostname: %s (%s)\n", msg->hostname, msg->sourceip);
    //  if (!strcmp(msg->sourceip,peerIp)){
    //      peerIp = malloc(sizeof(char)*15);
    //      peerIp[0] = '\0';
    //      printf("Response from server\n");
    //      //set active clients
    //  }
    if (msg->destip != '\0' ) {
        printf("Destination IP: %s (to me.)\n",msg->destip);
    }

    //uint8_t i;
    //    for (i = 0; i < msg->n_texttosend; i++) {
    //        if (i > 0) {
    //            printf("\n");
    //        }
    //        printf("Data: %s",msg->texttosend[i]);
    //    }
    if (msg->texttosend[0] != '\0' && msg->texttosend != NULL) {
        printf("Data: %s", msg->texttosend);
    }
    printf("Message length: %d\n", msg->messagelength);

    switch (msg->opt) {
        case 0:
            printf("  LISTCLIENTS \n");
            break;

        case 1:
            printf( "  CHAT \n");
            break;

        case 2:
            printf("*** RESPONSE ***\n");
            break;
        default:
            printf("Option not set!\n");
    }
    printf("_______________---_______________\n");
}

static void
check_connection_reset (uint8_t fd, uint16_t epfd)
{
    if (errno == ECONNRESET) {
        printf("Server is down..\n");
        //        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);        
        exit(1);
    }
}

static void
unpack_header (uint8_t            *buffer,
               uint32_t            len,
               message_header_t   *header_out)
{
    assert(len == sizeof(message_header_t));
    uint32_t  ix = 0;

    header_out->len  = ntohl(*(uint32_t *)(buffer + ix));
    ix += sizeof(header_out->len);

    header_out->type = ntohl(*(uint32_t *)(buffer + ix));
    printf("type: %d\n", header_out->type);
    ix += sizeof(header_out->type);

    header_out->sender_id = ntohl(*(uint32_t *)(buffer + ix));
    printf("s_id: %d\n", header_out->sender_id);
    ix += sizeof(header_out->sender_id);

    header_out->receiver_id = ntohl(*(uint32_t *)(buffer + ix));
    printf("r_id: %d\n", header_out->receiver_id);
    ix += sizeof(header_out->receiver_id);

}

static int
handle_message (socket_cookie_t   *cookie,
                uint8_t           *buffer,
                uint32_t           buffer_len)
{
    message_header_t   header;

    unpack_header(buffer, sizeof(message_header_t), &header);

    /* handler rest of message body */
    ChatApp__Message *r_msg;
    printf("len %d\n", header.len); 
    printf("buff addr: %x\n",buffer);
    r_msg = NULL;

    r_msg = chat_app__message__unpack(NULL, header.len, (buffer + header_size));
    if (r_msg == NULL){
        fprintf(stderr, "error unpacking incoming message\n");
        return -1;
    }
    parseMessage(r_msg);
    printf("msg len: %d\n", header.len);

    chat_app__message__free_unpacked(r_msg, NULL);

    return 0;
}

static int
handle_message_on_read (socket_cookie_t *cookie, uint16_t epfd)
{
    assert(cookie);

    message_header_t    header_obj;
    uint32_t            n_processed;
    uint8_t             ret;
    n_processed         = 0;
    do {
        if (cookie->pending_read_buffer_len < sizeof(header_obj.len)) {
            printf("Not enough bytes to process message length from header!\n");
            return 0;
        }

        header_obj.len = ntohl(*(uint32_t *)cookie->pending_read_buffer);
        printf("Length of the message to unpack %d\n", header_obj.len);
        if (cookie->pending_read_buffer_len < header_obj.len + sizeof(header_obj)) {
            /* incomplete header or message */
            return n_processed;
        }

        ret = handle_message(cookie, cookie->pending_read_buffer, header_obj.len);
        if (ret == -1) {
            /* error : close socket */
            close(cookie->fd);
            return -1;
        }

        n_processed += header_size + header_obj.len; 
        /* message handled successfully */
        if (cookie->pending_read_buffer_len == (header_obj.len + sizeof(header_obj))) {
            free(cookie->pending_read_buffer);
            cookie->pending_read_buffer_len = 0;
        } else {
            memmove(cookie->pending_read_buffer,
                    cookie->pending_read_buffer + header_obj.len + sizeof(header_obj),
                    cookie->pending_read_buffer_len - header_obj.len - sizeof(header_obj));
            uint8_t *tmp;
            tmp = realloc(cookie->pending_read_buffer,
                    cookie->pending_read_buffer_len - header_obj.len - sizeof(header_obj));
            if(tmp == NULL){
                printf("Some bytes are left unprocessed!, Could not reallocate memory\n");
                //              close(cookie->fd);
                epoll_ctl(epfd, EPOLL_CTL_DEL, cookie->fd, NULL);
                free(cookie->pending_read_buffer);
                break;
            } else{
                cookie->pending_read_buffer = tmp;
            }

            cookie->pending_read_buffer_len -= header_obj.len + sizeof(header_obj);
        }

    } while(cookie->pending_read_buffer_len > 0);
    return n_processed;
}

int check_connection(uint8_t sockfd){
    
    if (errno == EINPROGRESS){
        printf("Connection in progress...\n");
    }
    uint8_t optval;
    socklen_t optval_len = sizeof(optval);
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void *)&optval, &optval_len);
    if (optval) {       /* optval is 0 if connection is successful*/
        printf("connection incomplete\n");
        if (errno == EALREADY) {
            printf("A previous connection attempt has not yet been completed.\n");
        }
            return -1;
    } 
    return 1;
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
    struct      epoll_event stdin_ev = {0};

    uint8_t     sockfd,connfd;
    uint8_t     id = getpid();
    uint16_t    epfd = epoll_create(10);
    uint8_t     opt;

    void                *buf;

    socket_cookie_t     stdin_cookie, socket_cookie;
    memset(&stdin_cookie, 0, sizeof(stdin_cookie));

    stdin_cookie.fd = 0;        /* fd for stdin */

    stdin_ev.data.ptr   = &stdin_cookie;
    stdin_ev.events     = EPOLLIN;
    epoll_ctl(epfd,EPOLL_CTL_ADD,((socket_cookie_t *)stdin_ev.data.ptr)->fd,&stdin_ev);

    sockfd  = socket(AF_INET,SOCK_STREAM,0);
    if (sockfd < 0) {
        printf("Socket creation error");
        exit(0);
    }
    printf("Socket created for id: %d \n",id);

    /*Setting to NON BLOCKING Socket*/
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    addr.sin_family = AF_INET;
    addr.sin_port   = htons(PORT);
    inet_pton(AF_INET, "10.145.0.245", &addr.sin_addr); 
    //inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr); 

    if ((connfd = connect(sockfd,(struct sockaddr*)&addr,sizeof(addr))) < 0) {
        printf("Connection error \n Client couldn't connect.\n");
        exit(1);
    }
    if(errno == EISCONN){
        printf("Already connected\n");
    }
    struct epoll_event sock_event ={0};
    socket_cookie.fd     = sockfd;
    sock_event.data.ptr  = &socket_cookie;
    sock_event.events    = EPOLLIN | EPOLLOUT;

    epoll_ctl(epfd,EPOLL_CTL_ADD,((socket_cookie_t *)sock_event.data.ptr)->fd,&sock_event); 
    message_header_t message_header;

    for (;;) {
        check_connection_reset(sockfd, epfd);
        printf("Enter your option \n 1.List all clients \n 2.Chat \n");
        uint8_t nready = epoll_wait(epfd,events,5,10000);
        
        for (uint8_t k = 0; k < nready; k++) {
            socket_cookie_t *cookie = (typeof(cookie))events[k].data.ptr;
            printf(" ready fd is: %d\n",cookie->fd);
            if (((socket_cookie_t *)events[k].data.ptr)->fd == 0) {
                /*  stdin is ready */
                int conn_status = check_connection(sockfd);
                if(!conn_status){
                    printf("conneciton pending...\n");
                    continue;
                }
                scanf("%hhd", &opt);
                
                ChatApp__Message  msg = CHAT_APP__MESSAGE__INIT;
                unsigned len;
                
                PromptForMessage(&msg, opt);
                len = chat_app__message__get_packed_size(&msg);
                buf = malloc(len);
                chat_app__message__pack(&msg,buf);

                fprintf(stderr,"Writing %d serialized bytes to buffer\n",len); // See the length of message
                fwrite(buf,len,1,stdout);
                printf("\nMessage recorded as '%s'.\n", msg.texttosend);

                /* adding message header */
                void        *send_buf;
                uint16_t    total_len = header_size + len;
                send_buf = malloc(total_len);

                memset(&message_header, 0, header_size);

                message_header.len          = len;
                message_header.type         = P2P_MESSAGE;
                message_header.sender_id    = 1;
                message_header.receiver_id  = 2;

                ((message_header_t *)send_buf)->len         = htonl(len);
                ((message_header_t *)send_buf)->type        = htonl(P2P_MESSAGE);
                ((message_header_t *)send_buf)->sender_id   = htonl(1);
                ((message_header_t *)send_buf)->receiver_id = htonl(2);

                memcpy(send_buf + header_size, buf, len);                  /* protobuf message */
                /* allocating memory of total length */    
                ((socket_cookie_t *)events[k].data.ptr)->pending_write_buffer = malloc(total_len);

                /* copying data to pending write buffer */
                memcpy(((socket_cookie_t *)events[k].data.ptr)->pending_write_buffer, send_buf, total_len);

                /* setting pending write buffer len to total len */
                ((socket_cookie_t *)events[k].data.ptr)->pending_write_buffer_len = total_len;

                uint8_t sentbytes; 
                int epoll_ret;
                do{
                    sock_event.events |= EPOLLOUT;
                    if((epoll_ret = epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &sock_event)) < 0){
                        printf("entered\n");
                        if(errno == ENOENT){
                            printf("Socket has closed!\n");
                            exit(1);
                        }
                    }

                    sentbytes = write(sockfd, cookie->pending_write_buffer, cookie->pending_write_buffer_len);
                    if(sentbytes < 0){
                        break;
                    }
                    if(sentbytes == cookie->pending_write_buffer_len){
                        free(cookie->pending_write_buffer);
                        cookie->pending_write_buffer_len = 0;
                    } else{
                        cookie->pending_write_buffer_len   -= sentbytes;       
                        memmove(cookie->pending_write_buffer,
                                cookie->pending_write_buffer + sentbytes,
                                cookie->pending_write_buffer_len);
                        uint8_t *tmp;
                        tmp = realloc(cookie->pending_write_buffer,
                                cookie->pending_write_buffer_len);
                        if(tmp == NULL){
                            printf("Could not reallocate memory\n");
                            epoll_ctl(epfd, EPOLL_CTL_DEL, cookie->fd, NULL);
                            free(cookie->pending_read_buffer);
                            break;
                        } else{
                            cookie->pending_write_buffer = tmp;
                        }
                    }

                } while(errno == EAGAIN);

                sock_event.events = EPOLLIN;
                if((epoll_ret = epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &sock_event)) < 0){
                    if(errno == ENOENT){
                        printf("Socket closed already!\n");
                        exit(1);
                    }
                }

                ((socket_cookie_t *)events[k].data.ptr)->pending_write_buffer            = 0;
                if (sentbytes < 0) {
                    printf("Nothing sent.\n");
                    //                close(sockfd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    exit(1);
                    /* Terminate socket */
                }
                printf("***%d bytes sent!***\n",sentbytes);
                free(buf); // Free the allocated serialized buffer
                //close(cookie->fd);
            } else {
                //socket is ready
                printf("Ready fd is socket fd\n");
                if (events[k].events & EPOLLHUP){
                    printf("SERVER SHUTDOWN \n");
                    exit(1);
                }
                if(events[k].events & EPOLLOUT){
                    int conn_status = check_connection(sockfd);
                    if (conn_status){
                        /* connected */
                        printf("CONNECTED TO SERVER\n");
                        sock_event.events = EPOLLIN;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd,&sock_event);
                    }else{
                        printf("Connection broke\n");
                        exit(1);
                    }
                }
                if (events[k].events & EPOLLIN) {
                    uint8_t          nbytes;
                    uint8_t          temp_buffer[MAXBUF];
                    uint8_t          n_processed;

                    printf("socket fd %u\n", cookie->fd);
                    cookie->pending_read_buffer_len = 0;
                    cookie->pending_read_buffer = NULL;
                    do {
                        nbytes = read(cookie->fd, temp_buffer, sizeof(temp_buffer));
                        if (nbytes > 0) {
                            uint8_t *tmp;
                             
                            tmp = realloc(cookie->pending_read_buffer,
                                          cookie->pending_read_buffer_len + nbytes);
                            if(tmp == NULL){
                                printf("Could not realloc!\n");
                            } else{
                                cookie->pending_read_buffer = tmp;
                            }
                            if (!cookie->pending_read_buffer) {
                                epoll_ctl(epfd, EPOLL_CTL_DEL, cookie->fd, NULL);
                                exit(1);
                            }
                            memcpy(cookie->pending_read_buffer + cookie->pending_read_buffer_len,
                                    temp_buffer,
                                    sizeof(temp_buffer));
                            cookie->pending_read_buffer_len += nbytes;
                            n_processed = handle_message_on_read(cookie, epfd);
                        } else if (nbytes <= 0) {
                            /* Socket close normal or error condition */
                            if (errno == EWOULDBLOCK){
                                printf("Socket is readable but nothing reieved");
                            }
                            printf("SERVER TERMINATED\n");
                            epoll_ctl(epfd, EPOLL_CTL_DEL, cookie->fd, NULL);
                            exit(1);
                        }
                    } while(nbytes > 0);
                    printf("Processed %d bytes", n_processed);
                }
                else if (events[k].events == 0) {
                    printf("Socket closing\n");
                    epoll_ctl(epfd, EPOLL_CTL_DEL, cookie->fd, NULL);
                    exit(1);
                }

            }       
        }
        if (errno == ECONNRESET) {
            printf("Server terminated..\n");
            //    close(sockfd);
            epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
            exit(1);
        }
    }

    return 0;
}

