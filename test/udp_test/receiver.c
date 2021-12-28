/**
 * Name: receiver_udp.c
 * Description: Test app that receives udp data
 *              Usage: udp_receiver <port> <delay bound in ms>
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

//CONSTANTS

//GLOBALS
static int dummy_socket_fd = -1;
static struct sockaddr_in address;
static timer_t wfTimerID;

/**
* Returns time difference in millisecs
*/
static long timeval_diff(struct timeval* time1, struct timeval* time2)
{
    long msec;
    msec = (time2->tv_sec - time1->tv_sec)*1000;
    msec+= (time2->tv_usec - time1->tv_usec)/1000;
    return msec;
}

/**
* Entry point
*/
//#define IPPORT 6799
int main(int argc, char **argv)
{
    int counter_rx=0;
    int sin_len;
    char message[1024]={0};
    long int tot_delay = 0;
    struct timeval time_in_pkt = {0};    
    long time_diff = 0;
	struct timeval  curr_time = {0};
    int socket_descriptor;
    struct sockaddr_in sin;
    int pkt_intime = 0;
    int port = 0;
    int delay_bound = 0;
    int delta = 0;
    if(argc<3)
    {
    	printf("Usage: udp_receiver <port> <delay bound in ms>\n");
    	exit(0);
    }
    port = atoi(argv[1]);
    delay_bound = atoi(argv[2]);
    
    printf("Waiting for data form sender \n");

    bzero(&sin,sizeof(sin));
    sin.sin_family=AF_INET;
    sin.sin_addr.s_addr=htonl(INADDR_ANY);
    sin.sin_port=htons(port);
    sin_len=sizeof(sin);

    socket_descriptor=socket(AF_INET,SOCK_DGRAM,0);
    bind(socket_descriptor,(struct sockaddr*) &sin, sizeof(sin));
	//receives the packet that this server is listening to
    while(1)
    {
        recvfrom(socket_descriptor,message,sizeof(message),0,(struct sockaddr *)&sin, &sin_len);
        char* pch;
        int seq_id = 0;       
        
        pch = strtok(message,",");
        if(pch!=NULL)
        {
            time_in_pkt.tv_sec = atol(pch);
            pch = strtok(NULL,",");
        }
        if(pch!=NULL)
        {
            time_in_pkt.tv_usec = atol(pch);
            pch = strtok(NULL,",");
        }
        if(pch!=NULL)
        {
            seq_id = atol(pch);
        }
        
        gettimeofday(&curr_time, NULL);
              
        time_diff = timeval_diff( &time_in_pkt, &curr_time );
        //The first packet received is used to synchronized the time between the sender and receiver
        //Packet with seq_id=0 is the synchronization packet
        if(seq_id==0) delta = time_diff;
        //The actual packets start from sequence id >=1
	    if(seq_id!=0) counter_rx++;
       
        long int delay = time_diff - delta;
        if(seq_id!=0 && delay<delay_bound)
        {
        	++pkt_intime;
        }
	    tot_delay+=delay;
	    long avg_delay = 0;
	    if(counter_rx>0) avg_delay = tot_delay/counter_rx;

        //Display message for each packet received:
        //"<current time in secs:current time in usecs>::<sequence id of current packet>
        //<Delay of current packet><total packets received so far><total packets that meet delay requirement so far>
        //<difference between sender and receiver system clock for time synchronization>
        //<average delay of packets received so far>"
        printf("%ld:%ld::SeqID=%d Delay(in ms)=%ld Total rx=%d, total arrived on time=%d  Delta=%d Avg_delay=%ld\n",            
                curr_time.tv_sec, curr_time.tv_usec, seq_id, delay, counter_rx, pkt_intime, delta, avg_delay);
    }    
    close(socket_descriptor);
}

