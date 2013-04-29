#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void error(const char *msg){
	perror(msg);
	exit(1);
}

int main(){
	int pid;
	char *args[]={"/home/todd/Documents/GitHubProjects/beagle-bone.git/TestScripts/childProcess","-s","hello.",0};
	if ((pid=fork())==0){//child
		setuid(0);
		execv(args[0],args);
		error("Unable to call process:");
	} else {
		waitpid(pid,NULL,0);
		//Print executed.
		system("cat /home/todd/Documents/GitHubProjects/beagle-bone.git/TestScripts/whatIGot.txt");
	}
	return 0;
}
