/**
 * Name: zpsm_client_mod.c
 * Description: Module that listens for the index form AP through IP layer and schedules a listen interval timer
 * Author: 
 * Copyright:
 */

#undef __KERNEL__
#define __KERNEL__
 
#undef MODULE
#define MODULE

//SYSTEM INCLUDES
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netlink.h>
#include <linux/string.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/timer.h>

//INTERNAL INCLUDES
#include "zpsm_send_mod.h"
#include "zpsm_defines.h"
#include "../common/message.h"
#include "../common/zpsm_kern_debug.h"

//CONSTANTS
//The payload size is in the order of 1KB
#define MAX_PAYLOAD 			1024
#define SKB_NETWORK_HDR_IPH(skb) ((struct iphdr *)skb_network_header(skb))

//GLOBAL VARIABLES
//netlink communication socket
static struct sock *nl_sock = NULL; 
//pid value for netlink communication
static int pid;
//Timer that sends the wakeup frames to client
static struct timer_list channel_timer; //timers
//Timer that sends the heartbeat every listen interval
static struct timer_list heartbeat_timer;
//The authentication id for the client
static int client_auth_id = 0;
//The status of the client IN-RANGE or OUT-RANGE
static int zpsm_client_status = ZPSM_OUT_RANGE; //need lock
//The listen interval initialized to a default value
static int LI = DEFAULT_HEARTBEAT;//need lock
//Flag true if the netfilter module is loaded else 0
static int nfmod_loaded = 0;
//Flag 1 if timer is on else 0
static int timeron = 0;

MODULE_LICENSE("GPL");

/**
 * Send message to the user space app
 * @param  message The message to send to userspace through netlink
 */
void nlsendmsg(const char *message)
{
	struct nlmsghdr *nlh;
	struct sk_buff *nl_skb;
	
	nl_skb = alloc_skb(NLMSG_SPACE(MAX_PAYLOAD),GFP_ATOMIC);
	if(nl_skb==NULL)
	{
		printk(KERN_INFO "alloc skb failed\n");
		return;
	}
	
	nlh = nlmsg_put(nl_skb, 0, 0, 0, NLMSG_SPACE(MAX_PAYLOAD) - sizeof(struct nlmsghdr), 0);
	NETLINK_CB(nl_skb).pid = 0;
	memcpy(NLMSG_DATA(nlh),message,MAX_NETLINK_MSG_SIZE);
	netlink_unicast(nl_sock, nl_skb, pid, MSG_DONTWAIT);
}

/**
* Hook to capture packet with header ZPSM_AUTH_INDEX from AP
* @param hooknum Netfilter hook number denotes the hook point
* @param skb pointer to socket buffer
* @param in the netdevice to read incoming packets like wlan0
* @param out the netdevice to send outgoing packets like wlan0
* @param okfn Function handler, not used
*/
static unsigned int zpsm_hook(unsigned int hooknum,
                   struct sk_buff *skb,
                   const struct net_device *in,
                   const struct net_device *out,
                   int (*okfn) (struct sk_buff *))
{
    struct iphdr *iph = SKB_NETWORK_HDR_IPH(skb);
    struct udphdr *udph;
    char * message = skb->data;
	int controlMessage = message[28];//get data payload from sk_buff
	char msgtoSend[3] = {0};
    printk(KERN_INFO "In zpsm_hook");	
    udph = (struct udphdr *)((char *)iph + (iph->ihl << 2));
    //When a packet is received and before any routing decision is made, handle the packet
    if (hooknum == NF_INET_PRE_ROUTING)
    {        
        //If the source port or destination port is 6789, then it is a control message from AP and this needs
        // to be handled
        if (ntohs(udph->dest) == 6789 ||
            ntohs(udph->source) == 6789) 
        {
        	printk(KERN_INFO "Rx a control message\n");
            //Handle the authentication id send from the AP and store it and send it to
            // client application through netlink socket
            if(ZPSM_AUTH_ID==controlMessage)
            {
            //lock
                client_auth_id = message[29];
                msgtoSend[0] = UPDATE_AUTH_ID;
                msgtoSend[1] = client_auth_id;
                ZPSM_LOG("Received index %d",client_auth_id);
			 	nlsendmsg(msgtoSend);
            //unlock
                return NF_DROP;
            }
        }
    }
    return NF_ACCEPT;

}

/**
* Structure to setup the netfilter
**/
static struct nf_hook_ops zpsm_ops[] = {
    {
     .hook = zpsm_hook,

     .pf = PF_INET,
     .hooknum = NF_INET_PRE_ROUTING,
     .priority = NF_IP_PRI_FIRST,
     },
    {
     .hook = zpsm_hook,

     .pf = PF_INET,
     .hooknum = NF_INET_LOCAL_OUT,
     .priority = NF_IP_PRI_FILTER,
     },
};

/**
* Channel timer callback
* Every CHANNEL_TIMEOUT if the client has a valid id, then request the application side to
* calculate the channel quality, whether it is in-range or out-range and also the listen
* interval by sending a netlink request
* @param data Not used
*/
void channel_timer_callback( unsigned long data )
{
    char msgtoSend[2];
    ZPSM_LOG("Calling channel timer callback...\n");    
    mod_timer( &channel_timer, jiffies + msecs_to_jiffies(CHANNEL_TIMEOUT));    
    if(client_auth_id)
    {
	    msgtoSend[0] = GET_STATE_AND_LI;
 		nlsendmsg(msgtoSend);
 	}
}

/**
* Inrange timer callback
* Every LI, the client sends a heartbeat to the AP to notify it if it is IN zigbee range
* or not
* @param data Not used
*/
static int ack_num = 0;
void heartbeat_timer_callback( unsigned long data )
{
    unsigned char buf[2] = {0};
   
   	//lock
    mod_timer( &heartbeat_timer, jiffies + msecs_to_jiffies(LI));   
    buf[0] = zpsm_client_status; 
    sendtoAP(buf);
	ack_num++;
	ZPSM_LOG("ACK: num=%d",ack_num);
    if(zpsm_client_status==6)
	    ZPSM_LOG("Sending range status IN_RANGE LI=%d",LI);
	else if(zpsm_client_status==7)
	    ZPSM_LOG("Sending range status OUT_RANGE LI=%d",LI);
	else 
	    ZPSM_LOG("Sending range status UNKNOWN_RANGE LI=%d",LI);
    //unlock
}

/**
 * Callback issued when netlink socket is created and
 * kernel receives message from application
 * @param skb The netlink data
 */
void nl_data_ready(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	int len;
	char msgtoSend[32] = {0};
	char msgbuf[100] = {0};
	
	nlh = nlmsg_hdr(skb);
	len = skb->len;
	
	if(NLMSG_OK(nlh,len)) //Get message from user space just to know it is alive
	{
		memcpy(msgbuf, NLMSG_DATA(nlh), strlen(NLMSG_DATA(nlh)));
		printk(KERN_INFO "Got msg from uspace = %d",msgbuf[0]);
		
        //The kernel receives message from application saying application has started
		if(USER_APP_LOADED_MSG==msgbuf[0])
		{
			//Create a netfilter hook
			if(!nfmod_loaded)
			{
				printk(KERN_INFO "Serial port ready netfilter loading");
				nf_register_hook(&zpsm_ops[0]);
				nf_register_hook(&zpsm_ops[1]);
				nfmod_loaded = 1;
			}
			//Start channel timer and listen timer
			if(!timeron)
			{
				printk(KERN_INFO "Starting timer");
				init_timer(&channel_timer);
		        setup_timer( &channel_timer, channel_timer_callback, 0 );			
		        add_timer(&channel_timer);                
		        
		        init_timer(&heartbeat_timer);
		        setup_timer( &heartbeat_timer, heartbeat_timer_callback, 0 );			
		        add_timer(&heartbeat_timer);
		        timeron = 1;
		    }
		    msgtoSend[0]= KERNEL_MOD_ACK;
		}	
        // The kernel receives message from application saying LI and state has been updated
        // The kernel module cannot handle floating point operations hence the calculation is
        // done at the application level
		else if(UPDATE_STATE_AND_LI==msgbuf[0])
		{
			zpsm_client_status = msgbuf[1];
			LI = msgbuf[2];
			if(DEFAULT_HEARTBEAT_ID==LI)
				LI = DEFAULT_HEARTBEAT; //this is to avoid the overflow in pass thru netlink socket
			ZPSM_LOG("Updated LI timer in kernel=%d zpsm_client_status=%d\n",LI,zpsm_client_status);	
			if(timeron)		//lock
			{
				mod_timer( &heartbeat_timer, jiffies + msecs_to_jiffies(LI));  
			} //unlock
		}
		else
		{
			msgtoSend[0]= KERNEL_UNHANDLED_MSG;
		}
		pid = nlh->nlmsg_pid;
		nlsendmsg(msgtoSend);
	}
}

/**
 * Entry module
 */
static int __init zpsmclient_init(void)
{
	printk(KERN_INFO "enter ZPSM module\n");
    
	//create a netlink socket
	nl_sock = netlink_kernel_create(&init_net, NETLINK_CLIENT_ID, 0, 
			nl_data_ready, NULL, THIS_MODULE);		
	  
	if(!nl_sock)
	{
		printk(KERN_INFO "null socket\n");
		return -ENOMEM;
	}
	init_send();
	
	return 0;
}

/**
 * Exit module
 */
static void __exit zpsmclient_exit(void)
{	    
    char buf[2] = {0};
    
    printk(KERN_INFO "exit ZPSM module\n");
    
    //send disassociation message to AP	
    buf[0] = ZPSM_DISCONNECT; 
    sendtoAP(buf);
	
	//remove timers
	if(timeron)
	{
		del_timer( &channel_timer );
		del_timer( &heartbeat_timer );
	}
	
	//remove socket used to send to AP
	exit_send();	
	
	//unregister netlink hook
	if(nfmod_loaded)
	{
		nf_unregister_hook(&zpsm_ops[1]);
		nf_unregister_hook(&zpsm_ops[0]);
	}
		
	//close the netlink socket
	if(NULL!=nl_sock)
	{
		sock_release(nl_sock->sk_socket);
	}	

}

module_init(zpsmclient_init);
module_exit(zpsmclient_exit);

