/**
 * Name: zpsm_kernmod.c
 * Description: Module that listens for packets in the IP layer and signals 
 * user application to take care of it
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
#include <net/sock.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

#include <linux/proc_fs.h>
#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <net/route.h>
#include <linux/timer.h>

//System lock
#include <linux/spinlock.h>
#include <linux/list.h>


//INTERNAL INCLUDES
#include "zpsm_defines.h"
#include "../common/message.h"
#include "zpsm_queue_mod.h"
#include "zpsm_send_mod.h"
#include "../common/zpsm_kern_debug.h"

//CONSTANTS
//Netlink socket identifier
#define NETLINK_ZPSM 30
//MAX_PAYLOAD represents the max size of packet payload  
#define MAX_PAYLOAD 1024
//Get UDP header from sk_buff
#define SKB_NETWORK_HDR_IPH(skb) ((struct iphdr *)skb_network_header(skb))


//GLOBAL VARIABLES
//nl_sock represents the netlink socket
static struct sock *nl_sock = NULL;
//pid repersents the applicantion's process ID
static int pid;
//msg repersents the massage bettween the application and the kernel
static struct msghdr msg;
//The content of control massage
static int msgIndexInt = 0;
//A flag that repersents the creation of netlink
static int testFlag = 0;

//new timer
//Time's data structure
static struct timer_list my_timer;
//The total number of wakeup frames which have been send out
int total = 0;
//The result of timeer setup
int ret;


/**
 * Send message to the user space app
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
	strcpy(NLMSG_DATA(nlh),message);
	netlink_unicast(nl_sock, nl_skb, pid, MSG_DONTWAIT);
}

/**
 * Delete the index from wakeup frame
 */
static void deleteIndexInt(int index)
{
	if (index > 0 && index < 8) 
		msgIndexInt = msgIndexInt & (0<<index);
}

/**
 * Add the index to wakeup frame
 */
static void updateIndexInt(int index)
{
	if (index > 0 && index < 8)
		msgIndexInt = msgIndexInt | (1<<index);
}


//Handle control message from port 6789;
/**
 * This function handles the control massage from prot 6789
 *
 * @param sk_buff *skb the packet struct
 * @param index the ZigBee index for ZPSM station, it is cosponding to packet's IP address
 */
static void handleControlMessage(struct sk_buff *skb ,int index)
{
	int i = 0;
	struct iphdr *iph = SKB_NETWORK_HDR_IPH(skb);
	//handle control message here
	char * message = skb->data;
	int controlMessage = message[28];//get data payload from sk_buff
	char saddr[254] = {0};

	unsigned long timeout = -1;

	sprintf(saddr, "%d.%d.%d.%d",
            0x0ff & iph->saddr,
            0x0ff & (iph->saddr >> 8),
            0x0ff & (iph->saddr >> 16), 0x0ff & (iph->saddr >> 24));

	switch(controlMessage)
	{
        //If the station is in AP's ZigBee range, the AP shall update the active time for this station
		case ZPSM_IN_RANGE:
		{
			ZPSM_LOG("Recieved IN RANGE from %s index %d\n",saddr, index);
			printk("Recieved IN RANGE from %s index %d\n",saddr, index);			
			ZPSM_queue_set_verdict(ZPSM_QUEUE_UPDATE_STATE, iph->saddr, 0);// set this node in zigbee range
		}
		break;

        //If the AP receive ZPSM_ACK, it shall 
        //  1) delete the index from wakeup frame
        //  2) update the membership for this station
        //  3) if this station is not in the ZigBee membership, add it.
		case ZPSM_ACK:
		{
			ZPSM_LOG("Recieved ZPSM ACK from %s index %d\n",saddr, index);
            printk("Recieved ZPSM ACK from %s index %d\n",saddr, index);
            deleteIndexInt(index);//delete index from wakeup frame
			if (index == message[29])
			{
				timeout = ZPSM_queue_find_timeout(iph->saddr);
				ZPSM_queue_set_verdict(ZPSM_QUEUE_UPDATE_STATE, iph->saddr, 0);// set this node in zigbee range
			}
			else
			{
				ZPSM_LOG("Send Uptade index to client %s index %d\n",saddr, index);
				printk("Send Uptade index to client %s index %d\n",saddr, index);
				ZPSM_queue_set_verdict(ZPSM_QUEUE_UPDATE_STATE, iph->saddr, 0);// set this node in zigbee range
				//Handle request index
				ZPSM_add_request_index(index, iph->saddr);
			}

		}
	    break;

        //update the station's status to out of ZigBee range
		case ZPSM_OUT_RANGE:
		{
			ZPSM_LOG("Recieved ZPSM OUT RANGE from %s index %d\n",saddr, index);
			printk("Recieved ZPSM OUT RANGE from %s index %d\n",saddr, index);
			ZPSM_queue_set_verdict(ZPSM_QUEUE_UPDATE_STATE, iph->saddr, 1);// set this node out of zigbee range
		}
		break;

        //Add the station to ZigBee index request queue
		case ZPSM_REQUEST_INDEX:
		{
			ZPSM_LOG("Recieved ZPSM REQUEST INDEX from %s index %d\n",saddr, index);
			printk("Recieved ZPSM REQUEST INDEX from %s index %d\n",saddr, index);
			ZPSM_queue_set_verdict(ZPSM_QUEUE_UPDATE_STATE, iph->saddr, 0);// set this node in zigbee range
			//Handle request index
			ZPSM_add_request_index(index, iph->saddr);

		}
		break;

        //Remove the station from ZigBee membership table
		case ZPSM_DISCONNECT:
		{
			ZPSM_LOG("Recieved ZPSM DISCONNECT from %s index %d\n",saddr, index);
			printk("Recieved ZPSM DISCONNECT from %s index %d\n",saddr, index);
			ZPSM_dequeue(iph->saddr);//delete this node from zigbee membership table
		}
		break;

		default:
		{
			ZPSM_LOG("Invailed control message %d\n",controlMessage);
			ZPSM_LOG("The control message is %s\n",message);
			printk("Invailed control message %d\n",controlMessage);
			printk("The control message is %s\n",message);
		}
		break;
	}
}

/**
 * The callback function of netfliter. This function check the packet's IP header, and only care about UPD packet.
 * 
 * @param int hooknum the index of netfliter's hook point
 * @parma struct sk_buff *skb the packet's structure
 * @parma struct net_device *in the device which packet is came from
 * @parma struct net_device *out the device which packet will send to
 * @parma  int (*okfn) (struct sk_buff *) the next function which the packet can be send to
*/
static unsigned int zpsm_hook(unsigned int hooknum,
                   struct sk_buff *skb,
                   const struct net_device *in,
                   const struct net_device *out,
                   int (*okfn) (struct sk_buff *))
{
    struct iphdr *iph = SKB_NETWORK_HDR_IPH(skb);
    struct in_addr ifaddr, bcaddr;
    struct udphdr *udph;
    int index = 0;

    int testIndex = 0;

    char daddr[255] = {0};
    char saddr[255] = {0};

    unsigned long timeout = 0;

    memset(&ifaddr, 0, sizeof(struct in_addr));
    memset(&bcaddr, 0, sizeof(struct in_addr));

    if ((!iph) || (iph->protocol != IPPROTO_UDP))
	return NF_ACCEPT;

    if (iph->daddr == INADDR_BROADCAST || IN_MULTICAST(ntohl(iph->daddr))||iph->daddr == bcaddr.s_addr)
	return NF_ACCEPT;

    udph = (struct udphdr *)((char *)iph + (iph->ihl << 2));

    if (hooknum == NF_INET_LOCAL_OUT)
    {
        if (ntohs(udph->dest) == 6789 ||
                ntohs(udph->source) == 6789) 
        {
            printk("Send from port 6789 \n");
            return NF_ACCEPT;
        }

	    sprintf(daddr, "%d.%d.%d.%d",
                0x0ff & iph->daddr,
                0x0ff & (iph->daddr >> 8),
                0x0ff & (iph->daddr >> 16), 0x0ff & (iph->daddr >> 24));

	    if (strstr(daddr,"127") != NULL) // we do not care about loacl host 127.0.0.1
		    return NF_ACCEPT;

	    index = ZPSM_queue_find(iph->daddr);
	    //If the index does not exist, then we create a new entry.
	    //Else we update wakeup frame with this index
	    if (index == -1) 
        {
	    	index = ZPSM_queue_enqueue(iph->daddr);
        }
	    else
	    {
		    timeout = ZPSM_queue_find_timeout(iph->daddr);
			ZPSM_queue_set_verdict(ZPSM_QUEUE_UPDATE_TIMEOUT, iph->daddr, 0);// update timeout		
			updateIndexInt(index);
	    }
	    printk("Index %d and IP is %s\n",index,daddr);
	

	    if (index == -1)
	    {
		    printk("Failed to get index, drop the packet!\n");
	    	return NF_DROP;
	    }

        return NF_ACCEPT;
    }

    if (hooknum == NF_INET_PRE_ROUTING)
    {
	    timeout = ZPSM_queue_find_timeout(iph->saddr);
	    sprintf(saddr, "%d.%d.%d.%d",
                0x0ff & iph->saddr,
                0x0ff & (iph->saddr >> 8),
                0x0ff & (iph->saddr >> 16), 0x0ff & (iph->saddr >> 24));

	    if (strstr(saddr,"127") != NULL) // we do not care about loacl host 127.0.0.1
		    return NF_ACCEPT;

	    index = ZPSM_queue_find(iph->saddr);

    	if (index == -1) 
        {
    		index = ZPSM_queue_enqueue(iph->saddr);
        }
    	else
    	{
   			ZPSM_queue_set_verdict(ZPSM_QUEUE_UPDATE_TIMEOUT, iph->saddr, 0);// update timeout
	    }
	    printk("Index %d and IP is %s\n",index,saddr);

        if (ntohs(udph->dest) == 6789 ||
            ntohs(udph->source) == 6789)
        {
            printk("recieved test UDP data from port 6789, IP: %s\n",saddr);
         
	        if (index != -1)
            {
		        handleControlMessage(skb,index);
            }
	        else 
            {
		        printk("invailed index, packet droped\n");
            }

            return NF_ACCEPT;
        }

        return NF_ACCEPT;
    }

    return NF_ACCEPT;
}

//Netfliter's hook access point
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
 * Timer callback function. This function will be called when timer is expired
*/
void my_timer_callback( unsigned long data )
{
    char msgtoSend[200] = {0};
    printk( "my_timer_callback called (%d).\n", jiffies_to_msecs(jiffies) );
    printk("%d th timer\n",total);
    if (total == 2000) total = 11;
    ret = mod_timer( &my_timer, jiffies + msecs_to_jiffies(40));
    if (total>10) 
    {
	    printk("PKT ID %d \n",total-10);

	    msgtoSend[0] = ',';
	    msgtoSend[1] = msgIndexInt;
	    msgtoSend[2] = '\0';
	    printk("msgIndex from kernel %s\n",msgtoSend);
	    printk("msgIndexInt %d\n",msgIndexInt);
	
    	nlsendmsg(msgtoSend);

    	//flush the request index queue, send index to clients	
    	if (total % 10 == 0)
	    {
	    	ZPSM_request_index_flush();
    	}

	    //Clean up timeout entries
    	if (total % 1000 == 0)
    	{
	    	printk("ZPSM timeout flush\n");
	    	ZPSM_timeout_flush();
	    }
    }
    total++;
    if (ret) printk("Error in mod_timer\n");
}

/**
 * Callback issued when netlink socket is created
 *
 * @param struct sk_buff *skb the control massage's structure
 */
void nl_data_ready(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	int len;
	char msgtoSend[32] = {0};
	char msgbuf[100] = {0};
	int ret = -1;
	
	nlh = nlmsg_hdr(skb);
	len = skb->len;
	
	if(NLMSG_OK(nlh,len)) //Get message from user space just to know it is alive
	{
		memcpy(msgbuf, NLMSG_DATA(nlh), sizeof(msg));
		printk(KERN_INFO "%s",msgbuf);
		if(strcmp(USER_APP_LOADED_MSG, msgbuf) == 0)
		{
			strcpy(msgtoSend, KERNEL_MOD_ACK);
		}
		else if((strcmp(SERIAL_READY_MSG, msgbuf) == 0)&&(!testFlag))
		{
			printk(KERN_INFO "Gotcha %s\n",msgbuf);
			printk(KERN_INFO "msgtoSend %s\n",msgtoSend);
			testFlag = 1;
			ret = nf_register_hook(&zpsm_ops[0]);
			printk("zpsm ops 0 %d\n",ret);
			ret = nf_register_hook(&zpsm_ops[1]);
			printk("zpsm ops 1 %d\n",ret);
			printk("Timer module installing\n");

			init_timer(&my_timer);
			setup_timer( &my_timer, my_timer_callback, 0 );
			add_timer(&my_timer);
		}
		pid = nlh->nlmsg_pid;
		nlsendmsg(msgtoSend);
	}
}



/**
 * Entry module
 * Register netlink, netfliter and kernel socket
 */
static int __init netlink_init(void)
{
	printk(KERN_INFO "enter ZPSM module\n");
	//create a netlink socket
	nl_sock = netlink_kernel_create(&init_net, NETLINK_ZPSM, 0, 
			nl_data_ready, NULL, THIS_MODULE);

	printk(KERN_INFO "Netlink init finished\n");
	init_send();

	if(!nl_sock)
		return -ENOMEM;
	return 0;
}

/**
 * Exit module
 * Unregister netlink, netfliter, kernel socket and timer
 */
static void __exit netlink_exit(void)
{
	int ret;
	printk(KERN_INFO "Timer module uninstalling\n");

	ret = del_timer( &my_timer );
	if (ret) printk("The timer is still in use...\n");
	printk("Timer module uninstalling\n");
	//cleanup – unregister hook
	printk("exit modules testFlag %d\n",testFlag);

	if (testFlag == 1)
	{
		nf_unregister_hook(&zpsm_ops[1]);
		nf_unregister_hook(&zpsm_ops[0]);
	}
	//close the netlink socket
	if(NULL!=nl_sock)
	{
		sock_release(nl_sock->sk_socket);
	}
	exit_send();
	printk(KERN_INFO "exit ZPSM module\n");
}

module_init(netlink_init);
module_exit(netlink_exit);

