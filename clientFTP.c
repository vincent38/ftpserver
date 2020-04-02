/*
 * echoclient.c - An echo client
 */
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include "csapp.h"
#include <termios.h>

int clientfd;

void crush(int sig){
    // Client forced to close, we cut properly the socket and send a BYE command.
	Rio_writen(clientfd, "BYE\n", 4);
    Close(clientfd);
    exit(0);
}

// Used to split our command string in a char**
char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

int main(int argc, char **argv)
{
    int port;
    char *host, buf[MAXLINE];
    char reply[MAXLINE];
    rio_t rio;
    rio_t bufferRio;
    int fdin;
    int fdstat;
    size_t n;
    struct stat st;
    size_t CHUNK_SIZE = 10000; // If edited, please make sure it is the same value in both server and client !

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
    signal(SIGINT, crush);
    
    Rio_readinitb(&rio, clientfd);
    printf("ftp> ");
    // Connected, display the prompt

    do {
        while (Fgets(buf, MAXLINE, stdin) != NULL) {
            // Waits for a command, and breaks it into an array
            char **cmd = str_split(buf, ' ');

            // Checks which functionnality we are calling
            if (strcmp(cmd[0], "get") == 0){
                // Getting a file from remote server
                printf("This is get\n");
                Rio_writen(clientfd, "GET\n", 4);
                // Get the filename
                char okFileName[strlen(cmd[1])];
                int i;
                for (i = 0; i < strlen(cmd[1]); i++){
                    okFileName[i] = cmd[1][i];
                }
                okFileName[strcspn(okFileName,"\r\n")] = 0;

                Rio_writen(clientfd, cmd[1], strlen(cmd[1]));
                // Sends the filename to the server, and waits for a anwser
                
                // Create our temp file - holds the name of the processed file for the recover command
                fdstat = Open("status.tmp", O_RDWR | O_CREAT, 0644);
                rio_writen(fdstat, cmd[1], strlen(cmd[1]));
                Close(fdstat);

                //end of section
                char code[4];

                totalSize = 0;
                start = clock();
                // Starts timer for status check

                if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                    //printf("%s\n", buf);
                    strncpy(code, buf, 3);
                    //printf("%s\n", code);
                    // We catch the reply code
                    if (strcmp(code, "100") == 0) {
                        // Server is sending ! Catch size, determine how many chunks we are going to fetch + the remaining data size
                        printf("Server replied, receiving file...\n");

                        // Opens the new file
                        fdin = Open(okFileName, O_WRONLY | O_CREAT, 0644);
                        size_t expectedSize;

                        if ((n = Rio_readlineb(&rio, &expectedSize, sizeof(size_t))) != 0){
                            printf("Awaiting %ld bytes !\n", expectedSize);
                        }

                        long int chunks = expectedSize / CHUNK_SIZE;

                        // Catch the data
                        while (chunks > 0) {
                            if ((n = Rio_readnb(&rio, buf, CHUNK_SIZE)) > 0) {
                                totalSize += rio_writen(fdin, buf, n);
                                chunks -= 1;
                            }
                        }

                        if ((n = Rio_readnb(&rio, buf, (expectedSize - totalSize))) > 0) {
                            totalSize += rio_writen(fdin, buf, n);
                        }

                        end = clock();
                        // Finished, print statistics and timing

                        Close(fdin);

                        cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

                        printf("Transfer succesfully complete.\n");

                        ssize_t bandwidth = (totalSize/1000) / cpu_time_used;

                        if (bandwidth > 0){
                            printf("%ld bytes received in %f seconds (%ld Kbytes/s)\n", totalSize, cpu_time_used, bandwidth);
                        } else {
                            printf("%ld bytes received in %f seconds (inf Kbytes/s)\n", totalSize, cpu_time_used);
                        }

                        // We nailed it, remove the now useless status.tmp
                        remove("status.tmp");
                    } else if (strcmp(code, "550") == 0){
                        // File does not exists, well, quit
                        printf("File does not exists remotely !\n");
                        remove("status.tmp");
                    } else {
                        // Something happened :(
                        printf("Unknown error.\n");
                    }
                }
            } else if (strcmp(cmd[0], "recover\n") == 0) {
                // We are recovering a partially-downloaded file !
                printf("Check for file lock...\n");
                // Check if there is already a lock - if yes, a previous dl was interrupted !
                fdstat = open("status.tmp", O_RDONLY, 0);
                if (fdstat != -1) {
                    printf("File lock status.tmp found ! Recovering file...\n");
                    // There is a lock
                    rio_readinitb(&bufferRio, fdstat); 
                    if ((n = Rio_readlineb(&bufferRio, buf, MAXLINE)) > 0) {
                        Rio_writen(clientfd, "REC\n", 4);
                        // Get the filename
                        char okFileName[n];
                        int i;

                        for (i = 0; i < n; i++){
                            okFileName[i] = buf[i];
                        }

                        okFileName[strcspn(okFileName,"\r\n")] = 0;

                        stat(okFileName, &st);
                        printf("already has %ld bytes of file %s\n", st.st_size, okFileName);

                        // File name and size
                        Rio_writen(clientfd, buf, strlen(buf));

                        char code[4];

                        if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                            strncpy(code, buf, 3);
                            if (strcmp(code, "550") == 0){
                                // Does not exists, fake lock ? We kill it anyways
                                printf("File does not exists remotely ! Closing connection...\n");
                                remove("status.tmp");
                            } else if (strcmp(code, "100") == 0) {
                                // Exists, we catch up
                                printf("Server replied, receiving file...\n");
                                Rio_writen(clientfd, &(st.st_size), sizeof(size_t)-1);

                                fdin = Open(okFileName, O_WRONLY|O_APPEND, 0644);
                                
                                size_t expectedSize;

                                if ((n = Rio_readlineb(&rio, &expectedSize, sizeof(size_t))) != 0){
                                    printf("Awaiting %ld bytes !\n", expectedSize);
                                }

                                long int chunks = expectedSize / CHUNK_SIZE;
                                totalSize = 0;
                                
                                while (chunks > 0) {
                                    if ((n = Rio_readnb(&rio, buf, CHUNK_SIZE)) > 0) {
                                        totalSize += rio_writen(fdin, buf, n);
                                        chunks -= 1;
                                    }
                                }

                                if ((n = Rio_readnb(&rio, buf, (expectedSize - totalSize))) > 0) {
                                    totalSize += rio_writen(fdin, buf, n);
                                }

                                printf("Situation recovered ! %ld bytes downloaded to complete the file.\n", totalSize);
                                Close(fdin);
                                close(fdstat);
                                remove("status.tmp");
                                // Recovered, we can close the now complete file and remove the lock
                            } else {
                                // Oh well
                                printf("Unknown error...\n");
                            }
                        }
                    }
                } else {
                    // Was status.tmp externally destroyed or something like that ? Anyways, there is no lock. Abort.
                    printf("Nothing to recover, or the file status.tmp has been deleted by another application.\n");
                    //break;
                }
            } else if (strcmp(cmd[0], "put") == 0) {
                // We are putting a file, prepare the data and the commands
                // Get the filename
                char okFileName[strlen(cmd[1])];
                int i;
                for (i = 0; i < strlen(cmd[1]); i++){
                    okFileName[i] = cmd[1][i];
                }
                okFileName[strcspn(okFileName,"\r\n")] = 0;

                    printf("Putting file %s on remote server...\n", okFileName);

                    // Ouverture du fichier
                    fdin = open(okFileName, O_RDONLY, 0);

                    if (fdin == -1) {
                        // File ? What file ?
                        printf("file does not exists !!!\n");
                    } else {
                        // Nah jk, preepare it for sending
                        stat(okFileName, &st);
                        long int chunks = st.st_size / CHUNK_SIZE;
                        printf("file exists, sending %ld bytes in %ld chunks...\n", st.st_size, chunks);

                        Rio_writen(clientfd, "PUT\n", 4);
                        // Ask to PUT a file
                        char code[4];

                        if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                            strncpy(code, buf, 3);
                            if (strcmp(code, "530") == 0){
                                // Not logged in, abort (server won't accept your file even if you force send it).
                                printf("Action not permitted, account needed !\n");
                            } else if (strcmp(code, "100") == 0) {
                                // All green on track 1
                                Rio_writen(clientfd, cmd[1], strlen(cmd[1]));
                                Rio_writen(clientfd, &st.st_size, sizeof(size_t)-1);
                                // Mise en place du buffer
                                rio_readinitb(&bufferRio, fdin); 

                                size_t rSent = 0;

                                // Same send as in the server

                                while (chunks > 0) {
                                    if ((n = Rio_readnb(&bufferRio, buf, CHUNK_SIZE)) > 0) {
                                        Rio_writen(clientfd, buf, CHUNK_SIZE);
                                        rSent += n;
                                        chunks -= 1;
                                    }
                                }

                                if ((n = Rio_readnb(&bufferRio, buf, CHUNK_SIZE)) > 0) {
                                    Rio_writen(clientfd, buf, n);
                                    rSent += n;
                                    //printf("Sent remaining data\n");
                                }

                                close(fdin);
                            }
                        }
                    }
            
            } else if (strcmp(cmd[0], "ls\n") == 0) {
                // Extra command - ls - prints the nb of elements and their names (at least 2 : . and ..)
                Rio_writen(clientfd, "CMD\n", 4);
                Rio_writen(clientfd, "LS\n", 3);
                int count;
                if ((n = Rio_readnb(&rio, &count, sizeof(int))) > 0) {
                    printf("There are %d elements on the current folder\n", count);
                    while(count > 0){
                        if ((n = Rio_readnb(&rio, &reply, MAXLINE)) > 0) {
                            printf("\t%s ", reply);
                        }
                        printf("\n");
                        count -= 1;
                    }
                }
            } else if (strcmp(cmd[0], "pwd\n") == 0) {
                // Extra command - pwd - prints wd
                Rio_writen(clientfd, "CMD\n", 4);
                Rio_writen(clientfd, "PWD\n", 4);
                if ((n = Rio_readnb(&rio, &reply, MAXLINE)) > 0) {
                    printf("Working dir : %s\n", reply);
                }
            } else if (strcmp(cmd[0], "cd") == 0) {
                // Extra command - cd - prints new wd or error
                Rio_writen(clientfd, "CMD\n", 4);
                Rio_writen(clientfd, "CD\n", 3);
                Rio_writen(clientfd, cmd[1], strlen(cmd[1]));
                if ((n = Rio_readnb(&rio, &reply, MAXLINE)) > 0) {
                    printf("New wd : %s\n", reply);
                }
            } else if (strcmp(cmd[0], "mkdir") == 0) {
                // Extra command - mkdir - prints status - needs login
                Rio_writen(clientfd, "CMD\n", 4);
                Rio_writen(clientfd, "MKDIR\n", 6);
                char code[4];

                if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                    strncpy(code, buf, 3);
                    if (strcmp(code, "530") == 0){
                        printf("Action not permitted, account needed !\n");
                    } else {
                        Rio_writen(clientfd, cmd[1], strlen(cmd[1]));
                        if ((n = Rio_readnb(&rio, &reply, MAXLINE)) > 0) {
                            printf("Reply : %s\n", reply);
                        }
                    }
                }
                
            } else if (strcmp(cmd[0], "rm") == 0) {
                // Extra command - rm and rm -r - removes file/directory and prints status - needs login
                Rio_writen(clientfd, "CMD\n", 4);
                char code[4];
                if (strcmp(cmd[1], "-r") == 0) {
                    // Remove dir mode
                    Rio_writen(clientfd, "RMDIR\n", 6);
                    if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                        strncpy(code, buf, 3);
                        if (strcmp(code, "530") == 0){
                            printf("Action not permitted, account needed !\n");
                        } else {
                            Rio_writen(clientfd, cmd[2], strlen(cmd[2]));
                            if ((n = Rio_readnb(&rio, &reply, MAXLINE)) > 0) {
                                printf("Reply : %s\n", reply);
                            }
                        }
                    }
                } else {
                    // Remove file mode
                    Rio_writen(clientfd, "RMFILE\n", 7);
                    if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                        strncpy(code, buf, 3);
                        if (strcmp(code, "530") == 0){
                            printf("Action not permitted, account needed !\n");
                        } else {
                            Rio_writen(clientfd, cmd[1], strlen(cmd[1]));
                            if ((n = Rio_readnb(&rio, &reply, MAXLINE)) > 0) {
                                printf("Reply : %s\n", reply);
                            }
                        }
                    }
                }
                
            } else if (strcmp(cmd[0], "auth") == 0) {
                // Login on the server - only if not yet login
                Rio_writen(clientfd, "CMD\n", 4);
                Rio_writen(clientfd, "AUTH\n", 5);
                char code[4];

                if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                    strncpy(code, buf, 3);
                    if (strcmp(code, "230") == 0){
                        printf("You are already logged in !\n");
                    } else {
                        Rio_writen(clientfd, cmd[1], strlen(cmd[1]));
                        printf("PASSWORD> ");
                        
                        if (Fgets(buf, MAXLINE, stdin) != NULL) {
                            Rio_writen(clientfd, buf, strlen(buf));
                            if ((n = Rio_readnb(&rio, &reply, MAXLINE)) > 0) {
                                printf("Reply : %s\n", reply);
                            }
                        } else {
                            printf("Please provide a password...");
                        }
                    }
                }
            } else if (strcmp(cmd[0], "addusr") == 0) {
                // Create an account - login needed
                Rio_writen(clientfd, "CMD\n", 4);
                Rio_writen(clientfd, "ADDUSR\n", 7);
                char code[4];

                if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                    strncpy(code, buf, 3);
                    if (strcmp(code, "530") == 0){
                        printf("Action not permitted, account needed !\n");
                    } else {
                        Rio_writen(clientfd, cmd[1], strlen(cmd[1]));
                        printf("PASSWORD FOR NEW USER> ");
                        
                        if (Fgets(buf, MAXLINE, stdin) != NULL) {
                            Rio_writen(clientfd, buf, strlen(buf));
                            if ((n = Rio_readnb(&rio, &reply, MAXLINE)) > 0) {
                                printf("Reply : %s\n", reply);
                            }
                        } else {
                            printf("Please provide a password...");
                        }
                    }
                }
                
            } else if (strcmp(cmd[0], "bye\n") == 0) {
                // Say bye kevin
                printf("Bye.\n");
                Rio_writen(clientfd, "BYE\n", 4);
                Close(clientfd);
                exit(0);
            } else {
                // Sorry not sorry
                printf("Unknown command !\n");
            }
            // Reprompt after command
            printf("ftp> ");
        }
    } while(1);

}
