#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>


#include "../include/dirtree.h"


// The following line declares a function pointer with the same prototype as the open function.  
int (*orig_open)(const char *pathname, int flags, ...);  // mode_t mode is needed when flags includes O_CREAT
int (*orig_close)(int fd);
ssize_t (*orig_read)(int fd, void *buf, size_t cnt);
ssize_t (*orig_write)(int fd, const void *buf, size_t cnt);
off_t (*orig_lseek)(int fildes, off_t offset, int whence);
int (*orig_stat)(const char *path, struct stat *buf);
int (*orig_unlink)(const char *path);
ssize_t (*orig_getdirentries)(int fd, char *buf, size_t nbytes , off_t *basep);
struct dirtreenode* (*orig_getdirtree)(const char *path);
void (*orig_freedirtree)(struct dirtreenode* dt);
int (*orig_xstat)(int ver, const char * path, struct stat * stat_buf);

int sockfd;

// This is our replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}
	// we just print a message, then call through to the original open function (from libc)
	//fprintf(stderr, "mylib: open called for path %s\n", pathname);
	printf("open\n");
	//send(sockfd, "open\n", strlen("open\n"), 0);
	return orig_open(pathname, flags, m);
}

int close(int fd){
	//fprintf(stderr, "mylib: close called for fd %d\n", fd);
	printf("close\n");
	//send(sockfd, "close\n", strlen("close\n"), 0);

	return orig_close(fd);
}

ssize_t read(int fd, void *buf, size_t cnt){
	//fprintf(stderr, "mylib: read called for fd %d\n", fd);
	printf("read\n");
	//send(sockfd, "read\n", strlen("read\n"), 0);
	return orig_read(fd, buf, cnt);
}

ssize_t write(int fd, const void *buf, size_t cnt){
	//fprintf(stderr, "mylib: write called for fd %d\n", fd);
	printf("write\n");
	//send(sockfd, "write\n", strlen("write\n"), 0);
	return orig_write(fd, buf, cnt);
}
off_t lseek(int fildes, off_t offset, int whence){
	//fprintf(stderr, "mylib: lseek called for fd %d\n", fildes);
	printf("lseek\n");
	//send(sockfd, "lseek\n", strlen("lseek\n"), 0);
	return orig_lseek(fildes, offset, whence);
}

int stat(const char *path, struct stat *buf){
	//fprintf(stderr, "mylib: stat called for path %s\n", path);
	printf("stat\n");
	//send(sockfd, "stat\n", strlen("stat\n"), 0);
	return orig_stat(path, buf);
}

int unlink(const char *path){
	//fprintf(stderr, "mylib: unlink called for path %s\n", path);
	printf("unlink\n");
	//send(sockfd, "unlink\n", strlen("unlink\n"), 0);
	return orig_unlink(path);
}

int __xstat(int ver, const char * path, struct stat * stat_buf){
	//fprintf(stderr, "mylib: __xstat called for path %s\n", path);
	printf("__xstat\n");
	//send(sockfd, "__xstat\n", strlen("__xstat\n"), 0);
	return orig_xstat(ver, path, stat_buf);
}


ssize_t getdirentries(int fd, char *buf, size_t nbytes , off_t *basep){
	//fprintf(stderr, "mylib: getdirentries called for fd %d\n", fd);
	printf("getdirentries\n");
	//send(sockfd, "getdirentries\n", strlen("getdirentries\n"), 0);
	return orig_getdirentries(fd, buf, nbytes, basep);
}

struct dirtreenode* getdirtree(const char *path){
	//fprintf(stderr, "mylib: getdirtree called for path %s\n", path);
	printf("getdirtree\n");
	//send(sockfd, "getdirtree\n", strlen("getdirtree\n"), 0);
	return orig_getdirtree(path);
}
void freedirtree(struct dirtreenode* dt){
	//fprintf(stderr, "mylib: freedirtree called for name %s\n", dt->name);
	printf("freedirtree\n");
	//send(sockfd, "freedirtree\n", strlen("freedirtree\n"), 0);
	return orig_freedirtree(dt);
}

void init_client(){
	char *serverip;
	char *serverport;
	unsigned short port;
	//char *msg="Hello from client";
	int rv;
	struct sockaddr_in srv;
	
	// Get environment variable indicating the ip address of the server
	serverip = "127.0.0.1";

	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) fprintf(stderr, "Got environment variable serverport15440: %s\n", serverport);
	else {
		fprintf(stderr, "Environment variable serverport15440 not found.  Using 15440\n");
		serverport = "15440";
	}
	port = (unsigned short)atoi(serverport);
	
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			// in case of error
	
	// setup address structure to point to server
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;			// IP family
	srv.sin_addr.s_addr = inet_addr(serverip);	// IP address of server
	srv.sin_port = htons(port);			// server port

	// actually connect to the server
	rv = connect(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);

}




// This function is automatically called when program is started
void _init(void) {
	// set function pointer orig_open to point to the original open function
	orig_open = dlsym(RTLD_NEXT, "open");
	orig_close = dlsym(RTLD_NEXT, "close");
	orig_read = dlsym(RTLD_NEXT, "read");
	orig_write = dlsym(RTLD_NEXT, "write");
	orig_lseek = dlsym(RTLD_NEXT, "lseek");
	//orig_stat = dlsym(RTLD_NEXT, "stat");
	orig_unlink = dlsym(RTLD_NEXT, "unlink");
	orig_xstat = dlsym(RTLD_NEXT, "__xstat");
	orig_getdirentries = dlsym(RTLD_NEXT, "getdirentries");
	orig_getdirtree = dlsym(RTLD_NEXT, "getdirtree");
	orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");

	//init_client();

	fprintf(stderr, "Init mylib\n");
}


