/**
 * Name: seriallisten.h
 * Description: Utility to listen for incoming packets from the mote over serial port
 *
/* Copyright (c) 2006 Intel Corporation
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached INTEL-LICENSE     
 * file. If you do not find these files, copies can be found by writing to
 * Intel Research Berkeley, 2150 Shattuck Avenue, Suite 1300, Berkeley, CA, 
 * 94704.  Attention:  Intel License Inquiry.
 */
/* Authors:  David Gay  <dgay@intel-research.net>
 *           Intel Research Berkeley Lab
 */

#ifndef __SERIALLISTEN_H
#define __SERIALLISTEN_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include "../common/serialprotocol.h"

//CONSTANTS
typedef int bool;

enum {
  FALSE = 0,
  TRUE = 1,
  BUFSIZE = 256,
  MTU = 256,
  ACK_TIMEOUT = 100000, /* in us */
  SYNC_BYTE = SERIAL_HDLC_FLAG_BYTE,
  ESCAPE_BYTE = SERIAL_HDLC_CTLESC_BYTE,

  P_ACK = SERIAL_SERIAL_PROTO_ACK,
  P_PACKET_ACK = SERIAL_SERIAL_PROTO_PACKET_ACK,
  P_PACKET_NO_ACK = SERIAL_SERIAL_PROTO_PACKET_NOACK,
  P_UNKNOWN = SERIAL_SERIAL_PROTO_PACKET_UNKNOWN
};


typedef enum {
  msg_unknown_packet_type,	/* packet of unknown type received */
  msg_ack_timeout, 		/* ack not received within timeout */
  msg_sync,			/* sync achieved */
  msg_too_long,			/* greater than MTU (256 bytes) */
  msg_too_short,		/* less than 4 bytes */
  msg_bad_sync,			/* unexpected sync byte received */
  msg_bad_crc,			/* received packet has bad crc */
  msg_closed,			/* serial port closed itself */
  msg_no_memory,		/* malloc failed */
  msg_unix_error		/* check errno for details */
} serial_source_msg;

struct packet_list
{
  uint8_t *packet;
  int len;
  struct packet_list *next;
};

struct serial_source_t {
  int fd;
  bool non_blocking;
  void (*message)(serial_source_msg problem);

  /* Receive state */
  struct {
    uint8_t buffer[BUFSIZE];
    int bufpos, bufused;
    uint8_t packet[MTU];
    bool in_sync, escaped;
    int count;
    struct packet_list *queue[256]; // indexed by protocol
  } recv;
  struct {
    uint8_t seqno;
    uint8_t *escaped;
    int escapeptr;
    uint16_t crc;
  } send;
};

typedef struct serial_source_t *serial_source;

//FUNCTION DECLARATIONS
/**
* Opens the mote and returns a structure to read or write from it
* @device_filename Device filename of the mote like "/dev/ttyUSB0"
* @baud_rate Baud rate of the serial device by default, 115200
* @return The device source object
**/
serial_source open_mote(const char* device_filename, int baud_rate);

/**
* Reads packets from serial device
* @src The device source object
* @len OUT Length of packets to read
* @return packets read
**/
void *read_serial_packet(serial_source src, int *len);

#endif //__SERIALLISTEN_H

