/**
 * Name: zpsm_defines.h
 * Description: Definitions
 * Author: 
 * Copyright:
 */

#ifndef __ZPSM_DEFINES_H
#define __ZPSM_DEFINES_H

#define NETLINK_CLIENT_ID       29          //Netlink socket identifier for client kernel module
#define MAX_NETLINK_MSG_SIZE	3           //The number of bytes for a netlink message
#define CHANNEL_TIMEOUT     	200         //Channel timeout to calculate state and LI
#define DEFAULT_HEARTBEAT		120000      //Default LI or heartbeat timeout send to AP
#define DEFAULT_HEARTBEAT_ID	0           //Identifier to flag that default heartbeat be used
#define AP_IP_ADDRESS_LONG		((unsigned long int)0xc0a80101) /* 192.168.1.1 */ //Address of AP in long format
#define AP_IP_ADDRESS			"192.168.1.1" //Address of AP in string format
#define AP_PORT					6789            //Port at which AP sends the control messages

/*
* Structure that identifies the messages between kernel and application
* through netlink socket
**/
enum
{
	USER_APP_LOADED_MSG = 1,    //App has been loaded to kernel
	KERNEL_MOD_ACK,             //Kernel module sends ACK to application
	SERIAL_READY_MSG,           //Serial device is ready to kernel
	UPDATE_AUTH_ID,             //Request to update the authentication id to kernel
	GET_STATE_AND_LI,           //Request to calculate state and LI to application
	UPDATE_STATE_AND_LI,        //Request to update state and LI to kernel
	KERNEL_UNHANDLED_MSG        //When kernel receives a message it doesn't understand
};

#define WLAN_INTERFACE			"wlan0"

#endif // __ZPSM_DEFINES_H

