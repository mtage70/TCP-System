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

static int DATA_SIZE = 1460;

//what have I sent?
unsigned int sent_sequence = 0;
//what have a heard back about?
unsigned int rec_sequence = -1;

//make a cache for sender window
void* wind_cache[WIND_SIZE];
unsigned char first_window = 1;
unsigned int debug_time = SEND_TIMEOUT; 

void usage() {
	printf("Usage: 3600send host:port\n");
	exit(1);
}

/**
* Reads the next block of data from stdin
*/
int get_next_data(char *data, int size) {
	return read(0, data, size);
}

/**
* Builds and returns the next packet, or NULL
* if no more data is available.
*/
void *get_next_packet(int sequence, int *len) {
    //store sent packets in buffer
    if (wind_cache[sequence % WIND_SIZE])
    {
        void* cached_pkt = wind_cache[sequence % WIND_SIZE];
        int cached_header_seq = read_hseq(cached_pkt);
        if (cached_header_seq == sequence ) { 
            //packets are only sent from the buffer if packet loss or a timeout occured
            debug_time = SEND_TIMEOUT; 
            mylog( "[From Cache] %i\n", cached_header_seq);
            *len = sizeof(header) + read_hlength(cached_pkt);
            return cached_pkt;
        }
    }
    //increase timeout
    debug_time += SHORT_TIMEOUT; 
    char *data = malloc(DATA_SIZE);
    int data_length = get_next_data(data, DATA_SIZE);
    if (data_length == 0) {
        free(data);
        return NULL;
    }
    
    //create header for packet
    header *myheader = make_header(sequence, data_length, 0, 0);
    
    //create the packet
    void *pkt = malloc(sizeof(header) + data_length + sizeof(checksum_t));
    memcpy(pkt, myheader, sizeof(header));
    memcpy(((char *) pkt) + sizeof(header), data, data_length);
    
    //calculate the checksum for the packet and attach
    checksum_t chksm = chksum(data, data_length); 
    memcpy(((char *) pkt) + sizeof(header) + data_length, &chksm, sizeof(checksum_t)); 
    free(data);
    free(myheader);
    
    //len = packet size in bytes
    *len = sizeof(header) + data_length + sizeof(checksum_t);
    mylog("Checksum %d\n", get_chksum(pkt, data_length + sizeof(header)));
    
    
    if (wind_cache[sequence % WIND_SIZE])
    {
        free(wind_cache[sequence % WIND_SIZE]);
    }
    wind_cache[sequence % WIND_SIZE] = (void*)malloc(sizeof(header) + data_length + sizeof(checksum_t));
    memcpy(wind_cache[sequence % WIND_SIZE], pkt, sizeof(header) + data_length + sizeof(checksum_t));

    return pkt;
}

int send_next_packet(int sock, struct sockaddr_in out) {
    int pkt_len = 0;
    void *pkt = get_next_packet(sent_sequence, &pkt_len);
    if (pkt == NULL)
    {
        return 0;
	}
	mylog("[send data] %d %d (%d)\n", read_hseq(pkt), sent_sequence, pkt_len - sizeof(header));

    if (sendto(sock, pkt, pkt_len, 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0)
    {
        perror("sendto");
        exit(1);
    }
    return 1;
}
int send_next_window(int sock, struct sockaddr_in out, unsigned int* pkts_sent) {
    //determines unacknowledged packets in circulation   
    *pkts_sent = 0;
    for(int i = 0; i < WIND_SIZE; i++) {
        if (!send_next_packet(sock,out))
        {
            break;
        }
        (*pkts_sent)++;
        sent_sequence++;
    }
    sent_sequence--;

    return *pkts_sent;
}


void send_final_packet(int sock, struct sockaddr_in out) {
	header *myheader = make_header(sent_sequence + 1, 0, 1, 0);
	mylog("[send eof]\n");

	if (sendto(sock, myheader, sizeof(header), 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0)
    {
		perror("sendto");
		exit(1);
	}
}

int fast_retrans(int sock, struct sockaddr_in out, fd_set socks, struct timeval t, struct sockaddr_in in, socklen_t in_len ) {    
    for (int i = 0; i < RETRANS_WIND; i++ ) {
        send_next_packet(sock, out);
        if (select(sock + 1, &socks, NULL, NULL, &t))
        {
            unsigned char buf[DATA_SIZE + sizeof(header)];
            int buf_length = sizeof(buf);
            int recvd;
            if ((recvd = recvfrom(sock, &buf, buf_length, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0)
            {
                perror("Recvfrom");
                exit(1);
            }
            header *myheader = get_header(buf);
            if ((myheader->magic == MAGIC) && (myheader->ack == 1))
            {
                mylog("[Recv ack] %d\n", myheader->sequence);
                rec_sequence = myheader->sequence;
            } 
            else 
            {
                mylog("[Recv Corrupted ack] %x %d > %d - %d - %d\n", MAGIC, myheader->sequence, sent_sequence, myheader->ack, myheader->eof);
            }
        } 
        else 
        {
            mylog("[Error] A Timeout Occurred\n");
            sent_sequence = rec_sequence + 1;
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
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

	// extract the host IP and port
	if ((argc != 2) || (strstr(argv[1], ":") == NULL)) {
		usage();
	}

	char *tmp = (char *) malloc(strlen(argv[1])+1);
	strcpy(tmp, argv[1]);

	char *ip_s = strtok(tmp, ":");
	char *port_s = strtok(NULL, ":");

	// first, open a UDP socket  
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	// next, construct the local port
	struct sockaddr_in out;
	out.sin_family = AF_INET;
	out.sin_port = htons(atoi(port_s));
	out.sin_addr.s_addr = inet_addr(ip_s);

	// socket for received packets
	struct sockaddr_in in;
	socklen_t in_len = sizeof(in);

	// construct the socket set
	fd_set socks;

	// construct the timeout
	struct timeval t;
	t.tv_sec = DEBUG_SEND_TIMEOUT;
	t.tv_usec = SEND_TIMEOUT;
	
	//set all elements of window cache to NULL by default
	for (int i = 0; i < WIND_SIZE; i++) {
        wind_cache[i] = NULL;
    }
    
    unsigned int pkts_sent = 0;
    //keeps track of consecutive timeouts
    unsigned int consec_timeouts = 0;
    //keeps track of how many acks have been repeated
    unsigned int repeated_acks = 0;
    
    
	while (send_next_window(sock, out, &pkts_sent) && consec_timeouts < MAX_TIMEOUTS)
	{
        char timeout = 0;
        
		unsigned int done = 0;

		while (!timeout && done < pkts_sent)
		{
            t.tv_sec = DEBUG_SEND_TIMEOUT;
            t.tv_usec = debug_time; 
			FD_ZERO(&socks);
			FD_SET(sock, &socks);

			// wait to receive, or for a timeout
			if (select(sock + 1, &socks, NULL, NULL, &t))
			{
                consec_timeouts = 0;
				unsigned char buf[DATA_SIZE + sizeof(header)];
				int buf_length = sizeof(buf);
				int recvd;
				if ((recvd = recvfrom(sock, &buf, buf_length, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0)
                {
					perror("Recvfrom");
					exit(1);
				}

				header *myheader = get_header(buf);

                if ((myheader->magic == MAGIC) && (myheader->sequence <= sent_sequence) && (myheader->ack == 1))
                {
                    mylog("[Recv ack] %d\n", myheader->sequence);
                    //call fast transmit if too many acks are being repeated                    
                    if (rec_sequence == myheader->sequence)
                    {
                       repeated_acks++;
                    } 
                    else 
                    {
                        repeated_acks = 0;
                    }
                    if (repeated_acks == RETRANS) {
                        repeated_acks = 0;
                        debug_time = DEBUG_SEND_TIMEOUT; 
                        sent_sequence = rec_sequence + 1;
                        if (!fast_retrans(sock, out, socks, t, in, in_len))
                        {
                            break;
                        }
                    } 
                    else 
                    {
                        rec_sequence = myheader->sequence;
                    }
                    done++;
                } 
                else 
                {
                    mylog("[Recv Corrupted ack] %x %d > %d - %d - %d\n", MAGIC, myheader->sequence, sent_sequence, myheader->ack, myheader->eof);
                }
            } 
            else 
            {
                mylog("[error] A Timeout Occurred\n");
                timeout = 1;
                consec_timeouts++;
                sent_sequence = rec_sequence + 1;
                break;
            }
        }
        sent_sequence = rec_sequence + 1;

    }
    //too many timeouts occuring
    if (consec_timeouts == MAX_TIMEOUTS) 
    {
        mylog( "[Max Timeouts Reached]\n" );
    } 
    else 
    {
        consec_timeouts = 0;
        while ((consec_timeouts < (MAX_TIMEOUTS/4))){
            send_final_packet(sock, out);
            
            t.tv_sec = 0;
            t.tv_usec = SHORT_TIMEOUT;
            t.tv_sec = debug_time; 
            
            if (select(sock + 1, &socks, NULL, NULL, &t))
            {
                unsigned char buf[DATA_SIZE + sizeof(header)];
                int buf_length = sizeof(buf);
                int recvd;
                if ((recvd = recvfrom(sock, &buf, buf_length, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0)
                {    
                    perror("Recvfrom");
                    exit(1);
                }

                header* myheader = get_header(buf);
                if ((myheader->magic == MAGIC) &&  myheader->eof)
                {
                    break;
                }
            } 
            else {
                consec_timeouts++;
                mylog( "[Timeout on EoF]\n");
            }
        }
    }

     mylog("[Complete]\n");

    return 0;
}
