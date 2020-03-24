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
int myfd[NPROC][2];
int currP = 0;

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
    int listenfd, connfd;
    pid_t handler;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[MAX_NAME_LEN];
    int position;

    signal(SIGINT, crush);
    
    if (argc != 1) {
        fprintf(stderr, "usage: %s\n", argv[0]);
        exit(0);
    }
    //port = atoi(argv[1]);
    
    clientlen = (socklen_t)sizeof(clientaddr);

    listenfd = Open_listenfd(4266);
    for (int i = 0; i < NPROC; i++) {

        socketpair(PF_LOCAL, SCM_RIGHTS, 0, myfd[i]);
        
        if ((handler = Fork()) == 0){
            close(myfd[i][1]);

            // Create the handler
            printf("Started process %d\n", i);

            struct msghdr m;
            struct cmsghdr *cm;
            struct iovec iov;
            char dummy[100];
            char buf[CMSG_SPACE(sizeof(int))];
            //ssize_t readlen;
            int *fdlist;

            do {
                // Code indigeste de réception de socket
                // Librement inspiré et adapté du thread suivant :
                // https://stackoverflow.com/questions/14427898/how-can-i-pass-a-socket-from-parent-to-child-processes
                iov.iov_base = dummy;
                iov.iov_len = sizeof(dummy);
                memset(&m, 0, sizeof(m));
                m.msg_iov = &iov;
                m.msg_iovlen = 1;
                m.msg_controllen = CMSG_SPACE(sizeof(int));
                m.msg_control = buf;
                recvmsg(myfd[i][0], &m, 0);
                printf("Awaiting connection...\n");
                /* Do your error handling here in case recvmsg fails */
                int received_file_descriptor = -1; /* Default: none was received */
                for (cm = CMSG_FIRSTHDR(&m); cm; cm = CMSG_NXTHDR(&m, cm)) {
                    if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS) {
                        //int nfds = (cm->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                        fdlist = (int *)CMSG_DATA(cm);
                        received_file_descriptor = *fdlist;
                        break;
                    }
                }

                printf("%d got file descriptor %d\n", i, received_file_descriptor);

                ftpHandler(received_file_descriptor);

                //Close(received_file_descriptor);
            } while(1);

        } else {
            // Register it's id on a table
            close(myfd[i][0]);
            myProcess[i] = handler;
        }

    }

    // Start on first process
    position = 0;

    // Wait for a connection
    while(1) {
        printf("The next handler will be %d\n", position);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        /* determine the name of the client */
        Getnameinfo((SA *) &clientaddr, clientlen,
            client_hostname, MAX_NAME_LEN, 0, 0, 0);
                
        /* determine the textual representation of the client's IP address */
        Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string,
            INET_ADDRSTRLEN);
                
        printf("server connected to %s (%s) - looking for handler available\n", client_hostname,
            client_ip_string);

        // Code indigeste de transmission de socket
        // Librement inspiré et adapté du thread suivant :
        // https://stackoverflow.com/questions/14427898/how-can-i-pass-a-socket-from-parent-to-child-processes
        struct msghdr m;
        struct cmsghdr *cm;
        struct iovec iov;
        char buf[CMSG_SPACE(sizeof(int))];
        char dummy[2];    

        memset(&m, 0, sizeof(m));
        m.msg_controllen = CMSG_SPACE(sizeof(int));
        m.msg_control = &buf;
        memset(m.msg_control, 0, m.msg_controllen);
        cm = CMSG_FIRSTHDR(&m);
        cm->cmsg_level = SOL_SOCKET;
        cm->cmsg_type = SCM_RIGHTS;
        cm->cmsg_len = CMSG_LEN(sizeof(int));
        *((int *)CMSG_DATA(cm)) = connfd;
        m.msg_iov = &iov;
        m.msg_iovlen = 1;
        iov.iov_base = dummy;
        iov.iov_len = 1;
        dummy[0] = 0;   /* doesn't matter what data we send */
        sendmsg(myfd[position][1], &m, 0);

        Close(connfd);

        if (position >= NPROC-1) {
            position = 0;
        } else {
            position++;
        }
   
    }
    exit(0);

}

