/**
 * Name: serialsend.h
 * Description: Utility to send some message to the mote over serial port
 * Author: 
 * Copyright:
 */

#ifndef __SERIALSEND_H
#define __SERIALSEND_H

#include "../common/message.h"

/**
* Send a message from PC to mote through serial port
* @param device_filename The filename of mote device eg. /dev/ttyUSB0
* @param msg The message to send to mote
* @param baud_rate baude rate of mote
* @return 0 for success else error
**/
int sendMsgToMote(const char* device_filename, zpsm_message_t msg, int baud_rate);

#endif //__SERIALSEND_H
