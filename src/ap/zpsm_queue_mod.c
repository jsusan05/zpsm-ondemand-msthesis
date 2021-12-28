/*****************************************************************************
 *
 * Copyright (C) 2001 Uppsala University and Ericsson AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Erik Nordström, <erik.nordstrom@it.uu.se>
 *          
 *
 *****************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/netfilter_ipv4.h>
#include <linux/spinlock.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <net/sock.h>
#include <net/route.h>
#include <net/icmp.h>
#include <linux/timer.h>


#include "zpsm_queue_mod.h"
#include "zpsm_send_mod.h"
#include "zpsm_defines.h"
#include "../common/zpsm_kern_debug.h"



/*
 * This is basically a shameless rippoff of the linux kernel's ip_queue module.
 */

//The max number of stations
#define ZPSM_QUEUE_QMAX_DEFAULT 8
#define ZPSM_QUEUE_PROC_FS_NAME "ZPSM_queue"
//The max number of request queue
#define NET_ZPSM_QUEUE_QMAX 20
#define NET_LZPSM_QUEUE_QMAX_NAME "ZPSM_queue_maxlen"
//Get the IP header from Sk_BUFF
#define SKB_NETWORK_HDR_IPH(skb) ((struct iphdr *)skb_network_header(skb))

//ZPSM membership element
struct ZPSM_queue_entry {
	struct list_head list;
	unsigned long IP;
	int index;
	unsigned long timeout;
	int outOfRange;
};

//Compare two ZPSM stations
typedef int (*ZPSM_queue_cmpfn) (struct ZPSM_queue_entry *, unsigned long);

//Max number of ZPSM stations
static unsigned int queue_maxlen = ZPSM_QUEUE_QMAX_DEFAULT;
//Max number of ZPSM request queue
static unsigned int queue_index_max = NET_ZPSM_QUEUE_QMAX;
//The total number of ZPSM stations
static unsigned int queue_total = 0;
//The total number of ZPSM request station
static unsigned int queue_index_total = 0;
static LIST_HEAD(queue_list);
static int queue_index[9] = {0};
static int queue_index_send[9] = {0};



/*
 * Find and return a queued entry matched by cmpfn
 * 
 */
static inline struct ZPSM_queue_entry
*__ZPSM_queue_find_entry(ZPSM_queue_cmpfn cmpfn, unsigned long data)
{
	struct list_head *p;

	list_for_each_prev(p, &queue_list) {
		struct ZPSM_queue_entry *entry = (struct ZPSM_queue_entry *)p;

		if (cmpfn(entry, data))
			return entry;
	}
	return NULL;
}

/*
 * Put entry into ZPSM queue
 * 
 * @param struct ZPSM_queue_entry *entry the station information
 */
static inline int __ZPSM_queue_enqueue_entry(struct ZPSM_queue_entry *entry)
{
	if (queue_total >= queue_maxlen) {
		if (net_ratelimit())
			printk(KERN_WARNING "ZPSM-queue: full at %d entries, "
			       "dropping packet(s).\n", queue_total);
		return -1;
	}
	list_add(&entry->list, &queue_list);
	queue_total++;
	return 0;
}

/*
 * Check if the IP address equels to entry's IP address
 * 
 * @param struct ZPSM_queue_entry *e the station information
 * @param unsigned long daddr IP address
 *
 * @return 1 if they are equaled; otherwise return 0
 */
static inline int dest_cmp(struct ZPSM_queue_entry *e, unsigned long daddr)
{
	return (daddr == e->IP);
}

/**
 * Find and dequeue an entry based on IP address, return this entry if it is found
 */
static inline struct ZPSM_queue_entry
*__ZPSM_queue_find_dequeue_entry(ZPSM_queue_cmpfn cmpfn, unsigned long data)
{
	struct ZPSM_queue_entry *entry;

	entry = __ZPSM_queue_find_entry(cmpfn, data);
	if (entry == NULL)
		return NULL;

	list_del(&entry->list);
	queue_total--;

	return entry;
}

/**
 * Create an entry for new IP addres, and return index; 
 * If the queue is full, then ruturn -1;
 */
int ZPSM_queue_enqueue(__u32 IP)
{
	int status = -1;
	int index = -1;
	int i;
	struct ZPSM_queue_entry *entry;

	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);

	if (entry == NULL) {
		printk(KERN_ERR
		       "ZPSM_queue: OOM in LBMac_queue_enqueue()\n");
		return -1;
	}
	for (i=1;i<=queue_maxlen;i++)
	{
		if (queue_index[i] == 0)
		{
			index = i;
			queue_index[i] = 1;
			break;
		}
	} 

	//The queue is full;
	if (index == -1) 
	{
		//ZPSM_LOG("ZPSM-queue: full at %d entries, dropping packet(s).\n", queue_total);	
		printk(KERN_WARNING "ZPSM-queue: full at %d entries, "
			       "dropping packet(s).\n", queue_total);
		return -1; 
	}
	
	entry->IP = IP;
	entry->index = index;
	entry->timeout = jiffies_to_msecs(jiffies);
	entry->outOfRange = 1;//We assume that all the nodes are out of range at first

	//write_lock_bh(&queue_lock);

	status = __ZPSM_queue_enqueue_entry(entry);

	if (status < 0)
		goto err_out_unlock;

	//write_unlock_bh(&queue_lock);
	return index;

    err_out_unlock:
	//write_unlock_bh(&queue_lock);
	kfree(entry);

	return -1;
}

//Find index by IP
int ZPSM_queue_find(__u32 IP)
{
	struct ZPSM_queue_entry *entry;
	int index = -1;

	//read_lock_bh(&queue_lock);
	entry = __ZPSM_queue_find_entry(dest_cmp, IP);
	if (entry != NULL)
	{
		index = entry->index;
	}

	//read_unlock_bh(&queue_lock);
	return index;
}

/**
* Get active time by IP address
* 
* @param __u32 IP IP address
* @return timeout the active time
*/
unsigned long ZPSM_queue_find_timeout(__u32 IP)
{
	struct ZPSM_queue_entry *entry;
	unsigned long timeout = 0;

	//read_lock_bh(&queue_lock);
	entry = __ZPSM_queue_find_entry(dest_cmp, IP);
	if (entry != NULL)
	{
		timeout = entry->timeout;
	}

	return timeout;
}

/**
* Print out the total number of ZPSM members
*/
void ZPSM_print_total(void)
{
	printk("There is total: %d index\n",queue_total);
}

//Delete entry by IP and return the entry
static struct ZPSM_queue_entry
*ZPSM_queue_find_dequeue_entry(ZPSM_queue_cmpfn cmpfn, unsigned long data)
{
	struct ZPSM_queue_entry *entry;

	entry = __ZPSM_queue_find_dequeue_entry(cmpfn, data);
	return entry;
}

/**
* Dequeue based on IP address
*
* @param __u32 IP IP address
*/
void ZPSM_dequeue(__u32 IP)
{
	char saddr[254] = {0};
	struct ZPSM_queue_entry *entry;
	entry = ZPSM_queue_find_dequeue_entry(dest_cmp, IP);
	if (entry != NULL)
	{
		queue_index[entry->index] = 0;
	    	sprintf(saddr, "%d.%d.%d.%d",
            	0x0ff & entry->IP,
            	0x0ff & (entry->IP >> 8),
            	0x0ff & (entry->IP >> 16), 0x0ff & (entry->IP >> 24));
		
		if (queue_index_send[entry->index] != 0)
		{
			queue_index_send[entry->index] = 0;
			queue_index_total--;
		}
		
		printk("Index %d, IP %s has been deleted, Total %d entries in the queue\n",entry->index,saddr,queue_total);
		kfree(entry);
	}
}

/**
 * Handle different control message base on verdict
 * If the verdict equals to ZPSM_QUEUE_UPDATE_TIMEOUT, find the entry based on IP address and update active time
 * If the verdict equals to ZPSM_QUEUE_UPDATE_STATE, find the entry based on IP address and update the state
 * 
 * @param verdict quals to ZPSM_QUEUE_UPDATE_TIMEOUT or ZPSM_QUEUE_UPDATE_STATE
 * @param __u32 daddr IP address
 * @param int state station's state
*/
void ZPSM_queue_set_verdict(int verdict, __u32 daddr, int state)
{
	struct ZPSM_queue_entry *entry;

	if (verdict == ZPSM_QUEUE_UPDATE_TIMEOUT) {
			
		entry = __ZPSM_queue_find_entry(dest_cmp, daddr);

		if (entry == NULL)
			return;
		entry->timeout = jiffies_to_msecs(jiffies);


	} else if (verdict == ZPSM_QUEUE_UPDATE_STATE) {

		entry = __ZPSM_queue_find_entry(dest_cmp, daddr);

		if (entry == NULL)
			return;

		entry->timeout = jiffies_to_msecs(jiffies);
		entry->outOfRange = state;
	}

}

/**
* Add station to request index queue
*
* @param int index the station's ZPSM index (pre-assigned by AP)
* @param __u32 IP IP address of the station
*/
void ZPSM_add_request_index(int index, __u32 IP)
{
	printk("queue index total %d\n", queue_index_total);

	if (queue_index_total>=queue_maxlen)
	{
		printk("Request index buffer overflow!\n");
		return;
	}

	if ((index>queue_maxlen)||(index <= 0))
	{
		printk("Request index %d is out of index range\n",index);
		return;
	}
	if (queue_index_send[index] == 0)
	{
		queue_index_total++;
		queue_index_send[index] = IP;
		printk("Add request index %d to sending queue\n");
	}
}

/**
* send index to clients if the request index queue is not empty
*/
void ZPSM_request_index_flush(void)
{
	int i;
	if (queue_index_total == 0) 
	{
		printk("Request index queue is empty!\n");
		return;
	}
	for (i=1;i<=queue_maxlen;i++)
	{
		if (queue_index_send[i] != 0)
		{
			sendmsg(queue_index_send[i], 6789, i);
			queue_index_send[i] = 0;
			queue_index_total--;
		}
	}
	
}

/**
* Delete all the timeout station from membership table
*/
void ZPSM_timeout_flush(void)
{
	struct list_head *p;
	list_for_each_prev(p, &queue_list) {
		struct ZPSM_queue_entry *entry = (struct ZPSM_queue_entry *)p;

		if ((!entry) && (time_after(msecs_to_jiffies(entry->timeout + CLEANUP_TIMEOUT*60*1000), jiffies)))
		{
			if (queue_index_send[entry->index] != 0)
			{
				queue_index_send[entry->index] = 0;
				queue_index_total--;
			}
			list_del(&entry->list);
			queue_total--;
			kfree(entry);

		}
	}
}


