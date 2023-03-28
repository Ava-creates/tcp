#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

// to compile this code, you need to link math library by using '-lm': gcc -o output code.c -lm


// struct timeval represents an elapsed time. https://pubs.opengroup.org/onlinepubs/7908799/xsh/systime.h.html
//In Linux, the current time is maintained by keeping the number of seconds elapsed since midnight of January 01, 1970 (called epoch)


// struct itimerval represents interval timer that can be programmed to auto-recur (create signals) at a fixed time interval
//  contains two fields of it_value and it_interva
//  If it_value is non-zero, it indicates the time to the next timer expiration. 
// If it_interval is non-zero, it indicates the time to be used to reset the timer when the it_value time elapses.
// If it_value is zero, the timer is disabled and the value of it_interval is ignored. If it_interval is zero, the timer is disabled after the next timer expiration.


FILE *csv;
int window_size = 64;
struct timeval current_packet_time; 
struct itimerval timer;
struct timeval time_init; //for intial time of prgram.
sigset_t sigmask;

void resend_packets(int);
void start_timer();
void stop_timer();
void init_timer(int, void(int));
float timedifference_msec(struct timeval, struct timeval);


int main(int argc, char **argv)
{


	init_timer(500, resend_packets);

	// assuming 
	int p1 = 1;
	int p2 = 1;
	struct timeval p1_time;
	struct timeval p2_time;
	struct timeval t1; // dummy for intermediate calculation
	printf("conceptually sending the packet p1 \n");
	gettimeofday(&p1_time, 0); // note the time of p1 sent
	current_packet_time = p1_time;
	start_timer();

	printf("conceptually sending the packet p2 \n");
	gettimeofday(&p2_time, 0); // note the time of p1 sent

	printf("sleep for 100 milliseconds \n");
	usleep(100000);

	printf("conceptually assumed ack for the packet p1 is received \n");
	stop_timer();

	// now we want to start timer for P2 BUT considering it's sent time
	current_packet_time = p2_time;
    gettimeofday(&t1, 0);
	float elasped = timedifference_msec(p2_time, t1);
	printf("elasped milliseconds for packet p2 : %f\n", elasped);
	int els_int;
	els_int = (int)elasped;
	els_int = 500 - els_int;// time remaning till timeout from p2 timestamp
	printf("remaining time in milliseconds for packet p2 : %d\n", els_int);

	// it will be better to check if els_int is positive
	if (els_int > 0)
	{
	timer.it_value.tv_sec = els_int / 1000; // sets an initial value
    timer.it_value.tv_usec = (els_int % 1000) * 1000;
	start_timer();
	printf("sleep for 500 milliseconds \n");
	usleep(500000); // in midst of it, the timeout of packet 2 will happen

	stop_timer();
	}
	else
	{
	
	// also even without timeout I could call resend packets routine by giving it the sigalarm
	resend_packets(SIGALRM);

	}






	// csv = fopen("../cwnd.csv", "w");
    // if (csv == NULL)
    // {
    //     printf("Error opening csv\n");
    //     return 1;
    // }

	// gettimeofday(&t1, 0);
    // fprintf(csv, "%f,%d\n", timedifference_msec(time_init, t1), (int)congestion_window_size, (int)ssthresh);


	// fclose(csv);

}


void resend_packets(int sig)
{
    if (sig == SIGALRM)
    {

		printf("conceptually resending the packets \n");


		struct timeval t1;
        gettimeofday(&t1, 0); // https://linuxhint.com/gettimeofday_c_language/  gets the system’s clock time. The current time is expressed in elapsed seconds and microseconds since 00:00:00, January 1, 1970 (Unix Epoch).

		printf("Timeout happened after : %f \n", timedifference_msec(t1, current_packet_time));

        // plot new window size

        //fprintf(csv, "%f,%d\n", timedifference_msec(t1, time_init), (int)window_size);

    }
}

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
    signal(SIGALRM, resend_packets);
    timer.it_interval.tv_sec = delay / 1000; // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;
    timer.it_value.tv_sec = delay / 1000; // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}

float timedifference_msec(struct timeval t0, struct timeval t1)
{
    return fabs((t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f);
}