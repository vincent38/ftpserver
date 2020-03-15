/*
 * echoserveri.c - An iterative echo server
 */

#include "csapp.h"

#define NPROC 2

#define MAX_NAME_LEN 256

void ftpHandler(int connfd);

/* 
 * Note that this code only works with IPv4 addresses
 * (IPv6 is not supported)
 */

pid_t myProcess[NPROC];

void crush(int sig){
	int status;
	pid_t pid;

    for (int i = 0; i < NPROC; i++) {
        kill(myProcess[i], SIGINT);
    }

	while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0){
        if (status) {
            printf("An error occured while closing process %d\n", pid);
        } else {
            printf("Client disconnected.");
        }
    }
    exit(0);
}

int main(int argc, char **argv)
{
    int listenfd, connfd, port;
    pid_t handler;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[MAX_NAME_LEN];

    signal(SIGINT, crush);
    
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    port = atoi(argv[1]);
    
    clientlen = (socklen_t)sizeof(clientaddr);

    listenfd = Open_listenfd(2121);
    for (int i = 0; i < NPROC; i++) {
        
        if ((handler = Fork()) == 0){

            printf("Started process %d\n", i);

            while(1) {
                connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

                /* determine the name of the client */
                Getnameinfo((SA *) &clientaddr, clientlen,
                            client_hostname, MAX_NAME_LEN, 0, 0, 0);
                
                /* determine the textual representation of the client's IP address */
                Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string,
                        INET_ADDRSTRLEN);
                
                printf("server connected to %s (%s)\n", client_hostname,
                    client_ip_string);

                ftpHandler(connfd);

                Close(connfd);
            }
            exit(0);
        } else {
            myProcess[i] = handler;
        }
    }

    while(1){}
}

