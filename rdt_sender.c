#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

#include"packet.h"
#include"common.h"

#define STDIN_FD    0
#define RETRY  1000 //millisecond

int next_seqno=0;
int send_base=0;
int window_size = 10;


int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer; 
tcp_packet *sndpkt;
tcp_packet *recvpkt;
sigset_t sigmask;       
FILE *fp;


void start_timer()
{
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}


void stop_timer()
{
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}


/*
 * init_timer: Initialize timer
 * delay: delay in milliseconds
 * sig_handler: signal handler function for re-sending unACKed packets
 */
void init_timer(int delay, void (*sig_handler)(int)) 
{
    signal(SIGALRM, sig_handler);
    timer.it_interval.tv_sec = delay / 1000;    // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;  
    timer.it_value.tv_sec = delay / 1000;       // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}

int send_packet(int seq_num){
    char buffer[DATA_SIZE];
    fseek(fp, seq_num, SEEK_SET);
    int len = fread(buffer, 1, DATA_SIZE, fp);
    if (len <= 0){
        return -1;
    }
    sndpkt = make_packet(len);
    memcpy(sndpkt->data, buffer, len);
    sndpkt->hdr.seqno = seq_num;
    sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt),  0, (const struct sockaddr *) &serveraddr, serverlen);
    return 0;
}

void send_bulk(int start_seq_num, int end_seq_num){
    for(int i = start_seq_num; i<end_seq_num; i+=DATA_SIZE){
        if(send_packet(i)==-1){
            break; //EOF
        }
        if(send_base==i){
            start_timer();
        }
    }
}

void resend_packets(int sig){
    if (sig == SIGALRM)
    {
        VLOG(INFO, "Timeout happend");
        send_packet(send_base);
        next_seqno = send_base+DATA_SIZE;
    }
}


int main (int argc, char **argv)
{
    int portno;
    char *hostname;
    char buffer[DATA_SIZE];

    /* check command line arguments */
    if (argc != 4) {
        fprintf(stderr,"usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    fp = fopen(argv[3], "r");
    if (fp == NULL) {
        error(argv[3]);
    }

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");


    /* initialize server server details */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    /* covert host into network byte order */
    if (inet_aton(hostname, &serveraddr.sin_addr) == 0) {
        fprintf(stderr,"ERROR, invalid host %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    assert(MSS_SIZE - TCP_HDR_SIZE > 0);

    //Stop and wait protocol
    init_timer(RETRY, resend_packets);
    next_seqno = 0; // next packet to send

    fseek(fp, 0L, SEEK_END);
    long file_size = ftell(fp);
    while(1){
        if(send_base >= file_size){
            VLOG(INFO, "End Of File has been reached");
            sndpkt = make_packet(0);
            sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0,
                    (const struct sockaddr *)&serveraddr, serverlen);
            break;
        }
        send_bulk(next_seqno, send_base+window_size*DATA_SIZE);
        // shift next seqno to next unsent piece of data
        next_seqno = send_base+window_size*DATA_SIZE;
        // wait while we ack number is equal or more than send base. This means they received packet until ackno
        do{
            recvfrom(sockfd, buffer, MSS_SIZE, 0, (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen);
            recvpkt = (tcp_packet *)buffer;
        }while(recvpkt->hdr.ackno < send_base);
        // ISSUE: SEND BASE GETTING UPDATED EVEN WHEN RECEIVER DIDN'T GET THE RIGHT PACKET
        send_base = (recvpkt->hdr.ackno==-1?0:recvpkt->hdr.ackno)+DATA_SIZE;
    }
    free(sndpkt);
    return 0;

}



