/**
 * Name: zpsm_send_mod.c
 * Description: Module to send data to a destination address from a kernel
 * Author: 
 * Copyright:
 */

//SYSTEM INCLUDES
#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
#include <linux/config.h>
#endif
#ifdef KERNEL26
#include <linux/moduleparam.h>
#endif
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <net/route.h>

#include <linux/workqueue.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/timer.h>
#include <asm/param.h>

//INTERNAL INCLUDES
#include "zpsm_send_mod.h"
#include "../common/message.h"

#define MAXNUM 15

//Buffer to send
static unsigned char bufferNew[2];
//Socket structure
static struct socket *sock;

/**
* Send data to destination address
* @param sock socket structure
* @param buff buffer to send
* @param len Length of buffer to send
* @param flags Flags for message header
* @param addr destination address
* @param addr destination address length
* @return 0 success else error
**/
static int my_sendto(struct socket *sock, void * buff, size_t len,
unsigned flags, struct sockaddr *addr, int addr_len)
{
	struct kvec vec;
	struct msghdr msg;

	vec.iov_base=buff;
	vec.iov_len=len;

	memset(&msg, 0x00, sizeof(msg));
	msg.msg_name=addr;
	msg.msg_namelen=addr_len;
	msg.msg_flags = flags | MSG_DONTWAIT;

	return kernel_sendmsg(sock, &msg, &vec, 1, len);
}

/**
* Send client auth id to destination address
* @param daddr destination address
* @param portNum destination address length
* @param index auth id of client
**/
void sendmsg(__u32 daddr, int portNum, int index)
{

	printk("sendmsg start\n");
	char saddr[254] = {0};
	int n;
	int i;
	struct sockaddr_in addr;
	memset(&addr, 0x00, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(portNum);
	addr.sin_addr.s_addr = daddr;
	

	bufferNew[0] = ZPSM_AUTH_ID;
	bufferNew[1] = index;	

	sprintf(saddr, "%d.%d.%d.%d",
            0x0ff & daddr,
            0x0ff & (daddr >> 8),
            0x0ff & (daddr >> 16), 0x0ff & (daddr >> 24));

	printk("======Assign ID for %s=========\n",saddr);
	printk("sendmsg: control message AUTH_ID %d\n",bufferNew[0]);
	printk("sendmsg: index %d\n",bufferNew[1]);

	n = my_sendto(sock, bufferNew, sizeof(bufferNew), 0, (struct sockaddr *)&addr, sizeof(addr));
	
	printk("n is %d\n",n);


}

/**
* Initialize kernel socket
**/
void init_send()
{
	printk("Init send program\n");
	sock_create_kern(PF_INET, SOCK_DGRAM, 0, &sock);
}

/**
* Release kernel socket
**/
void exit_send()
{
	sock_release(sock);
	printk("Exit send program\n");
}
