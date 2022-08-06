#ifndef COMMON_IMPORTED
#define COMMON_IMPORTED
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define PORT 2468
#define SLAVE_IP "169.254.100.100"
#define NETMAP_INTERFACE "enp4s0"
#define MASTER_IP "169.254.227.16"
#define MASTER_INTERFACE "enp4s0"
#define NUM_OF_TIMES 15
#define FIXED_BUFFER 16
#define HELLO "Hello World!"
#ifndef likely /* For branch predictions */
    #define likely(x)   __builtin_expect(!!(x), 1)
    #define unlikely(x) __builtin_expect(!!(x), 0)
#endif
#define TO_NSEC(t) (((long)t[0] * 1000000000L) + t[1])
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
#endif