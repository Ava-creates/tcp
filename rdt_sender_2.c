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
#include <math.h>

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
void update_rto(int sample_rtt){
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

typedef struct selem{
    int seqno;
    int time;
    struct selem* next;
}sentElemNode;
sentElemNode* head;

sentElemNode* createSentElemNode(int seqno){
    sentElemNode* newNode = (sentElemNode*) malloc(sizeof(sentElemNode));
    newNode->seqno = seqno;
    newNode->time = (int) time(NULL);
    newNode->next = NULL;
    return newNode;
}

void addtoSendList(sentElemNode* head, int seqno){
    sentElemNode* newNode = createSentElemNode(seqno);
    if(head == NULL){
        head = newNode;
        return;
    }
    sentElemNode* curr = head;
    while(curr->next != NULL){
        curr = curr->next;
    }
    curr->next = newNode;
}

void removeFromSendList(sentElemNode* head, int seqno){
    sentElemNode* curr = head;
    sentElemNode* prev = NULL;
    while(curr!=NULL && curr->seqno != seqno){
        prev = curr;
        curr = curr->next;
    }

    // could not find for some reason
    if(curr == NULL){
        printf("could not find %d\n", seqno);
        return;
    }

    prev->next = curr->next;
    int ret = curr->time;
    printf("removing %d\n", curr->seqno);
    free(curr);
}

int getTimeFromSendList(sentElemNode* head, int seqno){
    sentElemNode* curr = head;
    sentElemNode* prev = NULL;
    while(curr!=NULL && curr->seqno != seqno){
        prev = curr;
        curr = curr->next;
    }

    // could not find for some reason
    if(curr == NULL){
        return (int) time(NULL);
    }

    prev->next = curr->next;
    int ret = curr->time;
    free(curr);
    return ret;
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
** /


/*
 * init_timer: Initialize timer
 * delay: delay in milliseconds
 * sig_handler: signal handler function for re-sending unACKed packets
 */


void start_timer()
{
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}


void stop_timer()
{
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}
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

int send_packet(int seq_num);

// resent packets with current base
void resend_packets(int sig){
    if (sig == SIGALRM)
    {
        VLOG(INFO, "Timeout happend");
        if(head->next){
            send_packet(head->next->seqno);
            sentElemNode* toRemove = head->next;
            head->next = head->next->next;
            free(toRemove);
        }else{
            printf("nothing to send\n");
        }
        // next_seqno = send_base+DATA_SIZE;
    }
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
    printf("sent: %d\n", seq_num);
    addtoSendList(head, seq_num);
    init_timer(rto, resend_packets);
    start_timer();
    return 0;
}

// send (but many depending on start and end 👍)
void send_bulk(int start_seq_num, int end_seq_num){
    for(int i = start_seq_num; i<end_seq_num; i+=DATA_SIZE){
        if(send_packet(i)==-1){
            break; //EOF
        }
        // if(send_base==i){
        //     start_timer();
        // }
    }
}

int main (int argc, char **argv)
{
    head = createSentElemNode(-1);
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
            int rec = (int) time(NULL) - getTimeFromSendList(head, recvpkt->hdr.ackno-DATA_SIZE);
            printf("rtt for %lu was %d\n", recvpkt->hdr.ackno, rec);
            if(rec){
                update_rto(rec);
                printf("new rtt: %d\n", rto);
            }

            if( recvpkt->hdr.ackno - DATA_SIZE== previous && previous==double_previous) {
                    ssthresh=floor(fmax(window_size/2, 2));
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
                    ssthresh=floor(fmax(window_size/2, 2));
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
    return 0;

}
