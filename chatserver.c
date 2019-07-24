# include <stdio.h>
# include <arpa/inet.h>
# include <string.h>
# include <sys/socket.h>
# include <stdlib.h>
# include <sys/epoll.h>
# include <errno.h>
# include <fcntl.h>
# include <stdbool.h>
# include <unistd.h>
# include "chatmessage.pb-c.h"

# define MAX_CLIENTS 100
# define BUFFER_SIZE 1000
# define PORT_NO 7000
# define MAXEVENTS 128


typedef enum message_type_e {
    LIVE_USER_BROADCAST = 1,
    P2P_MESSAGE         = 2,
} message_type_t;

typedef struct conninfo {
    char    username[20];
    struct  sockaddr_in address;

    struct  conninfo *next;
    struct  conninfo *prev;
}
Conninfo;

typedef struct message_header_s {
    uint32_t    len;
    uint32_t    type;

    uint32_t    sender_id;
    uint32_t    receiver_id;
} 
message_header_t;

typedef struct socket_cookie_s {
    uint32_t    fd;

    uint8_t     *pending_read_buffer;
    uint32_t     pending_read_buffer_len;

    uint8_t     *pending_write_buffer;
    uint32_t    pending_write_buffer_len;
}
socket_cookie_t;


Conninfo *head;

static Conninfo* create_conn_info() 
{
    head = (Conninfo *) malloc(sizeof(Conninfo));

    if(head != NULL) {
        head->next = NULL;
        head->prev = NULL;
    }

    return head;
}

static void add_conn_info(Conninfo** newconnect) 
{    
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

static void delete_conn_info(Conninfo **head, Conninfo **del) 
{
    if ((*head) == NULL || (*del) == NULL){
        return;  
    }

    if ((*head) == (*del)) {
        (*head) = (*del)->next;  
    }
    
    if ((*del)->next != NULL) {
        (*del)->next->prev = (*del)->prev;  
    }

    if ((*del)->prev != NULL) {
        (*del)->prev->next = (*del)->next;  
    }
    
    free((*del));
    return;   
} 

static bool isconnected(Conninfo **cur, 
        char *ip) 
{    
    while((*cur)->next != NULL) {

        if(!(strcmp(inet_ntoa((*cur)->address.sin_addr), ip))) {
            return true;
        }

        (*cur)=(*cur)->next;

    }

    return false;
}

static void setnonblocking(int sock)
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

static char* printList()  
{   
    Conninfo *connect = head;
    int len = 0;
    char *ip;
    while (connect != NULL)  
    {  
        len = len + strlen(inet_ntoa(connect->address.sin_addr)) + 1;
        connect = connect->next;
    }
    connect = head;
    ip = malloc(len);
    while(connect != NULL) {
        strcat(ip, inet_ntoa(connect->address.sin_addr));
        connect = connect -> next;
    }
    return ip;
}  


static void pack_message(ChatApp__Message* r_msg, const ChatApp__Message *msg) {
    uint8_t size,option;
    char szHostName[255];
    struct hostent *host_entry;

    gethostname(szHostName, 255);
    host_entry=gethostbyname(szHostName);
    char * szLocalIP;
    //szLocalIP = inet_ntoa (*((struct in_addr *)*host_entry->h_addr_list[0]));
    //printf("got hostbyname %s\n",szHostName);
    r_msg->hostname = "giri";

    if(msg->has_opt) {

        switch(msg->opt) {

            case CHAT_APP__MESSAGE__OPTION__LISTCLIENTS:
                r_msg->texttosend = printList();
                printf("IP LIST %s", r_msg->texttosend);
                r_msg->sourceip = "127.0.0.1";
                break;

            case CHAT_APP__MESSAGE__OPTION__CHAT:
                r_msg->texttosend = msg->texttosend;
                r_msg->sourceip = msg->destip;
                break;
        }
    }
    r_msg->messagelength = strlen(r_msg->texttosend);
    r_msg->has_opt = 1;
    r_msg->opt = CHAT_APP__MESSAGE__OPTION__RESPONSE;
}

static void handle_msg_write(socket_cookie_t    *cookie, 
        ChatApp__Message    msg , 
        int                 epollfd, 
        struct epoll_event  sock_event,
        int                 sockfd)
{
    int len;
    void *buf;
    len = chat_app__message__get_packed_size(&msg);
    buf = malloc(len);

    chat_app__message__pack(&msg,buf);  
    fprintf(stderr,"Writing %d serialized bytes to buffer\n",len); // See the length of message
    fwrite(buf,len,1,stdout);
    printf("\nMessage recorded as '%s'.\n", msg.texttosend);

    message_header_t message_header;
    uint32_t header_size = sizeof(message_header);
    /* adding message header */
    void      *send_buf;
    uint16_t  total_len = header_size + len;
    uint8_t   counter   = 0;
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
    cookie->pending_write_buffer = malloc(total_len);

    memcpy(cookie->pending_write_buffer, send_buf, total_len);

    cookie->pending_write_buffer_len = total_len;

    uint8_t sentbytes,w_counter = 0; 

    do{
        sock_event.events |= EPOLLOUT;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &sock_event);

        sentbytes = write(sockfd, cookie->pending_write_buffer, cookie->pending_write_buffer_len);

        if(sentbytes < 0){
            break;
        }

        if(sentbytes == cookie->pending_write_buffer_len){
            free(cookie->pending_write_buffer);
        } 
        else{
            cookie->pending_write_buffer_len   -= sentbytes;       

            memmove(cookie->pending_write_buffer,
                    cookie->pending_write_buffer + sentbytes,
                    cookie->pending_write_buffer_len);

            uint8_t *tmp;

            tmp = realloc(cookie->pending_write_buffer,
                    cookie->pending_write_buffer_len);

            if(tmp == NULL) {
                printf("Could not reallocate memory\n");
                epoll_ctl(epollfd, EPOLL_CTL_DEL, cookie->fd, NULL);
                free(cookie->pending_read_buffer);
                break;

            } 
            else{
                cookie->pending_write_buffer = tmp;
            }
        }

    } while(errno == EAGAIN);

}


static void PrintMessage(const ChatApp__Message* msg) {

    if (msg->destip != NULL) {
        printf("Destination IP: %s\n",msg->destip);
    }

    if (msg->texttosend != NULL) {
        printf("Text: %s\n",msg->texttosend);
    }

    printf("Message length: %d\n", (msg->messagelength-1));
    printf("option value: %d\n",msg->opt);

    switch (msg->opt) {

        case CHAT_APP__MESSAGE__OPTION__LISTCLIENTS:
            printf("LISTCLIENTS\n");
            break;

        case CHAT_APP__MESSAGE__OPTION__CHAT:
            printf("CHAT\n");
            break;

        case CHAT_APP__MESSAGE__OPTION__RESPONSE:
            printf("RESPONSE\n");
            break;

        default:
            printf("NO OPTION\n");
            break;
    }

}

static void unpack_header(uint8_t           *buffer,
        uint32_t           len,
        message_header_t  *header )
{
    uint32_t inc        = 0;
    uint32_t temp;
    header->len         = ntohl(*(uint32_t *)buffer);
    inc                += sizeof(header->len);

    printf("len %d \n", header->len);
    memcpy(&temp, buffer+inc, 4); 

    header->type        = ntohl(temp);
    inc                += sizeof(header->type);   

    printf("type %d \n", header->type);
    memcpy(&temp, buffer+inc, 4); 

    header->sender_id   = ntohl(temp);
    inc                += sizeof(header->sender_id);   

    printf("send %d \n", header->sender_id);
    memcpy(&temp, buffer+inc, 4); 

    header->receiver_id = ntohl(temp);
    inc                += sizeof(header->receiver_id);   

    printf("recv %d \n", header->receiver_id);
}

static uint8_t handle_message(socket_cookie_t    *cookie,
                              uint8_t            *buffer,
                              uint32_t            buffer_len,
                              int                 epollfd,
                              struct epoll_event  event,
                              int                 sockfd) 
{
    message_header_t header;
    unpack_header(buffer, sizeof(message_header_t), &header);
    ChatApp__Message *r_msg;
    ChatApp__Message  s_msg = CHAT_APP__MESSAGE__INIT;
    printf("buffer len %d \n", buffer_len);
    if((r_msg = chat_app__message__unpack(NULL, header.len, buffer + sizeof(header))) == NULL) {
        fprintf(stderr, "error unpacking message\n");           
        return -1;    
    }

    PrintMessage(r_msg);
    pack_message(&s_msg, r_msg);
    handle_msg_write(cookie, s_msg, epollfd, event, sockfd); 

    if(r_msg -> opt == CHAT_APP__MESSAGE__OPTION__LISTCLIENTS) {
        chat_app__message__free_unpacked(r_msg, NULL);
        return 1;
    }

    if(r_msg -> opt == CHAT_APP__MESSAGE__OPTION__CHAT) {
        chat_app__message__free_unpacked(r_msg, NULL);
        return 2;
    }

    return 0;
}

static int unpack_message(socket_cookie_t *cookie, int epollfd, struct epoll_event event, int sockfd) {

    message_header_t    header_obj;
    uint32_t            n_msg;
    uint8_t             retval;
    n_msg             = 0;
    do{
        if(cookie->pending_read_buffer_len < sizeof(header_obj.len)) {
            printf("unpack message header error\n");
            return 0;
        }

        header_obj.len = ntohl(*(uint32_t *)cookie->pending_read_buffer);

        if(cookie->pending_read_buffer_len < header_obj.len + sizeof(header_obj)) {
            return n_msg;      
        }

        retval = handle_message(cookie, cookie->pending_read_buffer, header_obj.len, epollfd, event, sockfd);

        n_msg   += 1;

        if(cookie->pending_read_buffer_len == (header_obj.len + sizeof(header_obj))) {
            free(cookie->pending_read_buffer);        
            cookie->pending_read_buffer_len = 0;    
        }
        
        else {
            memmove(cookie->pending_read_buffer,
                    cookie->pending_read_buffer + header_obj.len + sizeof(header_obj.len),
                    cookie->pending_read_buffer_len - header_obj.len - sizeof(header_obj));

            uint8_t *temp_buf;

            temp_buf    = realloc(cookie->pending_read_buffer,
                    cookie->pending_read_buffer_len - header_obj.len - sizeof(header_obj));

            if(temp_buf == NULL) {
                printf("realloc fail at processing new msg\n");
                epoll_ctl(epollfd, EPOLL_CTL_DEL, cookie->fd, NULL);
                free(cookie->pending_read_buffer);
                break;
            }
            else {
                cookie->pending_read_buffer  = temp_buf;
            }

            cookie->pending_read_buffer_len -= header_obj.len + sizeof(header_obj); 
        }

    } while(cookie->pending_read_buffer > 0);

    return n_msg;
}

static int initserver(Conninfo * server) 
{

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

    head = NULL;
    struct epoll_event event,events[MAX_CLIENTS];

    puts("Starting server...");
    Conninfo server;
    Conninfo *clients;
    char *recvbuf;
    int epollfd = epoll_create(0xbeef);
    int sockfd  = initserver( &server);

    setnonblocking(sockfd);

    char *cliaddr; 
    socket_cookie_t socket_c;
    socket_c.fd     = sockfd;
    event.data.ptr  = &socket_c;
    event.events    = EPOLLIN | EPOLLET;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event) < 0) {
        perror("Epoll error");
        exit(EXIT_SUCCESS);
    }


    uint32_t fd;
    socket_cookie_t sock_cookie[MAX_CLIENTS];

    for (;;) {

        int nfds, newsockfd,offset ;
        nfds = epoll_wait(epollfd, events, MAXEVENTS, -1);
        printf("ready fds %d\n", nfds);
        
        for(int i =0; i < nfds;i++) {

            fd = ((socket_cookie_t *)events[i].data.ptr)->fd;      
            int acceptfd;

            if (fd == sockfd) {

                add_conn_info(&clients);

                socklen_t addrlen = (socklen_t) sizeof(const struct sockaddr_in);
                acceptfd          = accept(sockfd, 
                        (struct sockaddr * ) & clients->address, &addrlen);    

                if(acceptfd < 0) {
                    perror("Accept failed\n");
                    continue;
                }

                cliaddr = inet_ntoa(clients->address.sin_addr);

                /*    if(isconnected(&clients, cliaddr)) {
                      delete_conn_info(&clients);
                      perror("Already connected");
                      close(acceptfd);
                      continue;
                      } */
                printf("Accepted %s\n", inet_ntoa(clients->address.sin_addr));
                setnonblocking(acceptfd);

                sock_cookie[acceptfd].fd                 = acceptfd;
                event.data.ptr                           = sock_cookie + acceptfd ;
                event.events                             = EPOLLIN;

                epoll_ctl(epollfd, EPOLL_CTL_ADD, sock_cookie[acceptfd].fd, &event);

                if (acceptfd < 0) {
                    if (errno != EAGAIN && 
                            errno != ECONNABORTED && 
                            errno != EPROTO && errno != EINTR) {
                        perror("accept");
                    }
                }
            }

            else if(events[i].events & EPOLLIN) {

                int total_len = sizeof(message_header_t);

                ((socket_cookie_t *) events[i].data.ptr)->pending_read_buffer     = (uint8_t *)malloc(BUFFER_SIZE);
                ((socket_cookie_t *) events[i].data.ptr)->pending_read_buffer_len = 0;

			    do {
			        offset = read(((socket_cookie_t *)events[i].data.ptr)  -> fd,
			                      ((socket_cookie_t *)events[i].data.ptr)        -> pending_read_buffer, 
			                        BUFFER_SIZE);
                    printf("Length is %d \n", offset);


			        if(offset <= 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
			            break;    
			        }

			        if(errno == ECONNRESET || errno == EPOLLERR || errno == EPOLLHUP) {
					    printf("Disconnected from %s\n", inet_ntoa(clients->address.sin_addr));
					    delete_conn_info(&head, &clients);
					    break;
				    }

			        if(offset == 0) {
				        printf("Disconnected from %s\n", inet_ntoa(clients->address.sin_addr));
		                delete_conn_info(&head, &clients);
				        close(((socket_cookie_t *)events[i].data.ptr)->fd);
				        break;
			        } 

                    printf("Length is %d \n", offset);
                    ((socket_cookie_t *)events[i].data.ptr)->pending_read_buffer_len     += offset;

			        uint8_t *tmp =  (uint8_t *)realloc(((socket_cookie_t *)events[i].data.ptr)->pending_read_buffer, 
			                ((socket_cookie_t *)events[i].data.ptr)->pending_read_buffer_len);

                    if(tmp == NULL) {
                        printf("realloc fail \n");
                        break;
                    }
                    else {
                        ((socket_cookie_t *) events[i].data.ptr)->pending_read_buffer  =  tmp;
                    }

			        uint32_t        retmsgval;
			        socket_cookie_t *cookie = ((socket_cookie_t *)events[i].data.ptr);
			        printf("Length of pending read buffer len is %d \n", cookie->pending_read_buffer_len);
                    retmsgval               = unpack_message(cookie, epollfd, events[i], sockfd);

                    ((socket_cookie_t *) events[i].data.ptr)->pending_read_buffer +=  offset;

			    } while(((socket_cookie_t *) events[i].data.ptr)->pending_read_buffer_len > 0);

            }

        }
    }   
}
