/*
 * echo - read and echo text lines until client closes connection
 */
#include "csapp.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>

int myRemoveDir(const char *okNewDir) {
    DIR *wd = opendir(okNewDir);
    size_t pLen = strlen(okNewDir);
    int r = -1;

    if (wd) {
        struct dirent *file;
        r = 0;
        while(!r && (file = readdir(wd))) {
            int r2 = -1;
            char *buffer;
            size_t length;
            if (!strcmp(file->d_name, ".") || !strcmp(file->d_name, "..")) {
                continue;
            }
            length = pLen + strlen(file->d_name) + 2;
            buffer = malloc(length);
            if (buffer) {
                struct stat sBuffer;
                snprintf(buffer, length, "%s/%s", okNewDir, file->d_name);
                if(!stat(buffer, &sBuffer)) {
                    if (S_ISDIR(sBuffer.st_mode)) {
                        r2 = myRemoveDir(buffer);
                    } else {
                        r2 = unlink(buffer);
                    }
                }
                free(buffer);
            }
            r = r2;
        }
        closedir(wd);
    }

    if (!r) {
        r = rmdir(okNewDir);
    }

    return r;
}

int logMeIn(const char *username, const char *pass) {
    FILE *fp;
    char un[50], pw[50];
 
    fp = fopen(".mnet", "r");
 
    if(fp == NULL)
    {
        printf("Error opening file\n");
        return -1;
    }
 
    while( fscanf(fp, "%s :%s\n", un, pw) != EOF )
    {
        if ((strcmp(username, un) == 0) && (strcmp(pass, pw) == 0)) {
            fclose(fp);
            return 0;
        }
    }
    
    fclose(fp);
    return 1;
}

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
    size_t CHUNK_SIZE = 10000;
    char buf[CHUNK_SIZE];
    char fileName[MAXLINE];
    char cmd[MAXLINE];
    char username[MAXLINE];
    rio_t rio;
    struct stat st;
    int login = 0; // 0 if not logged in, 1 otherwise

        Rio_readinitb(&rio, connfd);

        do {
            if ((n = Rio_readlineb(&rio, cmd, MAXLINE)) > 0){
                // Catch the command that we want to do
                printf("server received %u bytes\n", (unsigned int)n);
                char okCmd[n+1];
                int i;

                for (i = 0; i < n; i++){
                    okCmd[i] = cmd[i];
                }
                okCmd[strcspn(okCmd,"\n")] = 0;

                printf("%s\n",okCmd);
                
                if (strcmp(okCmd, "GET") == 0 || strcmp(okCmd, "REC") == 0) {
                    if ((n = Rio_readlineb(&rio, fileName, MAXLINE)) != 0){
                        // Catch the file name (will change shortly)
                        printf("server received %u bytes\n", (unsigned int)n);
                        char okFileName[n+1];
                        int j;

                        for (j = 0; j < n; j++){
                            okFileName[j] = fileName[j];
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

                            Rio_writen(connfd, "100 STARTING_TRANSFER\n", 22);
                            long int chunks;

                            if(strcmp(okCmd, "REC") == 0){
                                size_t part;

                                if ((n = Rio_readlineb(&rio, &part, sizeof(size_t))) != 0){
                                    printf("Looking for part of file starting %zu\n", part);
                                    Lseek(fdin, part, SEEK_SET);
                                    size_t nSize = st.st_size - part;
                                    chunks = nSize / CHUNK_SIZE;
                                    printf("file exists, sending %ld bytes in %ld chunks...\n", nSize, chunks);
                                    Rio_writen(connfd, &nSize, sizeof(size_t)-1);
                                }
                            } else {
                                chunks = st.st_size / CHUNK_SIZE;
                                printf("file exists, sending %ld bytes in %ld chunks...\n", st.st_size, chunks);
                                Rio_writen(connfd, &st.st_size, sizeof(size_t)-1);
                            }

                            size_t rSent = 0;

                            // Mise en place du buffer
                            rio_readinitb(&bufferRio, fdin); 

                            // Gonna send x chunks + the remaining data, client will catch x chunks + remaining data
                            while (chunks > 0) {
                                if ((n = Rio_readnb(&bufferRio, buf, CHUNK_SIZE)) > 0) {
                                    Rio_writen(connfd, buf, CHUNK_SIZE);
                                    rSent += n;
                                    //printf("Sent chunk %ld\n", chunks);
                                    chunks -= 1;
                                }
                            }

                            if ((n = Rio_readnb(&bufferRio, buf, CHUNK_SIZE)) > 0) {
                                Rio_writen(connfd, buf, n);
                                rSent += n;
                                //printf("Sent remaining data\n");
                            }

                            printf("I'm out !\n");
                            close(fdin);
                        }
                    } 
                } else if (strcmp(okCmd, "PUT") == 0) {
                    if (login == 0) {
                        Rio_writen(connfd, "530 NOT_LOGGED_IN\n", 18);
                    } else {
                        Rio_writen(connfd, "100 PROCESSING_CMD\n", 19);
                        if ((n = Rio_readlineb(&rio, fileName, MAXLINE)) != 0){
                            char okFileName[n+1];
                            int j;

                            for (j = 0; j < n; j++){
                                okFileName[j] = fileName[j];
                            }
                            okFileName[strcspn(okFileName,"\r\n")] = 0;

                            fdin = Open(okFileName, O_WRONLY | O_CREAT, 0644);

                            size_t expectedSize, totalSize = 0;

                            if ((n = Rio_readlineb(&rio, &expectedSize, sizeof(size_t))) != 0){
                                printf("Awaiting %ld bytes !\n", expectedSize);
                            }

                            long int chunks = expectedSize / CHUNK_SIZE;
                    
                            while (chunks > 0) {
                                if ((n = Rio_readnb(&rio, buf, CHUNK_SIZE)) > 0) {
                                    totalSize += rio_writen(fdin, buf, n);
                                    chunks -= 1;
                                }
                            }

                            if ((n = Rio_readnb(&rio, buf, (expectedSize - totalSize))) > 0) {
                                totalSize += rio_writen(fdin, buf, n);
                            }

                            Close(fdin);
                            printf("Upload succesfully complete.\n");
                        }
                    }
               
                } else if (strcmp(okCmd, "CMD") == 0) {
                    printf("Remote command called !\n");
                    if ((n = Rio_readlineb(&rio, cmd, MAXLINE)) > 0){
                        char sC[n+1];
                        int i;

                        for (i = 0; i < n; i++){
                            sC[i] = cmd[i];
                        }
                        sC[strcspn(sC,"\n")] = 0;

                        printf("%s\n",sC);
                        if (strcmp(sC, "LS") == 0) {
                            printf("We will call ls.\n");
                            DIR *wd;
                            struct dirent *file;
                            wd = opendir("./");
                            int count = 0;
                            while((file = readdir(wd)) != NULL){
                                count += 1;
                            }
                            closedir(wd);
                            wd = opendir("./");
                            Rio_writen(connfd, &count, sizeof(int));
                            while((file = readdir(wd)) != NULL){
                                printf("%s ", file->d_name);
                                Rio_writen(connfd, &file->d_name, MAXLINE);
                            }
                            printf("\n");
                            closedir(wd);
                        } else if (strcmp(sC, "PWD") == 0) {
                            printf("We will call pwd.\n");
                            char pwd[MAXLINE];
                            if (getcwd(pwd, sizeof(pwd)) != NULL) {
                                Rio_writen(connfd, &pwd, MAXLINE);
                            }
                        } else if (strcmp(sC, "CD") == 0) {
                            printf("We will call cd.\n");
                            char dir[MAXLINE];
                            if ((n = Rio_readlineb(&rio, dir, MAXLINE)) != 0){
                                char okNewDir[n+1];
                                int j;

                                for (j = 0; j < n; j++){
                                    okNewDir[j] = dir[j];
                                }
                                okNewDir[strcspn(okNewDir,"\r\n")] = 0;

                                if (chdir(okNewDir) == 0){
                                    char pwd[MAXLINE];
                                    if (getcwd(pwd, sizeof(pwd)) != NULL) {
                                        Rio_writen(connfd, &pwd, MAXLINE);
                                    }
                                } else {
                                    Rio_writen(connfd, "An error occured, staying at home.", MAXLINE);
                                }
                            }
                        } else if (strcmp(sC, "MKDIR") == 0) {
                            if (login == 0) {
                                Rio_writen(connfd, "530 NOT_LOGGED_IN\n", 18);
                            } else {
                                Rio_writen(connfd, "100 PROCESSING_CMD\n", 19);
                                printf("We will call mkdir.\n");
                                char dir[MAXLINE];
                                if ((n = Rio_readlineb(&rio, dir, MAXLINE)) != 0){
                                    char okNewDir[n+1];
                                    int j;

                                    for (j = 0; j < n; j++){
                                        okNewDir[j] = dir[j];
                                    }
                                    okNewDir[strcspn(okNewDir,"\r\n")] = 0;

                                    if (mkdir(okNewDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0){
                                        Rio_writen(connfd, "Folder created.", MAXLINE);
                                    } else {
                                        Rio_writen(connfd, "Couldn't create the folder.", MAXLINE);
                                    }
                                }
                            }
                        } else if (strcmp(sC, "RMFILE") == 0) {
                            if (login == 0) {
                                Rio_writen(connfd, "530 NOT_LOGGED_IN\n", 18);
                            } else {
                                Rio_writen(connfd, "100 PROCESSING_CMD\n", 19);
                                printf("We will call rm.\n");
                                char dir[MAXLINE];
                                if ((n = Rio_readlineb(&rio, dir, MAXLINE)) != 0){
                                    char okNewDir[n+1];
                                    int j;

                                    for (j = 0; j < n; j++){
                                        okNewDir[j] = dir[j];
                                    }
                                    okNewDir[strcspn(okNewDir,"\r\n")] = 0;

                                    if (remove(okNewDir) == 0){
                                        Rio_writen(connfd, "Target removed.", MAXLINE);
                                    } else {
                                        Rio_writen(connfd, "Couldn't remove the target.", MAXLINE);
                                    }
                                }
                            }
                        } else if (strcmp(sC, "RMDIR") == 0) {
                            if (login == 0) {
                                Rio_writen(connfd, "530 NOT_LOGGED_IN\n", 18);
                            } else {
                                Rio_writen(connfd, "100 PROCESSING_CMD\n", 19);
                                printf("We will call rm -r.\n");
                                char dir[MAXLINE];
                                if ((n = Rio_readlineb(&rio, dir, MAXLINE)) != 0){
                                    char okNewDir[n+1];
                                    int j;

                                    for (j = 0; j < n; j++){
                                        okNewDir[j] = dir[j];
                                    }
                                    okNewDir[strcspn(okNewDir,"\r\n")] = 0;

                                    int rCode = myRemoveDir(okNewDir);

                                    if (rCode == 0){
                                        Rio_writen(connfd, "Directory removed.", MAXLINE);
                                    } else {
                                        Rio_writen(connfd, "Couldn't remove the Directory.", MAXLINE);
                                    }
                                }
                            }
                        } else if (strcmp(sC, "AUTH") == 0) {
                            if (login == 1) {
                                Rio_writen(connfd, "230 LOGGED_IN\n", 14);
                                printf("User already logged in.");
                            } else {
                                Rio_writen(connfd, "100 PROCESSING_CMD\n", 19);
                                // Catch username
                                if ((n = Rio_readlineb(&rio, username, MAXLINE)) != 0){
                                    
                                    char pass[MAXLINE];
                                    username[strlen(username)-1] = '\0';
                                    printf("%s tries to login\n", username);

                                    // Catch password
                                    if ((n = Rio_readlineb(&rio, pass, MAXLINE)) != 0){
                                        pass[strlen(pass)-1] = '\0';
                                        printf("%s\n", pass);

                                        int rCode = logMeIn(username, pass);

                                        if (rCode == 0){
                                            Rio_writen(connfd, "AUTH success - Logged in.", MAXLINE);
                                            printf("%s logged in\n", username);
                                        } else if (rCode == 1) {
                                            Rio_writen(connfd, "AUTH failure - bad username/password.", MAXLINE);
                                            printf("%s login failed\n", username);
                                        } else {
                                            Rio_writen(connfd, "SEVERE AUTH failure - can't read accounts file ?!", MAXLINE);
                                            printf("%s login failed - file problem ?!\n", username);
                                        }
                                        login = !rCode;
                                    }
                                }
                            }
                        }
                    }
                } else if (strcmp(okCmd, "BYE") == 0) {
                    printf("Client disconnected.\n");
                    break;
                } else {
                    printf("Unknown command...\n");
                }
            }
        } while(1);
        printf("Closing...\n");
}

