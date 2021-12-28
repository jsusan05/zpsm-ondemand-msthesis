/**
 * Name: message.h
 * Description: Module to send data to a destination address from a kernel
 * Author: 
 * Copyright:
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_

//Maximum number of clients possible is limited
#define MAX_CLIENTS 8

/**
* Possible header values for the zigbee message
*/
enum
{
	ZPSM_REQUEST_INDEX = 1,     //Request client auth id to AP
	ZPSM_AUTH_ID,               //Client auth id
	ZPSM_ACK,                   //Acknowledgement
	ZPSM_WAKEUP_FRAME,          //Wakeup frame
	ZPSM_DISCONNECT,            //Disconnect status
	ZPSM_IN_RANGE,              //In-range status
	ZPSM_OUT_RANGE,             //Out of range status
	ZPSM_SERIAL_READY,          //Serial device is ready to use
	ZPSM_P_FROM_KERNEL,         //Receive the channel quality
	ZPSM_LI_FROM_USER           //receive LI from application
};

/**
* Structure of zigbee message send between AP and client through mote
* header_flag The header flag from enum
* id_list     The client authentication id list, do not use 0 and 8, they are reserved
*             Need to support more clients which is an open issue as the serial forwarder code
*             supports only exchange of 2 bytes between mote and board
* num_clients The number of clients in the list
*/
typedef struct zpsm_message_t
{
	unsigned int header_flag;
	unsigned int id_list[MAX_CLIENTS];
	unsigned int num_clients;
}zpsm_message_t;

#endif /* MESSAGE_H_ */
