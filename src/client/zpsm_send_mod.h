/**
 * Name: zpsm_send_mod.h
 * Description: Module to send data to a destination address from a kernel
 * Author: 
 * Copyright:
 */

#ifndef _ZPSM_SEND_H
#define _ZPSM_SEND_H

/**
* Send data to destination address
* @param buf buffer to send
**/
void sendtoAP(char buf[]);

/**
* Initialize kernel socket
**/
void init_send(void);

/**
* Release kernel socket
**/
void exit_send(void);

#endif
