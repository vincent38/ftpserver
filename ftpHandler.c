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

// Authentication system for step 8

const char*FICHIER=".mnet";

unsigned long hashPwd(const char*u, const char*p) {
    // You can change this to your preferred password hash system.
    // Do not disclose it, or somebody can try to force access by
    // guessing a password with same hashcode !
	unsigned long code = u[0] * p[0];
	for(int i = 1; i < strlen(p); i++) {
		if(i < strlen(u)) {
			code = code * i * u[i] * p[i];
		} else {
			code = code * i * p[i];
		}
        printf("%lu\n", code);
	}
	return code;
}

int logMeIn(const char *user, const char *pass) {
    // Hashes the password and checks if user exists and password is identical
	FILE*f=NULL;
	f=fopen(FICHIER,"r");
	if(f!=NULL) {
        char util[64];
        unsigned long passw;
        while(fscanf(f, "%s :%lu\n", util, &passw) != EOF) {
            if(strcmp(user,util)==0) {
                printf("%lu %lu\n", hashPwd(user,pass), passw);
				if(hashPwd(util,pass) == passw) {
					fclose(f);
					printf("LOGGED IN !\n");
					return 0;
				} else {
                    printf("ERROR : WRONG PASSWORD !\n");
				    return 1;
                }
            }
        }
        printf("ERROR : USER NOT REGISTERED !\n");
		fclose(f);
		return 2;
	}
	printf("ERROR : FILE NOT FOUND !");
	return 3;
}

int checkRegistration(const char*util) {
    // Checks if user is already registered
	FILE*f=NULL;
    f=fopen(FICHIER,"r");
    if(f!=NULL) {
    	char user[64];
		char passw[64];
        while(fscanf(f, "%s :%s\n", user, passw) != EOF) {
            if(strcmp(util, user)==0) {
				fclose(f);
				return 1;
            }
        }
        fclose(f);
        return 0;
    }
    return 0;
}

int addMeIn(const char*user, const char*passw) {
    // Adds a new user in the database
    FILE*f=NULL;
    f=fopen(FICHIER,"a+");
    if(f!=NULL) {
    	if(checkRegistration(user) == 0) {
			unsigned long password = hashPwd(user,passw);
            printf("%lu\n", password);
			fprintf(f,"%s :%lu\n",user,password);
			fclose(f);
			printf("USER ADDED !\n");
			return 0;
    	}
        fclose(f);
        printf("ERROR : USER ALREADY EXISTING IN DATBASE !\n");
        return 1;
    }
    printf("ERROR : FILE NOT FOUND !\n");
    return 2;
}

// My own custom remove directory

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

// The handler, which manages all commands

void ftpHandler(int connfd)
{
    size_t n;
    rio_t bufferRio;
    int fdin;
    size_t CHUNK_SIZE = 10000; // If edited, please make sure it is the same value in both server and client !
    char buf[CHUNK_SIZE];
    char fileName[MAXLINE];
    char cmd[MAXLINE];
    char username[MAXLINE];
    rio_t rio;
    struct stat st;
    int login = 0; // 0 if not logged in, 1 otherwise

        // We open the command buffer
        Rio_readinitb(&rio, connfd);

        do {
            if ((n = Rio_readlineb(&rio, cmd, MAXLINE)) > 0){

                // Catch the command that we want to do and purge it
                printf("server received %u bytes\n", (unsigned int)n);
                char okCmd[n+1];
                int i;

                for (i = 0; i < n; i++){
                    okCmd[i] = cmd[i];
                }
                okCmd[strcspn(okCmd,"\n")] = 0;
                
                // Debug print
                printf("%s\n",okCmd);

                // Commands -> GET, REC, PUT, CMD, BYE (basic get/put cmd + recovery + bye + cmd which indicates that we are askingf for a specific cmd)
                
                if (strcmp(okCmd, "GET") == 0 || strcmp(okCmd, "REC") == 0) {
                    if ((n = Rio_readlineb(&rio, fileName, MAXLINE)) != 0){
                        // Catch the file name
                        printf("server received %u bytes\n", (unsigned int)n);
                        char okFileName[n+1];
                        int j;

                        for (j = 0; j < n; j++){
                            okFileName[j] = fileName[j];
                        }
                        okFileName[strcspn(okFileName,"\r\n")] = 0;

                        printf("asking for file : %s\n", okFileName);

                        // Opens the file
                        fdin = open(okFileName, O_RDONLY, 0);

                        if (fdin == -1) {
                            // Does not exists, error code
                            printf("file does not exists !!!\n");
                            Rio_writen(connfd, "550 DOES_NOT_EXISTS\n", 20);
                        } else {
                            // Exists, check the file statistics
                            stat(okFileName, &st);

                            // Success code, transferring in progress
                            Rio_writen(connfd, "100 STARTING_TRANSFER\n", 22);
                            long int chunks;

                            // This is a rescue, specific part of fole only is to be sent
                            if(strcmp(okCmd, "REC") == 0){
                                size_t part;

                                if ((n = Rio_readlineb(&rio, &part, sizeof(size_t))) != 0){
                                    // Got the "already downloaded" size, only seeks what's interesting and send it
                                    printf("Looking for part of file starting %zu\n", part);
                                    Lseek(fdin, part, SEEK_SET);
                                    size_t nSize = st.st_size - part;
                                    chunks = nSize / CHUNK_SIZE;
                                    printf("file exists, sending %ld bytes in %ld chunks...\n", nSize, chunks);
                                    Rio_writen(connfd, &nSize, sizeof(size_t)-1);
                                } else {
                                    // Error, should NEVER happens
                                }
                            } else {
                                // Full file
                                chunks = st.st_size / CHUNK_SIZE;
                                printf("file exists, sending %ld bytes in %ld chunks...\n", st.st_size, chunks);
                                Rio_writen(connfd, &st.st_size, sizeof(size_t)-1);
                            }

                            // We sent the size of the file that we are going to transfer (or the part if concerned)

                            size_t rSent = 0;

                            // Mise en place du buffer
                            rio_readinitb(&bufferRio, fdin); 

                            // Gonna send x chunks + the remaining data, client will catch x chunks + remaining data
                            while (chunks > 0) {
                                if ((n = Rio_readnb(&bufferRio, buf, CHUNK_SIZE)) > 0) {
                                    Rio_writen(connfd, buf, CHUNK_SIZE);
                                    rSent += n;
                                    chunks -= 1;
                                }
                            }

                            if ((n = Rio_readnb(&bufferRio, buf, CHUNK_SIZE)) > 0) {
                                Rio_writen(connfd, buf, n);
                                rSent += n;
                            }

                            // Over, we sent the file !
                            close(fdin);
                        }
                    } 
                } else if (strcmp(okCmd, "PUT") == 0) {
                    // Put the file from client
                    if (login == 0) {
                        // Not logged in, go away
                        Rio_writen(connfd, "530 NOT_LOGGED_IN\n", 18);
                    } else {
                        // Let's go
                        Rio_writen(connfd, "100 PROCESSING_CMD\n", 19);
                        if ((n = Rio_readlineb(&rio, fileName, MAXLINE)) != 0){
                            // Get the filename
                            char okFileName[n+1];
                            int j;

                            for (j = 0; j < n; j++){
                                okFileName[j] = fileName[j];
                            }
                            okFileName[strcspn(okFileName,"\r\n")] = 0;
                            
                            // Create it
                            fdin = Open(okFileName, O_WRONLY | O_CREAT, 0644);

                            size_t expectedSize, totalSize = 0;

                            // Receive the sent size
                            if ((n = Rio_readlineb(&rio, &expectedSize, sizeof(size_t))) != 0){
                                printf("Awaiting %ld bytes !\n", expectedSize);
                            }

                            long int chunks = expectedSize / CHUNK_SIZE;

                            // Receive x chunks + remaining data
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
                            // Done !
                            printf("Upload succesfully complete.\n");
                        }
                    }
               
                } else if (strcmp(okCmd, "CMD") == 0) {
                    // Specific remote command called ! They are all handled here. 
                    // CMD is there to distinguish basic send/receive commands from extra features
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
                            // List all files and directories
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
                            // Send the number of entries
                            while((file = readdir(wd)) != NULL){
                                //printf("%s ", file->d_name);
                                Rio_writen(connfd, &file->d_name, MAXLINE);
                                // Send each entry
                            }
                            //printf("\n");
                            closedir(wd);
                            // finished
                        } else if (strcmp(sC, "PWD") == 0) {
                            // Where are we now ?
                            printf("We will call pwd.\n");
                            char pwd[MAXLINE];
                            if (getcwd(pwd, sizeof(pwd)) != NULL) {
                                Rio_writen(connfd, &pwd, MAXLINE);
                                // Only send the returned wd
                            }
                        } else if (strcmp(sC, "CD") == 0) {
                            // Change directory
                            printf("We will call cd.\n");
                            char dir[MAXLINE];
                            if ((n = Rio_readlineb(&rio, dir, MAXLINE)) != 0){
                                char okNewDir[n+1];
                                int j;

                                for (j = 0; j < n; j++){
                                    okNewDir[j] = dir[j];
                                }
                                okNewDir[strcspn(okNewDir,"\r\n")] = 0;
                                // Get the name of the new dir

                                if (chdir(okNewDir) == 0){
                                    char pwd[MAXLINE];
                                    if (getcwd(pwd, sizeof(pwd)) != NULL) {
                                        Rio_writen(connfd, &pwd, MAXLINE);
                                        // Send the new working directory
                                    }
                                } else {
                                    Rio_writen(connfd, "An error occured, staying there.", MAXLINE);
                                }
                            }
                        } else if (strcmp(sC, "MKDIR") == 0) {
                            // Make a new dir, needs login
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
                                    // Get the new dir name, and make it with all permissions for user + group + RX for others
                                    // Or error if couldn't

                                    if (mkdir(okNewDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0){
                                        Rio_writen(connfd, "Folder created.", MAXLINE);
                                    } else {
                                        Rio_writen(connfd, "Couldn't create the folder.", MAXLINE);
                                    }
                                }
                            }
                        } else if (strcmp(sC, "RMFILE") == 0) {
                            // Remove a single file, needs login
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
                                    // Get name and remove, or send error

                                    if (remove(okNewDir) == 0){
                                        Rio_writen(connfd, "Target removed.", MAXLINE);
                                    } else {
                                        Rio_writen(connfd, "Couldn't remove the target.", MAXLINE);
                                    }
                                }
                            }
                        } else if (strcmp(sC, "RMDIR") == 0) {
                            // Removes directory with recursive, needs login
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
                                    // Get name of dir, call myRemoveDir (removes everything recursively), and depending of the return code,
                                    // sends a OK message or a error

                                    int rCode = myRemoveDir(okNewDir);

                                    if (rCode == 0){
                                        Rio_writen(connfd, "Directory removed.", MAXLINE);
                                    } else {
                                        Rio_writen(connfd, "Couldn't remove the Directory.", MAXLINE);
                                    }
                                }
                            }
                        } else if (strcmp(sC, "AUTH") == 0) {
                            // Authentication with existing user, no user change existing for now.
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
                                        //printf("%s\n", pass);

                                        int rCode = logMeIn(username, pass);

                                        if (rCode == 0){
                                            Rio_writen(connfd, "AUTH success - Logged in.", MAXLINE);
                                            printf("%s logged in\n", username);
                                            login = 1;
                                        } else if (rCode == 1) {
                                            Rio_writen(connfd, "AUTH failure - bad username/password.", MAXLINE);
                                            printf("%s login failed\n", username);
                                            login = 0;
                                        } else {
                                            Rio_writen(connfd, "SEVERE AUTH failure - can't read accounts file ?!", MAXLINE);
                                            printf("%s login failed - file problem ?!\n", username);
                                            login = 0;
                                        }
                                    }
                                }
                            } 
                        } else if (strcmp(sC, "ADDUSR") == 0) {
                            // Add a user in database, needs login
                            if (login == 0) {
                                Rio_writen(connfd, "530 NOT_LOGGED_IN\n", 18);
                            } else {
                                Rio_writen(connfd, "100 PROCESSING_CMD\n", 19);
                                // Catch username
                                if ((n = Rio_readlineb(&rio, username, MAXLINE)) != 0){
                                    
                                    char pass[MAXLINE];
                                    username[strlen(username)-1] = '\0';
                                    printf("Trying to add user %s\n", username);

                                    // Catch password
                                    if ((n = Rio_readlineb(&rio, pass, MAXLINE)) != 0){
                                        pass[strlen(pass)-1] = '\0';
                                        printf("%s\n", pass);

                                        int rCode = addMeIn(username, pass);

                                        if (rCode == 0){
                                            Rio_writen(connfd, "ADDUSR success - user account added in database.", MAXLINE);
                                        } else if (rCode == 1) {
                                            Rio_writen(connfd, "ADDUSR failure - user already exists", MAXLINE);
                                        } else {
                                            Rio_writen(connfd, "SEVERE ADDUSR failure - can't read accounts file ?!", MAXLINE);
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else if (strcmp(okCmd, "BYE") == 0) {
                    // Client left, break
                    printf("Client disconnected.\n");
                    break;
                } else {
                    // Not existing as of today
                    printf("Unknown command...\n");
                }
            }
        } while(1);
        printf("Closing...\n");
}

