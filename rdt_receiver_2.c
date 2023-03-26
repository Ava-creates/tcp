#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <assert.h>

#include "common.h"
#include "packet.h"

/**
 *  TODO:
 *    - Buffer out of order packets
 */

/*
 * You are required to change the implementation to support
 * window size greater than one.
 * In the current implementation the window size is one, hence we have
 * only one send and receive packet
 */
tcp_packet *recvpkt;
tcp_packet *sndpkt;

typedef struct{
    tcp_packet* pkt;
    BufferList* next;
} BufferList;

BufferList* createBufferList(tcp_packet* pkt){
    BufferList* list = (BufferList*) malloc(sizeof(BufferList));
    list->next = NULL;
    list->pkt = make_packet(pkt->hdr.data_size);
    char buffer[DATA_SIZE];
    memcpy(list->pkt->data, buffer, pkt->hdr.data_size);
    return list;
}

void addNode(BufferList* head,  tcp_packet *pkt)
{
    if(head==NULL)
    {
        head = createBufferList(pkt);
        return;
    }

    BufferList* curr = head; 
    while(curr->next && curr->pkt->hdr.seqno < pkt->hdr.seqno){
        curr = curr->next;
    }
    if(!curr->next){
        curr->next = createBufferList(pkt);
    }else{
        BufferList* next = curr->next;
        curr->next = createBufferList(pkt);
        curr->next->next = next; 
    }
}

void printBList(BufferList* head){
    printf("printing now\n");
    BufferList* curr = head;
    while(curr){
        printf("seqno: %d\n", curr->pkt->hdr.seqno);
        curr = curr->next;
    }
}


int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    FILE *fp;
    char buffer[MSS_SIZE];
    struct timeval tp;
    BufferList* head = NULL;

    /* 
     * check command line arguments 
     */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    fp  = fopen(argv[2], "w");
    if (fp == NULL) {
        error(argv[2]);
    }

    /* 
     * socket: create the parent socket 
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, 
                sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    /* 
     * main loop: wait for a datagram, then echo it
     */
    VLOG(DEBUG, "epoch time, bytes received, sequence number");

    clientlen = sizeof(clientaddr);
    int expected_seq = 0;
    int have_seq = -1;
    while (1) {
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        //VLOG(DEBUG, "waiting from server \n");
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &clientaddr, (socklen_t *)&clientlen) < 0) {
            error("ERROR in recvfrom");
        }
        recvpkt = (tcp_packet *) buffer;
        assert(get_data_size(recvpkt) <= DATA_SIZE);
        if ( recvpkt->hdr.data_size == 0) {
            VLOG(INFO, "ayo");
            VLOG(INFO, "End Of File has been reached");
            printBList(head);
            fclose(fp);
            break;
        }
        /* 
         * sendto: ACK back to the client 
         */
        gettimeofday(&tp, NULL);
        VLOG(DEBUG, "%d, %lu, %d, %d", expected_seq, tp.tv_sec, recvpkt->hdr.data_size, recvpkt->hdr.seqno);
        if(expected_seq==recvpkt->hdr.seqno){
            addNode(head, recvpkt);
            fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
            fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);
            sndpkt = make_packet(0);
            sndpkt->hdr.ackno = recvpkt->hdr.seqno;
            sndpkt->hdr.ctr_flags = ACK;
            if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, 
                    (struct sockaddr *) &clientaddr, clientlen) < 0) {
                error("ERROR in sendto");
            }
            have_seq = expected_seq;
            expected_seq += recvpkt->hdr.data_size;
        }else{
            sndpkt = make_packet(0);
            sndpkt->hdr.ackno = expected_seq;
            sndpkt->hdr.ctr_flags = ACK;
            if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, 
                    (struct sockaddr *) &clientaddr, clientlen) < 0) {
                error("ERROR in sendto");
            }
            printf("discarded\n");
        }
    }

    return 0;
}