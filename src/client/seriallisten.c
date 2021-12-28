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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include "../common/serialprotocol.h"

#include "seriallisten.h"

static char *msgs[] = {
  "unknown_packet_type",
  "ack_timeout"	,
  "sync"	,
  "too_long"	,
  "too_short"	,
  "bad_sync"	,
  "bad_crc"	,
  "closed"	,
  "no_memory"	,
  "unix_error"
};

serial_source open_serial_source(const char *device, int baud_rate,
				 int non_blocking,
				 void (*message)(serial_source_msg problem));

void stderr_msg(serial_source_msg problem)
{
  fprintf(stderr, "Note: %s\n", msgs[problem]);
}

static void message(serial_source src, serial_source_msg msg)
{
  if (src->message)
    src->message(msg);
}

static void push_protocol_packet(serial_source src,
				 uint8_t type, uint8_t *packet, uint8_t len)
{
  /* I'm assuming short queues */
  struct packet_list *entry = malloc(sizeof *entry), **last;

  if (!entry)
    {
      message(src, msg_no_memory);
      free(packet);
      return;
    }

  entry->packet = packet;
  entry->len = len;
  entry->next = NULL;

  last = &src->recv.queue[type];
  while (*last)
    last = &(*last)->next;
  *last = entry;
}

static struct packet_list *pop_protocol_packet(serial_source src, uint8_t type)
{
  struct packet_list *entry = src->recv.queue[type];

  if (entry)
    src->recv.queue[type] = entry->next;

  return entry;
}

static bool packet_available(serial_source src, uint8_t type)
{
  return src->recv.queue[type] != NULL;
}

/* Slow implementation of crc function */
static uint16_t crc_byte(uint16_t crc, uint8_t b)
{
  uint8_t i;

  crc = crc ^ b << 8;
  i = 8;
  do
    if (crc & 0x8000)
      crc = crc << 1 ^ 0x1021;
    else
      crc = crc << 1;
  while (--i);

  return crc;
}

static uint16_t crc_packet(uint8_t *data, int len)
{
  uint16_t crc = 0;

  while (len-- > 0)
    crc = crc_byte(crc, *data++);

  return crc;
}

/* The escaper does the sync bytes+escape-like encoding+crc of packets */

static void escape_add(serial_source src, uint8_t b)
{
  src->send.escaped[src->send.escapeptr++] = b;
}

static int init_escaper(serial_source src, int count)
{
  src->send.escaped = malloc(count * 2 + 2);
  if (!src->send.escaped)
    {
      message(src, msg_no_memory);
      return -1;
    }
  src->send.escapeptr = 0;
  src->send.crc = 0;

  escape_add(src, SYNC_BYTE);

  return 0;
}

static void terminate_escaper(serial_source src)
{
  escape_add(src, SYNC_BYTE);
}

static void escape_byte(serial_source src, uint8_t b)
{
  src->send.crc = crc_byte(src->send.crc, b);
  if (b == SYNC_BYTE || b == ESCAPE_BYTE)
    {
      escape_add(src, ESCAPE_BYTE);
      escape_add(src, b ^ 0x20);
    }
  else
    escape_add(src, b);
}

static void free_escaper(serial_source src)
{
  free(src->send.escaped);
}

static int source_wait(serial_source src, struct timeval *deadline)
/* Effects: waits until deadline for some data on source. deadline
     can be NULL for indefinite waiting.
   Returns: 0 if data is available, -1 if the deadline expires
*/
{
  struct timeval tv;
  fd_set fds;
  int cnt;

  if (src->recv.bufpos < src->recv.bufused)
    return 0;

  for (;;)
    {
      if (deadline)
	{
	  gettimeofday(&tv, NULL);
	  tv.tv_sec = deadline->tv_sec - tv.tv_sec;
	  tv.tv_usec = deadline->tv_usec - tv.tv_usec;
	  if (tv.tv_usec < 0)
	    {
	      tv.tv_usec += 1000000;
	      tv.tv_sec--;
	    }
	  if (tv.tv_sec < 0)
	    return -1;
	}

      FD_ZERO(&fds);
      FD_SET(src->fd, &fds);
      cnt = select(src->fd + 1, &fds, NULL, NULL, deadline ? &tv : NULL);
      if (cnt < 0)
	{
	  if (errno == EINTR)
	    continue;
	  message(src, msg_unix_error);
	  return -1;
	}
      if (cnt == 0)
	return -1;
      return 0;
    }
}

static int serial_read(serial_source src, int non_blocking, void *buffer, int n)
{
  fd_set fds;
  int cnt;

  if (non_blocking)
    {
      cnt = read(src->fd, buffer, n);

      /* Work around buggy usb serial driver (returns 0 when no data
	 is available). Mac OS X seems to like to do this too (at
	 least with a Keyspan 49WG).
      */
      if (cnt == 0)
	{
	  cnt = -1;
	  errno = EAGAIN;
	}
      return cnt;
    }
  else
    for (;;)
      {
	FD_ZERO(&fds);
	FD_SET(src->fd, &fds);
	cnt = select(src->fd + 1, &fds, NULL, NULL, NULL);
	if (cnt < 0)
	  return -1;

	cnt = read(src->fd, buffer, n);
	if (cnt != 0)
	  return cnt;
      }
}

static int read_byte(serial_source src, int non_blocking)
/* Returns: next byte (>= 0), or -1 if no data available and non-blocking is true.
*/
{
  if (src->recv.bufpos >= src->recv.bufused)
    {
      for (;;)
	{
	  int n = serial_read(src, non_blocking, src->recv.buffer, sizeof src->recv.buffer);

	  if (n == 0) /* Can't occur because of serial_read bug workaround */
	    {
	      message(src, msg_closed);
	      return -1;
	    }
	  if (n > 0)
	    {
#ifdef DEBUG
	      dump("raw", src->recv.buffer, n);
#endif
	      src->recv.bufpos = 0;
	      src->recv.bufused = n;
	      break;
	    }
#ifndef LOSE32
	  if (errno == EAGAIN)
	    return -1;
	  if (errno != EINTR)
	    message(src, msg_unix_error);
#endif
    }
    }
  //printf("in %02x\n", src->recv.buffer[src->recv.bufpos]);
  return src->recv.buffer[src->recv.bufpos++];
}

static int source_write(serial_source src, const void *buffer, int count)
{
  int actual = 0;

  if (fcntl(src->fd, F_SETFL, 0) < 0)
    {
      message(src, msg_unix_error);
      return -1;
    }
  while (count > 0)
    {
      int n = write(src->fd, buffer, count);

      if (n < 0 && errno == EINTR)
	continue;
      if (n < 0)
	{
	  message(src, msg_unix_error);
	  actual = -1;
	  break;
	}

      count -= n;
      actual += n;
      buffer += n;
    }
  if (fcntl(src->fd, F_SETFL, O_NONBLOCK) < 0)
    {
      message(src, msg_unix_error);
      /* We're in trouble, but there's no obvious fix. */
    }
  return actual;
}

// Write a packet of type 'packetType', first byte 'firstByte'
// and bytes 2..'count'+1 in 'packet'
static int write_framed_packet(serial_source src,
			       uint8_t packet_type, uint8_t first_byte,
			       const uint8_t *packet, int count)
{
  int i, crc;

#ifdef DEBUG
  printf("writing %02x %02x", packet_type, first_byte);
  dump("", packet, count);
#endif

  if (init_escaper(src, count + 4) < 0)
    return -1;

  escape_byte(src, packet_type);
  escape_byte(src, first_byte);
  for (i = 0; i < count; i++)
    escape_byte(src, packet[i]);

  crc = src->send.crc;
  escape_byte(src, crc & 0xff);
  escape_byte(src, crc >> 8);

  terminate_escaper(src);

#ifdef DEBUG
  dump("encoded", src->send.escaped, src->send.escapeptr);
#endif

  if (source_write(src, src->send.escaped, src->send.escapeptr) < 0)
    {
      free_escaper(src);
      return -1;
    }
  free_escaper(src);
  return 0;
}

static void process_packet(serial_source src, uint8_t *packet, int len)
{
  int packet_type = packet[0], offset = 1;

  if (packet_type == P_PACKET_ACK)
    {
      /* send ack */
      write_framed_packet(src, P_ACK, packet[1], NULL, 0);
      /* And merge with un-acked packets */
      packet_type = P_PACKET_NO_ACK;
      offset = 2;
    }
  /* packet must remain a valid pointer to pass to free. So we move the
     data rather than pass an internal pointer */
  memmove(packet, packet + offset, len - offset);
  push_protocol_packet(src, packet_type, packet, len - offset);
}

static void read_and_process(serial_source src, int non_blocking)
/* Effects: reads and processes up to one packet.
*/
{
  uint8_t *packet = src->recv.packet;

  for (;;)
    {
      int byte = read_byte(src, non_blocking);

      if (byte < 0)
	return;

      if (!src->recv.in_sync)
	{
	  if (byte == SYNC_BYTE)
	    {
	      src->recv.in_sync = TRUE;
	      message(src, msg_sync);
	      src->recv.count = 0;
	      src->recv.escaped = FALSE;
	    }
	  continue;
	}
      if (src->recv.count >= MTU)
	{
	  message(src, msg_too_long);
	  src->recv.in_sync = FALSE;
	  continue;
	}
      if (src->recv.escaped)
	{
	  if (byte == SYNC_BYTE)
	    {
	      /* sync byte following escape is an error, resync */
	      message(src, msg_bad_sync);
	      src->recv.in_sync = FALSE;
	      continue;
	    }
	  byte ^= 0x20;
	  src->recv.escaped = FALSE;
	}
      else if (byte == ESCAPE_BYTE)
	{
	  src->recv.escaped = TRUE;
	  continue;
	}
      else if (byte == SYNC_BYTE)
	{
	  int count = src->recv.count;
	  uint8_t *received;
	  uint16_t read_crc, computed_crc;

	  src->recv.count = 0; /* ready for next packet */

	  if (count < 4)
	    /* frames that are too small are ignored */
	    continue;

	  received = malloc(count - 2);
	  if (!received)
	    {
	      message(src, msg_no_memory);
	      continue;
	    }
	  memcpy(received, packet, count - 2);

	  read_crc = packet[count - 2] | packet[count - 1] << 8;
	  computed_crc = crc_packet(received, count - 2);

#ifdef DEBUG
	  dump("received", packet, count);
	  printf("  crc %x comp %x\n", read_crc, computed_crc);
#endif
	  if (read_crc == computed_crc)
	    {
	      process_packet(src, received, count - 2);
	      return; /* give rest of world chance to do something */
	    }
	  else
	    {
	      message(src, msg_bad_crc);
	      free(received);
	      /* We don't lose sync here. If we did, garbage on the line
		 at startup will cause loss of the first packet. */
	      continue;
	    }
	}
      packet[src->recv.count++] = byte;
    }
}

int serial_source_empty(serial_source src)
/* Returns: true if serial source does not contain any pending data, i.e.,
     if the result is true and there is no data available on the source's
     file descriptor, then read_serial_packet will:
       - return NULL if the source is non-blocking
       - block if it is blocking

    (Note: the presence of this calls allows the serial_source to do some
    internal buffering)
*/
{
  return src->recv.bufpos >= src->recv.bufused &&
    !packet_available(src, P_PACKET_NO_ACK);
}

void *read_serial_packet(serial_source src, int *len)
/* Effects: Read the serial source src. If a packet is available, return it.
     If in blocking mode and no packet is available, wait for one.
   Returns: the packet read (in newly allocated memory), with *len is
     set to the packet length, or NULL if no packet is yet available and
     the serial source is in non-blocking mode
*/
{
  read_and_process(src, TRUE);
  for (;;)
    {
      struct packet_list *entry;

      entry = pop_protocol_packet(src, P_PACKET_NO_ACK);
      if (entry)
	{
	  uint8_t *packet = entry->packet;

	  *len = entry->len;
	  free(entry);

	  return packet;
	}
      if (src->non_blocking && serial_source_empty(src))
	return NULL;
      source_wait(src, NULL);
      read_and_process(src, src->non_blocking);
    }
}

serial_source open_mote(const char* device_filename, int baud_rate)
{
	return open_serial_source(device_filename, baud_rate, 0, stderr_msg);
}

serial_source open_serial_source(const char *device, int baud_rate,
				 int non_blocking,
				 void (*message)(serial_source_msg problem))
/* Effects: opens serial port device at specified baud_rate. If non_blocking
     is true, read_serial_packet calls will be non-blocking (writes are
     always blocking, for now at least)
   Returns: descriptor for serial forwarder at host:port, or
     NULL for failure (bad device or bad baud rate)
 */
{
  struct termios newtio;
  int fd;
  tcflag_t baudflag = B115200;//parse_baudrate(baud_rate);

  if (!baudflag)
    return NULL;

  fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0)
    return NULL;

  /* Serial port setting */
  memset(&newtio, 0, sizeof(newtio));
  newtio.c_cflag = CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR | IGNBRK;
  cfsetispeed(&newtio, baudflag);
  cfsetospeed(&newtio, baudflag);

  /* Raw output_file */
  newtio.c_oflag = 0;

  if (tcflush(fd, TCIFLUSH) >= 0 &&
      tcsetattr(fd, TCSANOW, &newtio) >= 0)
    {
      serial_source src = malloc(sizeof *src);

      if (src)
	{
	  memset(src, 0, sizeof *src);
	  src->fd = fd;
	  src->non_blocking = non_blocking;
	  src->message = message;
	  src->send.seqno = 37;

	  return src;
	}
    }
  close(fd);

  return NULL;
}

