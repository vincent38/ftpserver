/*
 * echoclient.c - An echo client
 */
#include <time.h>
#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd, port;
    char *host, buf[MAXLINE];
    rio_t rio;
    int fdin;
    size_t n;

    clock_t start, end;
    double cpu_time_used;
    ssize_t totalSize;
    
    if (argc != 2) {
        fprintf(stderr, "usage: %s <host>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = 2121;

    /*
     * Note that the 'host' can be a name or an IP address.
     * If necessary, Open_clientfd will perform the name resolution
     * to obtain the IP address.
     */
    clientfd = Open_clientfd(host, port);
    
    /*
     * At this stage, the connection is established between the client
     * and the server OS ... but it is possible that the server application
     * has not yet called "Accept" for this connection
     */
    printf("Connected to %s\n", host); 
    
    Rio_readinitb(&rio, clientfd);

    printf("ftp> ");
    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf));

        char okFileName[strlen(buf)+1];

        for (int i = 0; i < strlen(buf); i++){
            okFileName[i] = buf[i];
        }
        okFileName[strcspn(okFileName,"\r\n")] = 0;

        /*if ((n = Rio_readnb(&rio, buf, MAXLINE)) > 0) {
            char *r = strtok(buf, " ");
            char *array[2];
            int i = 0;
            while (r != NULL)
            {
                array[i++] = r;
                r = strtok (NULL, " ");
            }
            if (strcmp(array[0], "550") == 0) {
                printf("File does not exists on server.\n");
            }
        }*/


        // Get the filename
        fdin = Open(okFileName, O_WRONLY | O_APPEND | O_CREAT, 0644);
        //end of section
        char code[4];

        totalSize = 0;
        start = clock();
        while ((n = Rio_readnb(&rio, buf, MAXLINE)) > 0) {
            //Fputs(buf, stdout);
            strncpy(code, buf, 3);
            if (strcmp(code, "550") == 0){
                printf("File does not exists remotely ! Closing connection...\n");
                Close(clientfd);
                exit(0);
            } else {
                totalSize += rio_writen(fdin, buf, n);
            }
        }
        end = clock();
        Close(fdin);
        cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
        printf("Transfer succesfully complete.\n");
        ssize_t bandwidth = (totalSize/1000) / cpu_time_used;
        if (bandwidth > 0){
            printf("%ld bytes received in %f seconds (%ld Kbytes/s)\n", totalSize, cpu_time_used, bandwidth);
        } else {
            printf("%ld bytes received in %f seconds (inf Kbytes/s)\n", totalSize, cpu_time_used);
        }
        break;
    }
    Close(clientfd);
    exit(0);
}
