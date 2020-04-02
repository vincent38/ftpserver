#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char*FICHIER="test.txt";

unsigned long toInt(char*val) {
	unsigned long res=0;
	for(int i=0;i<strlen(val);i++) {
        res=res*10+(unsigned long)val[i]-(unsigned long)'0';
	}
	return res;
}

unsigned long hash(char*u, char*p) {
	unsigned long code=u[0]*p[0];
	for(int i=1;i<strlen(p);i++) {
		if(i<strlen(u)) {
			code=code*i*u[i]*p[i];
		} else {
			code=code*i*p[i];
		}
	}
	return code;
}

int logMeIn(const char *user, const char *pass) {
	FILE*f=NULL;
	f=fopen(FICHIER,"r");
	if(f!=NULL) {
        char util[32];
        char passw[32];
        while(fscanf(f, "%s :%s\n", util, passw) != EOF) {
            if(strcmp(user,util)==0) {
				if(hash(util,pass)==toInt(passw)) {
					fclose(f);
					printf("LOGGED IN !\n");
					return 0;
				}
				printf("ERROR : WRONG PASSWORD !\n");
				return 1;
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
	FILE*f=NULL;
    f=fopen(FICHIER,"r");
    if(f!=NULL) {
    	char user[64];
		char passw[64];
        while(fscanf(f, "%s :%s\n", user, passw) != EOF) {
            if(strcmp(util, user)==0) {
				fclose(f);
				return(1);
            }
        }
        fclose(f);
        return 0;
    }
}

int addMeIn(const char*user, const char*passw) {
    FILE*f=NULL;
    f=fopen(FICHIER,"a+");
    if(f!=NULL) {
    	if(checkRegistration(user)==0) {
			int password=hash(user,passw);
			fprintf(f,"\n%s :%lu",user,password);
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
/*
// main used for test
int main() {
	logMeIn("edouard","mlp");
	logMeIn("megumi","cureFlora");
	addMeIn("megumi","cureFlora");
	logMeIn("megumi","cureFlora");
	addMeIn("FX","YOLO!");
	addMeIn("bardeluc","colerecolere");
}
*/

