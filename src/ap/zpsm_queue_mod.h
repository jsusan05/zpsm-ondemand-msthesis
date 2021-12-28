#ifndef _ZPSM_QUEUE_H
#define _ZPSM_QUEUE_H

#define ZPSM_QUEUE_UPDATE_TIMEOUT 1
#define ZPSM_QUEUE_UPDATE_STATE 2

/**
 * Find index by IP address
 *
 * @param __u32 daddr the IP address
 * @return index if it is found; ohterwise -1
*/
int ZPSM_queue_find(__u32 daddr);

/**
 * Enqueue the entry based on IP address
 *
 * @param __u32 daddr the IP address
 * @return 1 if success; ohterwise -1
*/
int ZPSM_queue_enqueue(__u32 IP);

/**
 * Handle different control message base on verdict
 * If the verdict equals to ZPSM_QUEUE_UPDATE_TIMEOUT, find the entry based on IP address and update active time
 * If the verdict equals to ZPSM_QUEUE_UPDATE_STATE, find the entry based on IP address and update the state
 * 
 * @param verdict quals to ZPSM_QUEUE_UPDATE_TIMEOUT or ZPSM_QUEUE_UPDATE_STATE
 * @param __u32 daddr IP address
 * @param int state station's state
*/
void ZPSM_queue_set_verdict(int verdict, __u32 daddr, int state);

/**
* Print out the total number of ZPSM members
*/
void ZPSM_print_total(void);

/**
* Dequeue based on IP address
*
* @param __u32 IP IP address
*/
void ZPSM_dequeue(__u32 IP);

/**
* Add station to request index queue
*
* @param int index the station's ZPSM index (pre-assigned by AP)
* @param __u32 IP IP address of the station
*/
void ZPSM_add_request_index(int index, __u32 IP);

/**
* send index to clients if the request index queue is not empty
*/
void ZPSM_request_index_flush(void);

/**
* Delete all the timeout station from membership table
*/
void ZPSM_timeout_flush(void);

/**
* Get active time by IP address
* 
* @param __u32 IP IP address
* @return timeout the active time
*/
unsigned long ZPSM_queue_find_timeout(__u32 IP);

#endif
