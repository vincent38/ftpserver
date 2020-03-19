/*
 * echo - read and echo text lines until client closes connection
 */
#include "csapp.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

void ftpHandler(int connfd)
{
    /*size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("server received %u bytes\n", (unsigned int)n);
        printf("data : %s\n", buf);
        Rio_writen(connfd, buf, n);
    }*/

    size_t n;
    rio_t bufferRio;
    int fdin;
    size_t CHUNK_SIZE = 4092;
    char buf[CHUNK_SIZE];
    char fileName[MAXLINE];
    rio_t rio;
    struct stat st;

        Rio_readinitb(&rio, connfd);

        if ((n = Rio_readlineb(&rio, fileName, MAXLINE)) != 0){
            printf("server received %u bytes\n", (unsigned int)n);

            char okFileName[n+1];

            for (int i = 0; i < n; i++){
                okFileName[i] = fileName[i];
            }
            okFileName[strcspn(okFileName,"\r\n")] = 0;

            printf("asking for file : %s\n", okFileName);

            // Ouverture du fichier
            fdin = open(okFileName, O_RDONLY, 0);

            if (fdin == -1) {
                printf("file does not exists !!!\n");
                Rio_writen(connfd, "550 DOES_NOT_EXISTS\n", 20);
            } else {
                stat(okFileName, &st);
                long int chunks = st.st_size / CHUNK_SIZE;
                printf("file exists, sending %ld bytes in %ld chunks...\n", st.st_size, chunks);
                // Mise en place du buffer
                rio_readinitb(&bufferRio, fdin); 
                while ((n = Rio_readnb(&bufferRio, buf, CHUNK_SIZE)) > 0) {
                    Rio_writen(connfd, buf, n);
                }
                close(fdin);
            }
        }
        printf("Finished transmission.\nClosing...\n");
}

