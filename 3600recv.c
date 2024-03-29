/*
 * CS3600, Spring 2013
 * Project 4 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "3600sendrecv.h"

int main() {
  /**
   * I've included some basic code for opening a UDP socket in C, 
   * binding to a empheral port, printing out the port number.
   * 
   * I've also included a very simple transport protocol that simply
   * acknowledges every received packet.  It has a header, but does
   * not do any error handling (i.e., it does not have sequence 
   * numbers, timeouts, retries, a "window"). You will
   * need to fill in many of the details, but this should be enough to
   * get you started.
   */
   
  //construct a cache for the receiver's window
  void* wind_cache[WIND_SIZE];
  //set the elements of the recvr window cache to NULL by default
  for(int i = 0; i < WIND_SIZE; i++) {
          wind_cache[i] = NULL;
  }

  // first, open a UDP socket  
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  // next, construct the local port
  struct sockaddr_in out;
  out.sin_family = AF_INET;
  out.sin_port = htons(0);
  out.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sock, (struct sockaddr *) &out, sizeof(out))) {
    perror("bind");
    exit(1);
  }

  struct sockaddr_in tmp;
  int len = sizeof(tmp);
  if (getsockname(sock, (struct sockaddr *) &tmp, (socklen_t *) &len)) {
    perror("getsockname");
    exit(1);
  }

  mylog("[bound] %d\n", ntohs(tmp.sin_port));

  // wait for incoming packets
  struct sockaddr_in in;
  socklen_t in_len = sizeof(in);

  // construct the socket set
  fd_set socks;

  // construct the timeout
  struct timeval t;
  t.tv_sec = 30;
  t.tv_usec = 0;

  // our receive buffer
  int buf_len = 1500;
  void* buf = malloc(buf_len);
  //receiver keeps track of what the next logically sequenced packet's sequence # should be
  unsigned int next_sequence = 0;
  
  

  // wait to receive, or for a timeout
  while (1)
  {
    FD_ZERO(&socks);
    FD_SET(sock, &socks);

    if (select(sock + 1, &socks, NULL, NULL, &t))
	{
      int received;
      if ((received = recvfrom(sock, buf, buf_len, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0)
	  {
        perror("recvfrom");
        exit(1);
      }

      //dump_packet(buf, received);

      //store header information
      unsigned int header_seq = read_hseq(buf);
      unsigned int header_magic = read_hmagic(buf);
      int header_len = read_hlength(buf);
      int header_eof = read_heof(buf);
      //store the data
      char *data = get_data(buf);
  
      if (header_magic == MAGIC)
	  {
        //write(1, data, myheader->length);
        //clear old entries from recvr window cache
        if(wind_cache[header_seq & WIND_SIZE])
        {
            free(wind_cache[header_seq & WIND_SIZE]);
            wind_cache[header_seq & WIND_SIZE] = 0 ;
        }
        //check data for error by comparing checksum attached and calculated checksum
        if (get_chksum(data, header_len) == chksum(data, header_len))
        { 
            mylog("Checksum of Packet: %d\n", get_chksum(data,header_len));
            wind_cache[header_seq % WIND_SIZE] = (void*) malloc(sizeof(header) + read_hlength(buf));
            memcpy(wind_cache[header_seq % WIND_SIZE], buf, sizeof(header) + read_hlength(buf));
        }
        
        int ack_length = 0; 
        
        
        //only progress sequence if the header sequence is equal to the next one needed
        if (header_seq == next_sequence)
        {
            write(1, data, header_len);
            next_sequence++;
            void* next_pkt = NULL;
            next_pkt = wind_cache[next_sequence % WIND_SIZE];
            while (next_pkt && read_hseq(next_pkt) == (signed int)next_sequence){
                //fill in packets that have been previously stored
                write(1, get_data(next_pkt), read_hlength(next_pkt));
                next_sequence++;
                next_pkt = wind_cache[next_sequence % WIND_SIZE];
            }
            ack_length = header_len;
        }
        
        
        mylog("[recv data] %d (%d) %s\n", header_seq, header_len, "ACCEPTED (in-order)");
        mylog("[send ack] %d\n", next_sequence - 1);
        header *response_header = make_header(next_sequence - 1, 0, header_eof, 1);
        if (sendto(sock, response_header, sizeof(header), 0, (struct sockaddr *) &in, (socklen_t) sizeof(in)) < 0)
		{
          perror("sendto");
          exit(1);
        }
      }//end of MAGIC if

		//last_received = myheader->sequence + myheader->length;

      if (header_eof)
      {
          mylog("[recv eof]\n");
          mylog("[completed]\n");
          exit(0);
      }
    }//end of select if
   
   else
   {
      mylog("[Error] A Timeout Occurred\n");
      exit(1);
   }
}
   return 0;
}


