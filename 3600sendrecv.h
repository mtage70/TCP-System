/*
 * CS3600, Spring 2013
 * Project 4 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600SENDRECV_H__
#define __3600SENDRECV_H__

#include <stdio.h>
#include <stdarg.h>

#define WIND_SIZE 120
#define SEND_TIMEOUT 80000
#define DEBUG_SEND_TIMEOUT 0
#define SHORT_TIMEOUT 10000
#define RECV_TIMEOUT 15 //sec
#define MAX_TIMEOUTS 100
#define RETRANS 4
#define RETRANS_WIND 20
typedef char checksum_t;

typedef struct header_t {
  unsigned int magic:14;
  unsigned int ack:1;
  unsigned int eof:1;
  unsigned short length;
  unsigned int sequence;
} header;

unsigned int MAGIC;

void dump_packet(unsigned char *data, int size);
header *make_header(int sequence, int length, int eof, int ack);
header *get_header(void *data);
char *get_data(void *data);
char *timestamp();
void mylog(char *fmt, ...);
checksum_t get_chksum(void *data,int len);
checksum_t chksum(char* data,int len);

int read_hseq(void* data);
int read_hlength(void* data);
int read_heof(void* data);
int read_hmagic(void* data);

#endif

