#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

void error(const char *msg);
void *gpsThread(void *args);
bool pShutdown(void);

int pidShutdown;

int main(){
	pthread_t newThread;
	pthread_attr_t	attr;
	int ids=0;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
	pthread_create(&newThread,&attr,gpsThread,&ids);

	printf("Done with thread.\n");
	bool success=pShutdown();
	printf("Done with forker.\n");
	return 0;
}

void error(const char *msg){
	perror(msg);
}

void *gpsThread(void *args){
	char *myArgs[]={"/home/todd/Documents/GitHubProjects/beagle-bone.git/TestScripts/childProcess -s thread"};
	int pid1=fork();
	pipe(myPipe);
	if (pid1==0){
		system(myArgs[0]);
	} else {	
		waitpid(pid1,NULL,0);
	}
	pthread_exit(NULL);
}

bool pShutdown(void){
	char *myArgs[]={"/home/todd/Documents/GitHubProjects/beagle-bone.git/TestScripts/childProcess -s process"};
	pidShutdown=fork();
	char *envp[]={"NULL"};
	if (pidShutdown==0){
		system(myArgs[0]);
		error("Wasn't able to call child process");
	} 
	waitpid(pidShutdown,NULL,0);

	return true;
}
