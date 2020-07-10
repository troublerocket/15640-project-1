#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "../include/dirtree.h"
#include <dirent.h>
#define MAXMSGLEN 100

void DFS(struct dirtreenode* dt, int sessfd);

int main(int argc, char**argv) {
    char *serverport;
    unsigned short port;
    int sockfd, sessfd, rv;
    struct sockaddr_in srv, cli;
    socklen_t sa_size;
    
    // Get environment variable indicating the port of the server
    serverport = getenv("serverport15440");
    if (serverport) port = (unsigned short)atoi(serverport);
    else port = 15440;
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);    // TCP/IP socket
    if (sockfd<0) err(1, 0);            // in case of error
    
    // setup address structure to indicate server port
    memset(&srv, 0, sizeof(srv));            // clear it first
    srv.sin_family = AF_INET;            // IP family
    srv.sin_addr.s_addr = htonl(INADDR_ANY);    // don't care IP address
    srv.sin_port = htons(port);            // server port

    // bind to our port
    rv = bind(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
    int flag = 1;
    int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 
                (char *) &flag, sizeof(int));
    if (rv<0 || result<0) err(1,0);
    
    // start listening for connections
    rv = listen(sockfd, 5);
    if (rv<0) err(1,0);
    
    // main server loop
    while(1){    
        // wait for next client, get session socket
        sa_size = sizeof(struct sockaddr_in);
        sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);
        if (sessfd<0) err(1,0);

        // fork a child process to handle concurrent clients
        rv = fork();
        if(rv!=0) continue;

        while(1) {
            char bufsize[sizeof(int)]; 

            while ((rv = recv(sessfd, bufsize, 4, 0)) > 0) {
                if(rv >= 4){
                    break;
                }
            }
            int size = *((int *)bufsize); 
            char* msg = malloc(size);
            char* buf = malloc(size);
            int count = 0;
            // get message from this client
            while ((rv=recv(sessfd, buf, size, 0)) > 0) {
                memcpy(msg + count, buf, rv);
                count += rv;
                if(count>=size){
                    break;
                }
            }
            free(buf);
            // get function type id from the client's message
            int type = *((int *) msg);
            switch (type) {
                case 1: // open function
                    {
                        // read flag info
                        int flags = *((int *) msg + 1);
                        // read path name size
                        int size = *((int *) msg + 2);
                        mode_t mode;
                        // read mode info
                        memcpy(&mode, (char*)msg + 3 * sizeof(int),
                                sizeof(mode_t));
                        char path[size + 1];
                        // read path name
                        memcpy(path,(char*)msg + 3*sizeof(int) + sizeof(mode_t),
                                size);
                        path[size] = '\0';

                        errno = 0;
                        int fd = open(path, flags, mode);
                        int error = errno;
                        char retsize[sizeof(int)];
                        size = 2 * sizeof(int);
                        memcpy(retsize, &size, sizeof(int));
                        send(sessfd, retsize, sizeof(int),0);
                        char answer[2 * sizeof(int)];
                        // write fd and errno info
                        memcpy(answer, &fd, sizeof(int));
                        memcpy(answer + sizeof(int), &error, sizeof(int));
                        // send the message to client
                        send(sessfd, answer, 2 * sizeof(int), 0);
                    } 
                    break;
                    
                case 2: // close function
                    {
                        // read info from the client's message
                        int fd = *((int *) msg + 1);
                        errno = 0;
                        int err = close(fd);
                        int error = errno;       
                        char retsize[sizeof(int)];
                        int size = 2 * sizeof(int);

                        memcpy(retsize, &size, sizeof(int));
                        send(sessfd, retsize, sizeof(int), 0);

                        char answer[2 * sizeof(int)];
                        // write errno info
                        memcpy(answer, &err, sizeof(int));
                        memcpy(answer + sizeof(int), &error, sizeof(int));
                        // send the message to client
                        send(sessfd, answer, 2 * sizeof(int), 0);
                    } 
                    break;
     
                case 3: // write function
                    {
                        // read info from the client's message
                        int fd = *((int *) msg + 1);
                        size_t count;
                        memcpy(&count, (char*)msg + 2 * sizeof(int),
                                sizeof(size_t));
                        
                        char* buffer = malloc(count);
                        memcpy(buffer, 
                        (char*)msg + 2 * sizeof(int) + sizeof(size_t), count);
                        
                        errno = 0;
                        ssize_t num_bytes = write(fd, buffer, count);
                        int error = errno;
                        char retsize[sizeof(int)];
                        int size = sizeof(int) + sizeof(ssize_t);
                        memcpy(retsize, &size, sizeof(int));
                        send(sessfd, retsize, sizeof(int), 0);
                        
                        char answer[sizeof(int) + sizeof(ssize_t)];
                        // write errno and bytes written info
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer + sizeof(int), &num_bytes, sizeof(ssize_t));
                        // send the message to client
                        send(sessfd, answer, sizeof(int) + sizeof(ssize_t), 0);
                        free(buffer);

                    }
                    break;

                case 4: // read function
                    {
                        // read info from the client's message
                        int fd = *((int *) msg + 1);
                        size_t count;
                        memcpy(&count,
                        (char*)msg + 2 * sizeof(int), sizeof(size_t));
                        char buffer[count];                      
                        errno=0;
                        ssize_t num_bytes = read(fd, buffer, count);
                        int error = errno;
                        
                        if(num_bytes == -1){
                            num_bytes = 0;
                        }
                        
                        char retsize[sizeof(int)];
                        int size = sizeof(int) + sizeof(ssize_t) + num_bytes;
                        memcpy(retsize, &size, sizeof(int));
                        send(sessfd, retsize, sizeof(int), 0);
                        
                        char answer[sizeof(int) + sizeof(ssize_t) + num_bytes];
                        // write errno and bytes info to message
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer + sizeof(int), 
                                &num_bytes, sizeof(ssize_t));
                        memcpy(answer + sizeof(int) + sizeof(ssize_t),
                                buffer, num_bytes);
                        // send the message to client
                        send(sessfd, answer, 
                            sizeof(int) + sizeof(ssize_t) + num_bytes, 0);
                       
                    } 
                    break;
                case 5: // lseek function
                    {
                        // read info from the client's message
                        int fd = *((int *) msg+1);
                        int whence = *((int *) msg+2);
                        off_t offset;
                        memcpy(&offset,
                            (char*)msg + 3 * sizeof(int), sizeof(off_t));
                        
                        errno = 0;
                        off_t result = lseek(fd, offset, whence);
                        int error = errno;

                        char retsize[sizeof(int)];
                        int size = sizeof(int) + sizeof(off_t);
                        memcpy(retsize, &size, sizeof(int));
                        send(sessfd, retsize, sizeof(int), 0);
                        char answer[sizeof(int)+sizeof(off_t)];
                        // write errno and result info to the message
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer + sizeof(int), &result, sizeof(off_t));
                        // send the message to client
                        send(sessfd, answer, sizeof(int) + sizeof(off_t), 0);
                    } 
                    break;
                case 6: // stat function
                    {
                        int pathsize = *((int *) msg + 1);
                        char path[pathsize + 1];
                        memcpy(path,(char*)msg + 2 * sizeof(int), pathsize);
                        path[pathsize] = '\0';

                        struct stat *buf = malloc(sizeof(struct stat));
                        errno = 0;
                        int result = stat(path, buf);
                        int error = errno;
                    
                        char retsize[sizeof(int)];
                        int size = 2 * sizeof(int) + sizeof(struct stat);
                        memcpy(retsize, &size, sizeof(int));
                        send(sessfd, retsize, sizeof(int), 0);

                        char answer[2 * sizeof(int) + sizeof(struct stat)];
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer + sizeof(int), &result, sizeof(int));
                        memcpy(answer + 2 * sizeof(int),
                                buf, sizeof(struct stat));
                        send(sessfd, answer,
                            2 * sizeof(int) + sizeof(struct stat), 0);
                        free(buf);
                    }
                    break;
                case 7: // __xstat function
                    {
                        // read the info from the client's message
                        int version = *((int *) msg + 1);
                        int pathsize = *((int *) msg + 2);
                        char path[pathsize + 1];
                        memcpy(path, (char*)msg + 3 * sizeof(int), pathsize);
                        path[pathsize] = '\0';
                        struct stat *buf = malloc(sizeof(struct stat));
                        
                        errno=0;
                        int result = __xstat(version, path, buf);
                        int error = errno;
                        char retsize[sizeof(int)];
                        int size = 2 * sizeof(int) + sizeof(struct stat);
                        memcpy(retsize, &size, sizeof(int));
                        send(sessfd, retsize, sizeof(int), 0);

                        char answer[2 * sizeof(int) + sizeof(struct stat)];
                        // write the errno and result info to the message
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer + sizeof(int), &result, sizeof(int));
                        memcpy(answer + 2 * sizeof(int),
                                buf, sizeof(struct stat));
                        // send the message to client
                        send(sessfd, answer,
                            2 * sizeof(int) + sizeof(struct stat), 0);
                        free(buf);
                    }
                    break;
                case 8: // unlink function
                    {
                        // read the info from the client's message
                        int pathsize = *((int *) msg + 1);
                        char path[pathsize + 1];
                        memcpy(path,(char*)msg + 2 * sizeof(int), pathsize);
                        path[pathsize] = '\0';
                        errno = 0;
                        int result = unlink(path);
                        int error = errno;
                        char retsize[sizeof(int)];
                        int size = 2 * sizeof(int);
                        memcpy(retsize, &size, sizeof(int));
                        send(sessfd, retsize, sizeof(int), 0);
                        char answer[2 * sizeof(int)];
                        // write errno and result info to the message
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer + sizeof(int), &result, sizeof(int));
                        // send the message to client
                        send(sessfd, answer, 2 * sizeof(int), 0);
                    } 
                    break;
                case 9: // getdirentries function
                    {
                        // read the info from the client's message
                        int fd = *((int *) msg + 1);
                        size_t nbytes;
                        memcpy(&nbytes,
                                (char*)msg + 2*sizeof(int), sizeof(size_t));
                        off_t basep;
                        memcpy(&basep,
                            (char*)msg + 2 * sizeof(int) + sizeof(size_t), 
                            sizeof(off_t));
                        
                        char buf[nbytes];
                        errno = 0;
                        ssize_t num_bytes 
                                    = getdirentries(fd, buf, nbytes, &basep);
                        int error = errno;
                        
                        if(num_bytes == -1){
                            num_bytes = 0;
                        }                         
                        char retsize[sizeof(int)];
                        int size = sizeof(int) + sizeof(ssize_t) + num_bytes;
                        memcpy(retsize, &size, sizeof(int));
                        send(sessfd, retsize, sizeof(int), 0);

                        char answer[sizeof(int) + sizeof(ssize_t) + num_bytes];
                        // write errno and result info to the message
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer + sizeof(int),
                                &num_bytes, sizeof(ssize_t));
                        memcpy(answer + sizeof(int) + sizeof(ssize_t),
                                buf, num_bytes);
                        // send the message to client
                        send(sessfd, answer, 
                            sizeof(int) + sizeof(ssize_t) + num_bytes, 0);
                    } 
                    break;
                case 10: // getdirtree function
                    {
                        // read the info from the client's message
                        int pathsize = *((int *) msg + 1);
                        char path[pathsize + 1];
                        memcpy(path, (char*)msg + 2 * sizeof(int), pathsize);
                        path[pathsize] = '\0';
                        errno=0;
                        struct dirtreenode* tree = getdirtree(path);
                        int error = errno;
                        send(sessfd, &error, sizeof(int), 0);
                        // DFS transversal for the whole directory
                        DFS(tree, sessfd);
                        freedirtree(tree);
                    } 
                    break;
                default:
                    {
                      close(sessfd);
                      exit(0);
                    }
            }
            free(msg); 
            // either client closed connection, or error
            if (rv<0) err(1,0);
        }
    }
    // close socket
    close(sockfd);
    return 0;
}

// DFS transversal for the whole directory
void DFS(struct dirtreenode* dt, int sessfd){
    int namesize = (int)strlen(dt->name) + 1;
    send(sessfd, &namesize, sizeof(int), 0);
    send(sessfd, dt->name, namesize, 0);
    // get the number of sub directories
    int num = dt->num_subdirs;
    send(sessfd, &num, sizeof(int), 0);
    int i;
    for(i = 0; i < dt->num_subdirs; i++){
        // DFS transverse each sub directory
        DFS(dt->subdirs[i], sessfd);
    }
}