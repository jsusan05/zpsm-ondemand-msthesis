/**
 * Name: zpsm_send_mod.h
 * Description: Module to send data to a destination address from a kernel
 * Author: 
 * Copyright:
 */

#ifndef _ZPSM_SEND_H
#define _ZPSM_SEND_H


/**
* Send client auth id to destination address
* @param daddr destination address
* @param portNum destination address length
* @param index auth id of client
**/
void sendmsg(__u32 daddr, int portNum, int index);

/**
* Initialize kernel socket
**/
void init_send(void);

/**
* Release kernel socket
**/
void exit_send(void);

#endif
