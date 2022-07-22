#ifndef COMMON_IMPORTED
#define COMMON_IMPORTED

/* default, printf, etc */
#include <stdio.h>
/* gettimeofday() */
#include <sys/time.h>
/* time structs */
#include <time.h>
/* close() */
#include <unistd.h>
#include <sys/types.h>
#include <linux/in6.h>

/* USER-DEFINED SETTINGS */
#define PORT 2468
#define SLAVE_IP "169.254.100.100"
#define NETMAP_INTERFACE "eth0"
#define MASTER_IP "169.254.227.16"
#define MASTER_INTERFACE "enp4s0"
#define NUM_OF_TIMES 1  /* num of times to run protocol */
#define NS_PER_SEC 1000000000LL
#define FIXED_BUFFER 16
#define HELLO "Hello World!"

#ifndef likely /* For branch predictions */
    #define likely(x)   __builtin_expect(!!(x), 1)
    #define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#define TO_NSEC(t) (((long)t[0] * 1000000000L) + t[1])
#define TO_NSEC0(t) (((long)(t.tv_sec) * 1000000000L) + t.tv_nsec)
#define ERROR(err)						        \
	do {										\
		char msg[128];							\
		sprintf(msg, "[FILE:%s|FUNCTION:%s|LINE:%d] %s",	\
		  __FILE__, __FUNCTION__, __LINE__, err);  \
		perror(msg);						 	\
		printf("Exitting...\n");				\
        } while (0)

void get_time_real(int in[2]) {
    if(in != NULL) {
        /* check for nanosecond resolution support */
        #ifndef CLOCK_REALTIME
            struct timeval tv = {0};
            gettimeofday(&tv, NULL);
            in[0] = (int) tv.tv_sec;
            in[1] = (int) tv.tv_usec * 1000;
        #else
            struct timespec ts = {0};
            clock_gettime(CLOCK_REALTIME, &ts);
            in[0] = (int) ts.tv_sec;
            in[1] = (int) ts.tv_nsec;
        #endif
    }
}

void get_time_mono(int in[2]) {
    if(in != NULL) {
        /* check for nanosecond resolution support */
        #ifndef CLOCK_MONOTONIC
            struct timeval tv = {0};
            gettimeofday(&tv, NULL);
            in[0] = (int) tv.tv_sec;
            in[1] = (int) tv.tv_usec * 1000;
        #else
            struct timespec ts = {0};
            clock_gettime(CLOCK_MONOTONIC, &ts);
            in[0] = (int) ts.tv_sec;
            in[1] = (int) ts.tv_nsec;
        #endif
    }
}

void clockadj_step(clockid_t clkid, int64_t step)
{
	struct timex tx;
	int sign = 1;
	if (step < 0) {
		sign = -1;
		step *= -1;
	}
	memset(&tx, 0, sizeof(tx));
	tx.modes = ADJ_SETOFFSET | ADJ_NANO;
	tx.time.tv_sec  = sign * (step / NS_PER_SEC);
	tx.time.tv_usec = sign * (step % NS_PER_SEC);
	/*
	 * The value of a timeval is the sum of its fields, but the
	 * field tv_usec must always be non-negative.
	 */
	if (tx.time.tv_usec < 0) {
		tx.time.tv_sec  -= 1;
		tx.time.tv_usec += 1000000000;
	}
	if (adjtimex(&tx) < 0)
		printf("failed to step clock\n");
        // exit(-1);
}
#endif