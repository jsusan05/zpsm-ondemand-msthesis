/**
 * Name: receiver_udp.c
 * Description: Test app that receives udp data
 * Author: 
 * Copyright:
 */

//SYSTEM INCLUDES
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <time.h>
#include <math.h>

//INTERNAL INCLUDES
#include "../../src/common/message.h"
#include "../../src/ap/serialsend.h"

//CONSTANTS
#define LISTEN_IP_ADDRESS 	"192.168.1.1"
#define SEND_IP_ADDRESS		"192.168.1.1"
#define IP_PORT				6789
#define AP_WI				40

//GLOBALS
static int dummy_socket_fd = -1;
static struct sockaddr_in address;
static timer_t wfTimerID;

/**
* Sends a dummy packet to a dummy port of the AP that would change radio from PS to awake mode
*/
int send_to_ap(char buf[])
{
	if (dummy_socket_fd==-1) //create socket to sent dummy pkt
	{					
		bzero(&address,sizeof(address));
		address.sin_family=AF_INET;
		address.sin_addr.s_addr=inet_addr(SEND_IP_ADDRESS);
		address.sin_port=htons(IP_PORT);
		dummy_socket_fd=socket(AF_INET, SOCK_DGRAM, 0);
	}
	if(buf)
		sendto(dummy_socket_fd,buf,sizeof(buf),0,(struct sockaddr *)&address,sizeof(address));
}

/**
* Cleans up the module
*/
static int cleanup()
{
	close(dummy_socket_fd);
}

static int counter = 0;
static int failed = 0;
/**
* Generates dummy wakeup frames
*/
static void wfTimerCB( int sig, siginfo_t *si, void *uc )
{
   zpsm_message_t message = {ZPSM_WAKEUP_FRAME,{5,3,2,8},4};
   printf("Counter=%d",++counter);
   if(counter!=5 && sendMsgToMote("/dev/ttyUSB0", message, 115200)!=0)
   { printf("failed=%d",++failed);}
}


/**
* Handler for all timers
*/
static void timerHandler( int sig, siginfo_t *si, void *uc )
{
    timer_t *tidp;
    tidp = si->si_value.sival_ptr;

    if ( *tidp == wfTimerID )
        wfTimerCB(sig, si, uc);   
}

/**
* Starts a timer
*/
static int makeTimer( char *name, timer_t *timerID, int expireMS, int intervalMS )
{
    struct sigevent         te;
    struct itimerspec       its;
    struct sigaction        sa;
    int                     sigNo = SIGRTMIN;

    /* Set up signal handler. */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timerHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sigNo, &sa, NULL) == -1)
    {
        fprintf(stderr, "Failed to setup signal handling for %s.\n", name);
        return(-1);
    }

    /* Set and enable alarm */
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = sigNo;
    te.sigev_value.sival_ptr = timerID;
    timer_create(CLOCK_REALTIME, &te, timerID);

    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = intervalMS * 1000000;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = expireMS * 1000000;
    timer_settime(*timerID, 0, &its, NULL);

    return(0);
}

/**
* Entry point
*/
int main()
{
    int i;
    int sin_len;
    char message[256]={0};
	char buf[4] = {0};
	int client_index = 2; //test data
	
    int socket_descriptor;
    struct sockaddr_in sin;
    printf("Waiting for data form sender \n");

    bzero(&sin,sizeof(sin));
    sin.sin_family=AF_INET;
    sin.sin_addr.s_addr=htonl(INADDR_ANY);
    sin.sin_port=htons(IP_PORT);
    sin_len=sizeof(sin);

    socket_descriptor=socket(AF_INET,SOCK_DGRAM,0);
    bind(socket_descriptor,(struct sockaddr *)&sin,sizeof(sin));
	
	makeTimer("WF Timer", &wfTimerID, AP_WI, AP_WI);

    while(1)
    {
        recvfrom(socket_descriptor,message,sizeof(message),0,(struct sockaddr *)&sin,&sin_len);
        switch(message[0])
        {
        	case ZPSM_REQUEST_INDEX:
        	{        
        		printf("Received ZPSM_REQUEST_INDEX\n");		
				buf[0] = ZPSM_AUTH_ID;
				buf[1] = client_index;
				send_to_ap(buf);
        	}
        	break;
			case ZPSM_ACK:
			{
        		printf("Received ZPSM_ACK-");	
				if(message[1]==client_index)
				{
					printf("Correct index assigned to client\n");
				}
				else
				{
					printf("Wrong index assigned to client\n");
				}				
			}
			break;
			case ZPSM_DISCONNECT:
			{
				printf("Received ZPSM_DISCONNECT\n");	
			}
			break;
			case ZPSM_IN_RANGE:
			{
				printf("Received ZPSM_IN_RANGE\n");	
			}
			break;
			case ZPSM_OUT_RANGE:
			{
				printf("Received ZPSM_OUT_RANGE\n");	
			}
			break;
			default:
			break;
        }                    	
    }
    close(socket_descriptor);
}

