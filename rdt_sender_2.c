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
#define ALPHA 0.125
#define BETA 0.25

int ssthresh=64; 
int next_seqno=0;
int send_base=0;
float window_size = 1;

// RTO = estimated_rtt + 4 * dev_rtt
// estimated_rtt = (1-a) * estimated_rtt + a * sample_rtt
// dev_rtt = (1-b) * dev_rtt + b * abs(estimated_rtt - sample_rtt)
int rto = 3000;
int estimated_rtt = 0;
int dev_rtt = 0;

// update rto whenever we get a packet back
void update_rto(int sent_time){
    int curr_time = (int) time(NULL);
    int sample_rtt = curr_time - sent_time;
    estimated_rtt = (1-ALPHA) * estimated_rtt + ALPHA * sample_rtt;
    dev_rtt = (1-BETA) * dev_rtt + BETA * abs(estimated_rtt - sample_rtt);
    rto = estimated_rtt + 4 * dev_rtt;
}

int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer; 
tcp_packet *sndpkt;
tcp_packet *recvpkt;
sigset_t sigmask;       
FILE *fp;
int sent_track = 1;

typedef struct {
    int seqno;
    int time;
} sentElem;
sentElem** sentElemArr;


int getFromSent(sentElem** elems, int seqno){
    for(int i = 0; i<sent_track; i++){
        if(elems[i]->seqno == seqno){
            int time = elems[i]->time;
            free(elems[i]);
            elems[i] = NULL;
            return time;
        }
    }
}

void addtoSent(sentElem** elems, int seqno){
    for(int i = 0; i<sent_track; i++){
        if(elems[i] == NULL){
            elems[i] = (sentElem*) malloc(sizeof(sentElem));
            elems[i]->seqno = seqno;
            elems[i]->time = (int) time(NULL);
            return;
        }
    }
    elems = realloc(sizeof(sentElem), sent_track+1);
    elems[sent_track] = (sentElem*) malloc(sizeof(sentElem));
    elems[sent_track]->seqno = seqno;
    elems[sent_track]->time = (int) time(NULL);
    sent_track+=1;
    return;
}

void destroySentElems (sentElem** elems){
    for(int i = 0; i<sent_track; i++){
        if(elems[i] != NULL){
            free(elems[i]);
        }
    }
    free(elems);
}



/**
 *  TODO:
 *  - Handle triple acks
 *  - congestion control
 *      - slow start
 *      - congestion avoidance
 *      - Fast Retransmit
 *  - RTT estimator
 *  - Retransmission timeout timer
 * /

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

// send da packet with sequence num 😎
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
    addtoSent(sentElemArr, seq_num);
    init_timer(rto, resend_packets);
    return 0;
}

// send (but many depending on start and end 👍)
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

// resent packets with current base
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

    sentElemArr = (sentElem**) malloc(sent_track*sizeof(sentElem));

    //Stop and wait protocol
    next_seqno = 0; // next packet to send
    int previous=0; 
    int double_previous=0; 
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
        send_bulk(next_seqno, send_base+floor(window_size)*DATA_SIZE);
        // shift next seqno to next unsent piece of data
        next_seqno = send_base+floor(window_size)*DATA_SIZE;

        do{
            recvfrom(sockfd, buffer, MSS_SIZE, 0, (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen);
            recvpkt = (tcp_packet *)buffer;
            int rec = getFromSent(sentElemArr, recvpkt->hdr.ackno-DATA_SIZE);
            if(rec){
                update_rto(rec);
            }

            if( recvpkt->hdr.ackno - DATA_SIZE== previous && previous==double_previous) {
                    ssthresh=floor(max(window_size/2, 2));
                    window_size=1;
                    //slow start begins again
            } else {
                double_previous=previous; 
                previous = recvpkt->hdr.ackno-DATA_SIZE; 
            }

            if (send_base - DATA_SIZE == recvpkt->hdr.ackno) {   //this checks for packet loss??
                if(floor(window_size)<=ssthresh){
                    window_size++; 
                }
                else{
                    ssthresh=floor(max(window_size/2, 2));
                    // congestion avoidance
                }

            } else{
                window_size+=1/window_size;
            }
                
        }      

        while(recvpkt->hdr.ackno < send_base);

        send_base = recvpkt->hdr.ackno;
    }
    free(sndpkt);
    destroySentElems(sentElemArr);
    return 0;

}
