/**
 * Name: zpsm_client.c
 * Description: Application that receives wakeup frames and manages the delay bound
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
#include <arpa/inet.h>

//INTERNAL INCLUDES
#include "zpsm_defines.h"
#include "seriallisten.h"
#include "../common/message.h"
#include "../common/zpsm_debug.h"

//CONSTANTS
#define MAX_PAYLOAD 			1024
#define TELOSB_BAUDRATE 		115200
//The wakeup frame interval in ms as integer
#define AP_WI				    40
//The listen interval is updated only if the difference between new listen interval  
// calculated and previous one reaches this threshhold in ms	 
#define LI_THRESHHOLD			20
//The wakeup frame interval in ms as float	 
#define AP_WI_F				    40.00	
//The period during which the channel quality is calculated 
#define CHANNEL_TIMEOUT_F		200.00	 
			
//GLOBAL VARIABLES
//The netlink source address object to receive messages through the netlink socket
//The netlink destination object to send messages through the netlink socket
struct sockaddr_nl nl_src_addr, nl_dest_addr;
//The netlink message header object
struct nlmsghdr *nlh = NULL;
//The structure that describes the netlink buffer
struct iovec iov;
//The netlink socket descriptor
int nl_fd;
//The message structure that contains the message read/send through netlink socket
struct msghdr nl_msg;
//Buffer that receives messages from netlink socket
char recvbuf[1024]={0};
//The device filename for the mote
char device_filename[256] = {0};
//The baud rate of the mote
int baud_rate = TELOSB_BAUDRATE; 
//Counter that keeps tract of the received wakeup frames which is reset after ever CHANNEL_TIMEOUT period
static int WF_rx_counter = 0;
//Total wakeup frames received
static int tot_WF = 0;
//Total number of acknowledgements send if the client id is found in wakeup frame which is a way of turning radio on
static int ack_num = 0;
//The status of client in-zigbee range or out-of zigbee range
static int zpsm_client_status = ZPSM_OUT_RANGE;
//The value of the zigbee client id or authentication id recived from AP
static int zpsm_auth_id = 0;
//Socket descriptor to send packets to AP
static int dummy_socket_fd = -1;
//mutex that guards wf_rx_counter and client status
static pthread_mutex_t wf_mutex; 
//mutex to guard client auth_id
static pthread_mutex_t index_mutex; 
//AP address
static struct sockaddr_in address;	
//delay_bound that the client demands
static int D = 100; 

/**
* Sends a dummy packet to a dummy port of the AP that would change radio from PS to awake mode
* @param buf The bytes to be send
* @return 0 if success else error
*/
int send_to_ap(char buf[])
{
	if (dummy_socket_fd==-1) //create socket to sent dummy pkt
	{					
		bzero(&address,sizeof(address));
		address.sin_family=AF_INET;
		address.sin_addr.s_addr=inet_addr(AP_IP_ADDRESS);
		address.sin_port=htons(AP_PORT);
		dummy_socket_fd=socket(AF_INET, SOCK_DGRAM, 0);
	}
	else
	{
		if(buf)
			sendto(dummy_socket_fd,buf,sizeof(buf),0,(struct sockaddr *)&address,sizeof(address));
	}
}

/**
 * Send message through netlink socket
 * @param message The message to send to the netlink socket to the kernel 
 */
void sendnlmsg(const char *message)
{
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD)); 
	nlh ->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;
	
	memcpy(NLMSG_DATA(nlh),message, MAX_NETLINK_MSG_SIZE);
	
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
 * @param message The message to receive from the netlink socket from the kernel 
 */
void recvnlmsg(char *message)
{
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	recvmsg(nl_fd,&nl_msg,0);
	memcpy(message, NLMSG_DATA(nlh), MAX_NETLINK_MSG_SIZE);
}

/**
 * Create socket and listen
 */
void init_nl()
{
	char msg[10] = {0};
	nl_fd = socket(AF_NETLINK,SOCK_RAW, NETLINK_CLIENT_ID);
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
			
	//App is ready for serial communication
	//Send message to kernel module that 
	msg[0] = USER_APP_LOADED_MSG;
	sendnlmsg(msg);
	//Receive message from kernel saying it is alive
	recvnlmsg(recvbuf);
	printf("received from kspace=%d\n",recvbuf[0]);
	if(KERNEL_MOD_ACK!=recvbuf[0])
	{
		fprintf(stderr, "zpsm kernel module is not loaded");
		exit(0);
	}	
	printf("zpsm client kernel mod is loaded.\n");	
}


/**
* Netlink listen thread function
* @param Argument passed into the thread function
* @return None
*/
void *rxNetlinkThread(void *arg)
{
	float P = 0.0;
	int LI = 0;
	int DR = 0;
	int prevLI = 0;
	char msg[4] = {0};
	while(1)
	{
		recvnlmsg(recvbuf);		
		switch(recvbuf[0])
		{
			case UPDATE_AUTH_ID: //Update index in userspace
			{				
                pthread_mutex_lock (&index_mutex);
                zpsm_auth_id = recvbuf[1];
                pthread_mutex_unlock (&index_mutex);                			
			}
				break;
			case GET_STATE_AND_LI: //send state and LI to kernel
			{
				//calculate the listen interval							
				pthread_mutex_lock (&wf_mutex); //lock							    
				P = (WF_rx_counter*AP_WI_F)/CHANNEL_TIMEOUT_F;				
				printf("Channel quality=(%d*%0.0f)/%0.0f = %0.2f\n", WF_rx_counter, AP_WI_F, CHANNEL_TIMEOUT_F, P);	
				prevLI = LI;
                tot_WF += WF_rx_counter; 
				WF_rx_counter = 0;//reset the counter
				if(P>0.0 && P<1.0) //then IN_RANGE but there is a packet loss
				{
					//update LI = D*WI/(WI-PD)
					DR = (AP_WI-(P*D));
					if( DR>0 )
						LI = (D*AP_WI)/DR;
					else
						LI = DEFAULT_HEARTBEAT_ID;
					printf("LI=%d\n",LI);       
					zpsm_client_status = ZPSM_IN_RANGE;	
				}
				else if(P>=1.0) //if channel quality is optimal, we need to make known to AP that client is alive
				{
					zpsm_client_status = ZPSM_IN_RANGE;
					LI = DEFAULT_HEARTBEAT_ID;
				}
				else //out of zigbee range but in wifi range
				{
					zpsm_client_status = ZPSM_OUT_RANGE;
					LI = DEFAULT_HEARTBEAT_ID;					
				}    
				
				if( (LI-prevLI<0) || (LI-prevLI>LI_THRESHHOLD) )
				{
					printf("======>Updating LI=%d status=%d\n",LI,zpsm_client_status);
					msg[0] = UPDATE_STATE_AND_LI;	
					msg[1] = zpsm_client_status;	
					msg[2] = LI;									
					sendnlmsg(msg);		
				}
				pthread_mutex_unlock (&wf_mutex); //end lock
			}
				break;
			default:
				break;
		}
	}
}

/**
* Handles the incoming zigbee packets
* @param received_msg The received packet from the mote that has header and data
* @return 0 on success else error
*/
static int handleZpsmFrames(zpsm_message_t received_msg)
{
	int i=0;
    
	switch(received_msg.header_flag)
	{
		case ZPSM_WAKEUP_FRAME:
		{			
			unsigned int* item = NULL;
			pthread_t rxThread = 0;
   		    unsigned char buf[2] = {0,1};
   		    int i = 0;
            int found = 0;
			//The wakeup frame counter is incremented to denote it is in zigbee range
			pthread_mutex_lock (&wf_mutex);
			WF_rx_counter++;
			zpsm_client_status = ZPSM_IN_RANGE;	
			pthread_mutex_unlock (&wf_mutex);

			pthread_mutex_lock (&index_mutex);
    		if(!zpsm_auth_id) 			//If index is null, then request AP for index
    		{
                printf("Sending request ID\n");
                buf[0] = ZPSM_REQUEST_INDEX;
              	send_to_ap(buf);    		    
    		}
    		else //Check if client index is in wakeup frame and turn radio on by sending an ACK to AP
    		{        
			    found = 0;        
                for(i=0;i<MAX_CLIENTS;++i)
                {
                    if(received_msg.id_list[i]==zpsm_auth_id)
                    { found = 1; break;}
                }
	        	
                if(found)
			    {			
				    //start timer for out of range and once it times out then remove auth id and send ZPSM_DISASSOC message to AP
                    buf[0] = ZPSM_ACK;
                    buf[1] = zpsm_auth_id;
                	send_to_ap(buf);
			        ack_num++;                    
			    }	
            struct timeval  tv={0};
            gettimeofday(&tv, NULL);
                        
            FILE* fd = fopen("client_log", "a+" );
            fprintf(fd, "%ld:%ld::WF_counter=%d ACK_counter=%d\n", tv.tv_sec, tv.tv_usec, tot_WF, ack_num);    
            fclose(fd);
			}
			pthread_mutex_unlock (&index_mutex);
		}
			break;		
		default:
			break;
	}
}

 

/**
 * Entry point
 */

int main(int argc, char **argv)
{	
	int total = 0;
	int err = 0;
	int len, i;
	unsigned char *packet = NULL; 
	unsigned char msg_bits = 0;
	int index = 0;
	int id_counter = 1;
	zpsm_message_t received_msg = {0};
    char message[9] = {0};
	pthread_t rxThread = 0;
	if (argc < 4)
	{
		fprintf(stderr, "Usage: %s <device> <rate> <delay_bound>- send a raw packet to a serial port\n", argv[0]);
		exit(2);
	}
	//Initialize the netlink transmission
	init_nl();
    //Netlink messages are received in a separate thread
    pthread_create(&rxThread, NULL, rxNetlinkThread, NULL);
    
    pthread_mutex_init(&wf_mutex, NULL);
    pthread_mutex_init(&index_mutex, NULL);
    
	strcpy(device_filename, argv[1]); //mote device filename
	baud_rate = atoi(argv[2]); //baud rate
    D = atoi(argv[3]); //delay bound
	serial_source src = open_mote(device_filename, baud_rate);
    total = 0;
	for (;;)
	{	
	    //keep listening for packets in serial port
		packet = read_serial_packet(src, &len);	
		if (!packet)
		{
			exit(0);
		}		

		if(len>2) //The packet has two bytes
		{
			received_msg.header_flag=packet[1]; //Packet header-only wakeup frame header is supported
			msg_bits = packet[2];		        //Packet message-which has list of authentication id
			index = 0;
			id_counter = 1;
			i = 0;

			while(msg_bits!=0)
			{
				if(msg_bits&0x01==0x01) { received_msg.id_list[index++]=id_counter; }
				id_counter++; msg_bits=msg_bits>>1;						
			}
			total++;
            printf("Received WF:%d\n",total);
			received_msg.num_clients = index;
		
			message[0]=received_msg.header_flag;
			for (i = 0; i < received_msg.num_clients; i++)
				{ message[i+1]=received_msg.id_list[i];printf("%d ", received_msg.id_list[i]); }
		}	
		putchar('\n'); //Do not remove
		free((void *)packet);		

		handleZpsmFrames(received_msg);
	}

	return 0;
}



