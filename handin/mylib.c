#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include "../include/dirtree.h"
#include <dirent.h>

int sockfd;



void init_client(char *msg, int len, char *buf) {

    int rv, sv;
    int count = 0;

    // send message to server
    while(count < len){ 
        sv = send(sockfd, msg + count, len - count, 0);
        if(sv < 0){
            break;
        }
        count += sv;
    }
        
    // receive message from server
    char recsize[sizeof(int)];
    recv(sockfd, &recsize, sizeof(int), 0);     
    int retsize;
    memcpy(&retsize, recsize, sizeof(int));
    if(retsize > 0){
        char trans[retsize];
        count = 0;
        while (count < retsize){ 
            rv = recv(sockfd, trans, retsize, 0);
            memcpy(buf + count, &trans, rv);
            count += rv;
        }
        if (rv<0) err(1,0);  
    }
}


// The following lines declare function pointers  
int (*orig_open)(const char *pathname, int flags, ...);  
int (*orig_close)(int fd);
ssize_t (*orig_read)(int fd, void *buf, size_t count);
ssize_t (*orig_write)(int fd, const void *buf, size_t count);
off_t (*orig_lseek)(int fd, off_t offset, int whence);
int (*orig_stat)(const char *path, struct stat *buf);
int (*orig_xstat)(int version, const char *path, struct stat *buf);
int (*orig_unlink)(const char *pathname);
ssize_t (*orig_getdirentries)(int fd, char *buf, size_t nbytes , off_t *basep);
struct dirtreenode* (*orig_getdirtree)(const char *path);
void (*orig_freedirtree)(struct dirtreenode* dt);
struct dirtreenode* DFS(int sessfd);

// This is our replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
    mode_t m = 0;
    if (flags & O_CREAT) {
        va_list a;
        va_start(a, flags);
        m = va_arg(a, mode_t);
        va_end(a);
    }

    int size = (int)strlen(pathname);
	// message total length = message size + message content
    int len = sizeof(int) + 3 * sizeof(int) + sizeof(mode_t) + size;
	// message sent to server
    char msg[len];
	// message received from server
    char buffer[2 * sizeof(int)];
	// open function type is 1
    int function_id = 1;
    int offset = 0;
	// message content length
    int bufsize = 3 * sizeof(int) + sizeof(mode_t) + size;

	// write message content length
    memcpy(msg, &bufsize, sizeof(int));
    offset += sizeof(int);
	// write function type
    memcpy(msg + offset, &function_id, sizeof(int));
    offset += sizeof(int);
	// write flag info
    memcpy(msg + offset, &flags, sizeof(int));
    offset += sizeof(int);
	// write path name size
    memcpy(msg + offset, &size, sizeof(int));
    offset += sizeof(int);
	// write mode info
    memcpy(msg + offset, &m, sizeof(mode_t));
    offset += sizeof(mode_t);
	// write path name
    memcpy(msg + offset, pathname, size);
    offset += size;
    
	// initialize client sending and store the reply to buffer
    init_client(msg, len, buffer);

    int fd = *((int *) buffer);
    int error = *((int *) buffer + 1);

    if(fd == -1 || error != 0){
        errno = error;
        return -1;
    }
	// number control
    return fd + 10000;
}

// This is our replacement for the close function from libc.
int close(int fd) {
    if(fd <= 10000){
        return orig_close(fd);
    }else{
        fd -= 10000;
    }
	// message total length = message size + message content
    int len = sizeof(int) + 2 * sizeof(int);
	// message sent to server
    char msg[len];
	// message received from server
    char buffer[2 * sizeof(int)];
    // close function type is 2
    int function_id = 2;
    int offset = 0;
	// message content length
    int bufsize = 2 * sizeof(int);
	// write message length
    memcpy(msg, &bufsize, sizeof(int));
    offset += sizeof(int);
	// write function type
    memcpy(msg + offset, &function_id, sizeof(int));
    offset += sizeof(int);
	// write fd info
    memcpy(msg + offset, &fd, sizeof(int));
    offset += sizeof(int);

	// initialize client sending and store the reply to buffer
    init_client(msg, len, buffer);

    int err = *((int *) buffer);
    int error = *((int *) buffer + 1);
    if(err == -1){
        errno = error;
    }
    return err;
}

// This is our replacement for the read function from libc.
ssize_t read(int fd, void *buf, size_t count) {
    if(fd <= 10000){
        return orig_read(fd,buf,count);
    }else{
        fd -= 10000;
    }
    int len = sizeof(int) + 2 *sizeof(int) + sizeof(size_t);
	// message sent to server
    char msg[len];
	// message received from server
    char buffer[sizeof(int) + sizeof(ssize_t) + count];
	// read function type is 4
    int function_id = 4;
    int offset = 0;
	// message content length
    int bufsize = 2 * sizeof(int) + sizeof(size_t);
	// write message length info
    memcpy(msg, &bufsize, sizeof(int));
    offset += sizeof(int);
	// write function type info
    memcpy(msg + offset, &function_id, sizeof(int));
    offset += sizeof(int);
	// write fd info
    memcpy(msg + offset, &fd, sizeof(int));
    offset += sizeof(int);
	// write count info
    memcpy(msg + offset, &count, sizeof(size_t));
    offset += sizeof(size_t);

	// initialize client sending and store the reply to buffer
    init_client(msg, len, buffer);
    int error = *((int *) buffer);
    ssize_t num_bytes;
	// get bytes read info
    memcpy(&num_bytes, (char*)buffer + sizeof(int), sizeof(ssize_t));
    memcpy(buf, (char*)buffer + sizeof(int) + sizeof(ssize_t), num_bytes); 

    if(error != 0){
        errno = error;
        return -1;
    }
    return num_bytes;
}

// This is our replacement for the write function from libc.
ssize_t write(int fd, const void *buf, size_t count){
    if(fd <= 10000){
        return orig_write(fd,buf,count);
    }else{
        fd -= 10000;
    }
	// message total length = message size + message content
    int len = sizeof(int) + 2 * sizeof(int) + sizeof(size_t) + count;
	// mesaage sent to server
    char msg[len];
	// message received from server
    char buffer[sizeof(int) + sizeof(ssize_t)];
	// write function type is 3
    int function_id = 3;
    int offset = 0;
	// message content length
    int bufsize = 2 * sizeof(int) + sizeof(size_t) + count;
    memcpy(msg, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(msg + offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(msg + offset, &fd, sizeof(int));
    offset += sizeof(int);
    memcpy(msg + offset, &count, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(msg + offset, buf, count);
    offset += count;
	// initialize client sending and store the reply to buffer
    init_client(msg, len, buffer);
    int error = *((int *) buffer);
    ssize_t num_bytes;
	// get bytes written info
    memcpy(&num_bytes, (char*)buffer + sizeof(int), sizeof(ssize_t));
    
    if(num_bytes == -1){
        errno = error;
    }
    return num_bytes;
}

// This is our replacement for the lseek function from libc.
off_t lseek(int fd, off_t offset, int whence){
    if(fd <= 10000){
        return orig_lseek(fd, offset, whence);
    }else{
        fd -= 10000;
    }
    int len = sizeof(int) + 3 * sizeof(int) + sizeof(off_t);
	// message sent to server
    char msg[len];
	// message received from server
    char buffer[sizeof(int) + sizeof(off_t)];
	// lseek function type is 5
    int function_id = 5;
    int off = 0;
    int bufsize = 3 * sizeof(int) + sizeof(off_t);
    memcpy(msg, &bufsize, sizeof(int));
    off += sizeof(int);
    memcpy(msg + off, &function_id, sizeof(int));
    off += sizeof(int);
    memcpy(msg + off, &fd, sizeof(int));
    off += sizeof(int);
    memcpy(msg + off, &whence, sizeof(int));
    off += sizeof(int);
    memcpy(msg + off, &offset, sizeof(off_t));
    off += sizeof(off_t);
	// initialize client sending and store the reply to buffer
    init_client(msg, len, buffer);
    
    int error = *((int *) buffer);
    off_t result;
    memcpy(&result, (char*)buffer + sizeof(int), sizeof(off_t));
    
    if(result == -1){
        errno = error;
    }
    return result;
}

// This is our replacement for the stat function from libc.
int stat(const char *path, struct stat *buf){
    int pathsize = (int)strlen(path);
    int len = sizeof(int) + 2 * sizeof(int) + pathsize;
    char msg[len];
    char buffer[2 * sizeof(int) + sizeof(struct stat)];
	// stat function type is 6
    int function_id = 6;
    int offset = 0;
    int bufsize = 2 * sizeof(int) + pathsize;
    memcpy(msg, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(msg + offset, &function_id, sizeof(int));
    offset += sizeof(int);
	// write path name size
    memcpy(msg + offset, &pathsize, sizeof(int));
    offset += sizeof(int);
	// write path name
    memcpy(msg + offset, path, pathsize);
    offset += pathsize;
	// initialize client sending and store the reply to buffer
    init_client(msg, len, buffer);
     
    int error = *((int *) buffer);
    int status = *((int *) buffer + 1);
    memcpy(buf, (char*)buffer + 2 * sizeof(int), sizeof(struct stat)); 
    if(status == -1){
        errno = error;
    }
    return status;
}

// This is our replacement for the __xstat function from libc.
int __xstat(int version, const char *path, struct stat *buf){
    int pathsize = (int)strlen(path);
    int len = sizeof(int) + 3 * sizeof(int) + pathsize;
	// message sent to server
    char msg[len];
	// message received from server
    char buffer[2 * sizeof(int) + sizeof(struct stat)];
	// __xstat function type is 7
    int function_id = 7;
    int offset = 0;
    int bufsize = 3 * sizeof(int) + pathsize;
    memcpy(msg, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(msg + offset, &function_id, sizeof(int));
    offset += sizeof(int);
	// write version info
    memcpy(msg + offset, &version, sizeof(int));
    offset += sizeof(int);
	// write path name size
    memcpy(msg + offset, &pathsize, sizeof(int));
    offset += sizeof(int);
	// write path name
    memcpy(msg + offset, path, pathsize);
    offset += pathsize;
	// initialize client sending and store the reply to buffer
    init_client(msg, len, buffer);
     
    int error = *((int *) buffer);
    int status = *((int *) buffer + 1);
    memcpy(buf, (char*)buffer + 2*sizeof(int), sizeof(struct stat)); 
    if(status == -1){
        errno = error;
    }
    return status;
}

// This is our replacement for the unlink function from libc.
int unlink(const char *pathname){
    int pathsize = (int)strlen(pathname);
    int len = sizeof(int) + 2 * sizeof(int) + pathsize;
    char msg[len];
    char buffer[2 * sizeof(int)];
	// unlink function type is 8
    int function_id = 8;
    int offset = 0;
    int bufsize = 2 * sizeof(int) + pathsize;
    memcpy(msg, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(msg + offset, &function_id, sizeof(int));
    offset += sizeof(int);
	// write path name size
    memcpy(msg + offset, &pathsize, sizeof(int));
    offset += sizeof(int);
	// write path name
    memcpy(msg + offset, pathname, pathsize);
    offset += pathsize;
	// initialize client sending and store the reply to buffer
    init_client(msg, len, buffer);
    
    int error = *((int *) buffer);
    int status = *((int *) buffer + 1);

    if(status == -1){
        errno = error;
    }
    return status;
}

// This is our replacement for the getdirentries function
ssize_t getdirentries(int fd, char *buf, size_t nbytes, off_t *basep){
    if(fd <= 10000){
        return orig_getdirentries(fd, buf, nbytes, basep);
    }else{
        fd -= 10000;
    }
    int len = sizeof(int) + 2 * sizeof(int) + sizeof(size_t) + sizeof(off_t);
    char msg[len];
    char buffer[sizeof(int) + sizeof(ssize_t) + nbytes];
	// getdirentries function type is 9
    int function_id = 9;
    int offset = 0;
    int bufsize = 2 * sizeof(int) + sizeof(size_t) + sizeof(off_t);
    memcpy(msg, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(msg + offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(msg + offset, &fd, sizeof(int));
    offset += sizeof(int);
	// write maximum bytes to read
    memcpy(msg + offset, &nbytes, sizeof(size_t));
    offset += sizeof(size_t);
	// write base pointer info
    memcpy(msg + offset, basep, sizeof(off_t));
    offset += sizeof(off_t);
	// initialize client sending and store the reply to buffer
    init_client(msg, len, buffer);
    int error = *((int *) buffer);
    ssize_t num_bytes;
    memcpy(&num_bytes, (char*)buffer + sizeof(int), sizeof(ssize_t));
    memcpy(buf, (char*)buffer + sizeof(int) + sizeof(ssize_t), num_bytes); 
    // update basep with the new position after reading
	basep += num_bytes;
    if(error != 0){
        errno = error;
        return -1;
    }
    return num_bytes;
}

// This is our replacement for the getdirtree function
struct dirtreenode* getdirtree(const char *path){
    int pathsize = (int)strlen(path);
    int len = sizeof(int) + 2 * sizeof(int) + pathsize;
    char msg[len];
	// getdirtree function type is 10
    int function_id = 10;
    int offset = 0;
    int bufsize = 2 * sizeof(int) + pathsize;
    memcpy(msg, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(msg + offset, &function_id, sizeof(int));
    offset += sizeof(int);
	// write path name size
    memcpy(msg + offset, &pathsize, sizeof(int));
    offset += sizeof(int);
	// write path name
    memcpy(msg + offset, path, pathsize);
    offset += pathsize;
    send(sockfd, msg, len, 0);  
    int error;
    recv(sockfd, &error, sizeof(int), 0);
    if (error == -1){
        errno = error;
        orig_close(sockfd);
        return NULL;
    }
	// apply DFS to transverse the directory
    struct dirtreenode* root = DFS(sockfd);
   
    errno = error;
    return root;
}

// This is our replacement for the freedirtree function
void freedirtree(struct dirtreenode* dt){
    orig_freedirtree(dt);
}

// This function is automatically called when program is started
void _init(void) {
    // set function pointers to point to the original function
    orig_open = dlsym(RTLD_NEXT, "open");
    orig_close = dlsym(RTLD_NEXT, "close");
    orig_read = dlsym(RTLD_NEXT, "read");
    orig_write = dlsym(RTLD_NEXT, "write");
    orig_lseek = dlsym(RTLD_NEXT, "lseek");
    orig_stat = dlsym(RTLD_NEXT, "stat");
    orig_unlink = dlsym(RTLD_NEXT, "unlink");
    orig_getdirentries = dlsym(RTLD_NEXT, "getdirentries");
    orig_getdirtree = dlsym(RTLD_NEXT, "getdirtree");
    orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");
    orig_xstat = dlsym(RTLD_NEXT, "__xstat");

    char *serverip;
    char *serverport;
    int rv;
    unsigned short port;
    struct sockaddr_in srv;
    
    // Get environment variable indicating the ip address of the server
    serverip = getenv("server15440");
    if (serverip){
    }
    else {
        serverip = "127.0.0.1";
    }
    
    // Get environment variable indicating the port of the server
    serverport = getenv("serverport15440");
    if (serverport){
    }
    else {
        serverport = "15440";
    }
    port = (unsigned short)atoi(serverport);
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);    // TCP/IP socket
    if (sockfd<0) err(1, 0);            // in case of error
    
    // setup address structure to point to server
    memset(&srv, 0, sizeof(srv));            // clear it first
    srv.sin_family = AF_INET;            // IP family
    srv.sin_addr.s_addr = inet_addr(serverip);    // IP address of server
    srv.sin_port = htons(port);            // server port

    // actually connect to the server
    rv = connect(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
    int flag = 1;
    int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 
            	(char *) &flag, sizeof(int));
    if (rv<0 || result<0) err(1,0);
}

void _fini(void){
    // close socket
    int len = 2 * sizeof(int);
    char msg[len];

    int function_id = 69;
    int offset = 0;
    int bufsize = sizeof(int);
    memcpy(msg, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(msg + offset, &function_id, sizeof(int));
    offset += sizeof(int);
    
    send(sockfd, msg, len, 0);
    orig_close(sockfd);
}

// DFS transverse the whole directory
struct dirtreenode* DFS(int sessfd){ 
    int namesize, num_subdirs;
	char* name = malloc(namesize); 
    recv(sessfd, &namesize, sizeof(int), 0);
    recv(sessfd, name, namesize, 0);
    recv(sessfd, &num_subdirs, sizeof(int), 0);
    struct dirtreenode* dt 
		= (struct dirtreenode*)malloc(sizeof(struct dirtreenode));
    dt->name = name;
    dt->num_subdirs = num_subdirs;

    if(num_subdirs > 0){// if sub directory is not empty
        dt->subdirs = malloc(sizeof(struct dirtreenode) * num_subdirs);
        int i;
		// continue DFS to transverse each sub directory
        for(i = 0; i < num_subdirs; i++){
            struct dirtreenode* child = DFS(sessfd);
            dt->subdirs[i] = child;
        }   
    }else{
        dt->subdirs = NULL;
    }
    return dt;
}