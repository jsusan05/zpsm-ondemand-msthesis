/**
 * Name: zpsm_app.c
 * Description: Application that receives signal from kernel module that intercepts
 * 				packets and sends a message through serial port
 * Author: 
 * Copyright:
 */

//SYSTEM INCLUDES
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

//INTERNAL INCLUDES
#include "zpsm_defines.h"
#include "../common/message.h"

//CONSTANTS
//Netlink identifier
#define NETLINK_TEST 30
//Payload of 1 KB
#define MAX_PAYLOAD 1024
//Baud rate for telosb mote
#define TELOSB_BAUDRATE 115200
//Target channel quality for test, a value between 0 to 1. Change this to change the channel quality.
#define CHANNEL_TARGET 1 


//GLOBAL VARIABLES
//Socket address for source and destination
struct sockaddr_nl nl_src_addr,nl_dest_addr;
//The netlink message header object
struct nlmsghdr *nlh = NULL;
//The structure that describes the netlink buffer
struct iovec iov;
//Netlink socket descriptor
int nl_fd;
//Netlink message structure
struct msghdr nl_msg;
//Receive buffer for netlink data
char recvbuf[1024];
//The mote device filenmae
char device_filename[256] = {0};
//Set default baud rate of device to one for telosb
int baud_rate = TELOSB_BAUDRATE; 
//Number of packets received from mote
int count = 0;
//current time
char* timestamp = NULL;
//Structure for time value
struct timeval tv;

/**
* Get current timestamp
* @return timestamp
**/
long int get_timestamp()
{
	gettimeofday(&tv, NULL);
	return tv.tv_usec;
}

/**
 * Send message through netlink socket
 * @param message Message to send
 */
void sendnlmsg(const char *message)
{
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD)); 
	nlh ->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;
	
	strcpy((char *)NLMSG_DATA(nlh),message);
	
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	nl_msg.msg_name = (void *)&nl_dest_addr;
	nl_msg.msg_namelen = sizeof(nl_dest_addr);
	nl_msg.msg_iov = &iov;
	nl_msg.msg_iovlen=1;
	
	sendmsg(nl_fd,&nl_msg,0);
}

/**
 * Receive message through the socket
 * @message message Incoming message through netlink socket
 */
void recvnlmsg(char *message)
{
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	recvmsg(nl_fd,&nl_msg,0);
	strcpy(message, NLMSG_DATA(nlh));
}

/**
 * Handle requests from kernel
 * @param msg Message received from zigbee device
 */
int handleReceivedMsg(const char* msg)
{

	int err = 0;
	int i = 0;
	int res = 0;
	int msglen = 0;
	unsigned int temp = 0;

	printf("Message from kernel:%s",msg);
	fprintf(stderr, "message ID %d\n",count);
	count++;

	if (strstr(recvbuf,",") != NULL)
	{
		res = recvbuf[1];
		//send something through serial port		
		zpsm_message_t message = {ZPSM_WAKEUP_FRAME,{8},1}; //8 should not be used as index as zb expects some data other than 0
		message.header_flag = ZPSM_WAKEUP_FRAME;
		fprintf(stderr, "message List: ");
		for (i = 1;i<=8;i++)
		{
			if (res & (1<<i))
			{
				message.id_list[msglen] = i;
				msglen++;
				fprintf(stderr, "%d ", i);
			}
		}
		fprintf(stderr, "\n");
		fprintf(stderr,"msglen length: %d\n\n",msglen);
        if(msglen!=0)
    		message.num_clients = msglen;

        //Drop frame if the random value is not in range of channel quality that we want
        double r = ((double) rand()/(RAND_MAX));
        printf("random generated:%f", r);
        if(r<CHANNEL_TARGET)
		    sendMsgToMote(device_filename, message, baud_rate);
	}
	
}

/**
 * Create socket and listen
 */
void init_nl()
{
	nl_fd = socket(AF_NETLINK,SOCK_RAW,NETLINK_TEST);
	memset(&nl_msg,0,sizeof(nl_msg));
	
	memset(&nl_src_addr,0,sizeof(nl_src_addr));
	nl_src_addr.nl_family = AF_NETLINK;
	nl_src_addr.nl_pid = getpid();
	nl_src_addr.nl_groups = 0;
	
	bind(nl_fd,(struct sockaddr*)&nl_src_addr, sizeof(nl_src_addr));
	
	memset(&nl_dest_addr,0,sizeof(nl_src_addr));
	nl_dest_addr.nl_family = AF_NETLINK;
	nl_dest_addr.nl_pid = 0;
	nl_dest_addr.nl_groups = 0;
	
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	
	//Send message to kernel module that 
	sendnlmsg(USER_APP_LOADED_MSG);
	
	//Receive message from kernel saying it is alive
	recvnlmsg(recvbuf);
	//printf("%s",recvbuf);
	if(0!=strcmp(KERNEL_MOD_ACK, recvbuf))
	{
		fprintf(stderr, "zpsm kernel module is not loaded");
		exit(0);
	}	
	
	//App is ready for serial communication
	sendnlmsg(SERIAL_READY_MSG);
	
	while(1)
	{
		recvnlmsg(recvbuf);
		printf("Current time: %ld ms\n", get_timestamp());
		handleReceivedMsg(recvbuf);
	}
}

/**
 * Entry point
 */
int main(int argc, char **argv)
{	
	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s <device> <rate> - send a raw packet to a serial port\n", argv[0]);
		exit(2);
	}
	
	strcpy(device_filename, argv[1]); //The device filename for mote
	baud_rate = atoi(argv[2]); //The mote baud rate
		
	init_nl();
	return 0;
}



