/*******************************************************************************\
| shutdownInterrupt.c
| Author: Todd Sukolsky
| Initial Build: 4/15/2013
| Last Revised: 4/16/2013
|================================================================================
| Description: This module is a spin-off of commandCenter.c. It is used to 
|	alert the BeagleBone that the GAVR is sending it something. Blocks on read.
|	When hit it halts everything.
|--------------------------------------------------------------------------------
| Revisions: 4/15: Initial build/take.
|	     4/16: Added blocking. Should block when there we open the file, as long as it's set to rising > edge.
|		   Changed to work with online script. This script works and is complete.
|================================================================================
| *NOTES:Polling example found at: http://bwgz57.wordpress.com/tag/beaglebone/
\*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include "pollingExtras.h"

#define logpath "/home/root/Documents/tmp/logs/actions.log"
int main(){
	//Declare a file descriptor and open the file we need as read only.
	int fd;
	fd = open("/sys/class/gpio/gpio35/value", O_RDONLY);		//this is the file for shutdown request to come. Oringally supposed to be GPIO2_2, but that is a dead end.
	struct pollfd pfd;
	//Intiailize polling file as the file descriptor	
	pfd.fd = fd;
	pfd.events = POLLPRI;				//Urgent data--
	pfd.revents = 0;
	//Lead is what the current pin is set to, ready is how many times this guy has been activated.
	int lead,lastLead,ready;
	lastLead=get_lead(fd);
	while (1) {
		ready=poll(&pfd,1,-1);
		if (pfd.revents != 0) {	//if an event happened and the pin is a 1, that means we got an interrupt from GAVR
	//		printf("Got int\n");
			lead=get_lead(fd);
			if (lead==1 && lastLead==0){
				char buffer[25];
				time_t timer;
				struct tm* tm_info;
				time(&timer);
				tm_info=localtime(&timer);
				strftime(buffer,25,"%Y:%m:%d%H:%M:%S",tm_info);
				printf("Calling halt at %s.\n",buffer);
				system("halt");
			} else {
	//			printf("Change, lead=%d, lastLead=%d\n",lead,lastLead);
			}
			lastLead=lead;
		}
	}
	return 0;
}	

