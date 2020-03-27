#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define ERR_EXIT(a) { perror(a); exit(1); }

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
	int item;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
    int rwait;
} request;

typedef struct {
	int id;
	int amount;
	int price;
} Item;
			
int flag[20];
server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";

// Forwards

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

static int handle_read(request* reqP);
// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error
struct flock lck;

#include <time.h>
struct timeval wtime;

int lock(int a, int b, int c, int d){
	lck.l_type = a;
	lck.l_whence = b;
	lck.l_start = c;
	lck.l_len = d;
	return 0;
}

int cl(int a, int b){
	close(a);
	close(b);
	return 0;
}

int main(int argc, char** argv) {
    int i, ret;
    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512], buf2[512];
    int buf_len, sz = sizeof(Item);
	Item bidding;
	
    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }
    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    fd_set MAX;
    FD_ZERO(&MAX);
    FD_SET(svr.listen_fd, &MAX);
    maxfd = svr.listen_fd;		
    
	wtime.tv_sec = 20;
	wtime.tv_usec = 0;			

    while (1) {
        // TOO: Add IO multiplexing
    	fd_set TEMP;
    	FD_ZERO(&TEMP);
    	TEMP = MAX;

    	int time = select(maxfd + 1, &TEMP, NULL, NULL, &wtime);

		if (FD_ISSET(svr.listen_fd, &TEMP)) {
		    clilen = sizeof(cliaddr);
		    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
		    if (conn_fd < 0) {
		        if (errno == ENFILE) {
		            (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
		            continue;
		        if (errno == EINTR || errno == EAGAIN) continue;     
		        }
		        ERR_EXIT("accept")
		    }
		    requestP[conn_fd].conn_fd = conn_fd;
		    strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
		    fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
		    if(conn_fd >= maxfd)
				maxfd = conn_fd;
		    FD_SET(conn_fd, &MAX);
		    time--;
		}
		for (int i = svr.listen_fd+1; i <= maxfd && time > 0; i++) {
			if (FD_ISSET(i, &TEMP)) {
				ret = handle_read(&requestP[i]); // parse data from client to requestP[i].buf
				if (ret < 0) {
					fprintf(stderr, "bad request from %s\n", requestP[i].host);
					continue;
				}
	#ifdef READ_SERVER
			int x;
			int file_fd = open("item_list", O_RDONLY);
			lock(F_RDLCK, SEEK_CUR, 0, sz);
			if (fcntl(file_fd, F_SETLK, &lck) != -1) {
				sscanf(requestP[i].buf, "%d", &x);
				if (lseek(file_fd, (x-1)*sz, SEEK_CUR) != -1){
					read(file_fd, &bidding, sz);
					sprintf(buf,"item%d $%d remain:%d\n", bidding.id, bidding.price, bidding.amount);
				}
				else {
					sprintf(buf, "Error Occured.\n");
				}
				write(requestP[i].conn_fd, buf, strlen(buf));
			}
			else {
				if (errno == EACCES || errno == EAGAIN) {
		        	sprintf(buf2, "This item is locked.\n");
		            write(requestP[i].conn_fd, buf2, strlen(buf2));
		        } 
				else {
		            fprintf(stderr, "Error Occured.\n");
		        }
			}
			cl(file_fd, requestP[i].conn_fd);
			FD_CLR(i, &MAX);
			free_request(&requestP[i]);
	#else
			requestP[i].wait_for_write = open("item_list", O_RDWR);
			lock(F_WRLCK, SEEK_CUR, 0, sz);
			if (requestP[i].rwait == 0) {		
				sscanf(requestP[i].buf, "%d", &requestP[i].item);
				lseek(requestP[i].wait_for_write, (requestP[i].item-1)*sz, SEEK_SET);
				if (flag[requestP[i].item] != 1) {				
					if (fcntl(requestP[i].wait_for_write, F_SETLK, &lck) != -1) {
				        read(requestP[i].wait_for_write, &bidding, sz);
						sprintf(buf2, "This item is modifiable.\n");
						write(requestP[i].conn_fd, buf2, strlen(buf2));
						requestP[i].rwait = 1;
						flag[requestP[i].item] = 1;		
				    } 
					else { 
						if (errno == EACCES || errno == EAGAIN) {
				        	sprintf(buf2, "This item is locked.\n");
				            write(requestP[i].conn_fd, buf2, strlen(buf2));
				        } else {
				            fprintf(stderr, "Unexpected error.\n");
				        }
				        requestP[i].rwait = 2;
					}
				}
				else {
					sprintf(buf2, "This item is locked.\n");
			        write(requestP[i].conn_fd, buf2, strlen(buf2));
			        requestP[i].rwait = 2;
				}
			}
			else if(requestP[i].rwait == 1){
					char act[7];
					int num;
					sscanf(requestP[i].buf, "%s %d", &act, &num);
					lseek(requestP[i].wait_for_write, (requestP[i].item-1)*sz, SEEK_SET);
						if (strcmp(act, "price") == 0) {
							if (num < 0) { //price cant be 0 or negative
								sprintf(buf, "Operation failed.\n");
								write(requestP[i].conn_fd, buf, strlen(buf));
							}
							else {
								bidding.price = num;
								write(requestP[i].wait_for_write, &bidding, sz);
							}
							goto AFTERACT;
						}
						
						if (strcmp(act, "buy") == 0) {
							if (bidding.amount-num < 0 || num <= 0) { 
								sprintf(buf, "Operation failed.\n");
								write(requestP[i].conn_fd, buf, strlen(buf));
							}
							else {
								bidding.amount -= num;
								write(requestP[i].wait_for_write, &bidding, sz);
							}
							goto AFTERACT;
						}
						
						if (strcmp(act, "sell") == 0) {
							if (num <= 0) { 
								sprintf(buf, "Operation failed.\n");	
								write(requestP[i].conn_fd, buf, strlen(buf));
							}
							else {
								bidding.amount += num;
								write(requestP[i].wait_for_write, &bidding, sz);
							}
							goto AFTERACT;
						}
	
						if((strcmp(act, "price") != 0) || (strcmp(act, "sell") != 0) ||  (strcmp(act, "buy") == 0)){
							sprintf(buf, "Operation failed.\n");
							write(requestP[i].conn_fd, buf, strlen(buf));
						}
						
					AFTERACT:
					lseek(requestP[i].wait_for_write, (requestP[i].item-1)*sz, SEEK_SET);
					lock(F_UNLCK, SEEK_CUR, 0, sz);
			        if (fcntl(requestP[i].wait_for_write, F_SETLK, &lck) == -1)
			            fprintf(stderr, "Error Occured.\n");  
			        requestP[i].rwait = 2;
			        flag[requestP[i].item] = 0;
			        
				}
			if (requestP[i].rwait == 2) {
				flag[requestP[i].item] = 0;
			    cl(requestP[i].wait_for_write, requestP[i].conn_fd);
				FD_CLR(i, &MAX);
				free_request(&requestP[i]);
			}
			time--;
	#endif
			}
		}
    }
    free(requestP);
    return 0;
}


// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void* e_malloc(size_t size);


static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->item = 0;
    reqP->wait_for_write = 0;
    reqP->rwait;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error
static int handle_read(request* reqP) {
    int r;
    char buf[512];

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
	char* p1 = strstr(buf, "\015\012");
	int newline_len = 2;
	// be careful that in Windows, line ends with \015\012
	if (p1 == NULL) {
		p1 = strstr(buf, "\012");
		newline_len = 1;
		if (p1 == NULL) {
			ERR_EXIT("this really should not happen...");
		}
	}
	size_t len = p1 - buf + 1;
	memmove(reqP->buf, buf, len);
	reqP->buf[len - 1] = '\0';
	reqP->buf_len = len-1;
    return 1;
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }
}

static void* e_malloc(size_t size) {
    void* ptr;

    ptr = malloc(size);
    if (ptr == NULL) ERR_EXIT("out of memory");
    return ptr;
}

