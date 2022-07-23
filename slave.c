/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2022 Jesper Dangaard Brouer */
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <math.h>
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <sys/timex.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "libs/common.h"
#include "libs/stdnetwork.h"



long long handle_scm_timestamping(struct scm_timestamping *ts) {
  return ts->ts[0].tv_sec*1000000000+ts->ts[0].tv_nsec;
}

long long handle_time(struct msghdr *msg) {

  long long timeT = 0;

  for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg); cmsg;
       cmsg = CMSG_NXTHDR(msg, cmsg)) {
 
    if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR) {
      struct sock_extended_err *ext =
          (struct sock_extended_err *)CMSG_DATA(cmsg);
      printf("errno=%d, origin=%d\n", ext->ee_errno, ext->ee_origin);
      continue;
    }
 
    if (cmsg->cmsg_level != SOL_SOCKET)
      continue;
 
    switch (cmsg->cmsg_type) {
    case SO_TIMESTAMPNS: {
      struct scm_timestamping *ts = (struct scm_timestamping *)CMSG_DATA(cmsg);
      timeT = handle_scm_timestamping(ts);
    } break;
    case SO_TIMESTAMPING: {
      struct scm_timestamping *ts = (struct scm_timestamping *)CMSG_DATA(cmsg);
      timeT = handle_scm_timestamping(ts);
    } break;
    default:
      break;
    }
  }

  return timeT;
}


long long udp_receive(int sock, char *buf, size_t len) {
  char ctrl[2048];
  struct sockaddr_in infor;
  infor.sin_family = AF_INET;
  infor.sin_port = htons((uint16_t) 2468);
  inet_aton(SLAVE_IP, &infor.sin_addr);
  struct iovec iov = (struct iovec){.iov_base = buf, .iov_len = len};
  struct msghdr msg = (struct msghdr){.msg_control = ctrl,
                                      .msg_controllen = sizeof ctrl,
                                      .msg_name = &infor,
                                      .msg_namelen = sizeof infor,
                                      .msg_iov = &iov,
                                      .msg_iovlen = 1};
                                  
  ssize_t recv_len = recvmsg(sock, &msg, 0);
  if (recv_len < 0) {
    printf("Empty\n");
  }

  long long timeT = handle_time(&msg);
  return timeT;
}


long total_offset = 0;
static void sync_clock(int times, int *sock, struct sockaddr_in *master) {
  char useless_buffer[FIXED_BUFFER];
  int i;
	long time;
	long min_delay,min_offset,min_ms,min_sm;
	min_delay = 1000000000;
	min_offset = 1000000000;
	long delay, offset, ms, sm;
	int time1[2];
  long t1[times],t2[times],t3[times],t4[times];

	get_time_real(time1);
  printf("Running IEEE1588 PTP...\n");
  send_packet(sock,"Running", FIXED_BUFFER, NULL, master);

	for(i = 0; i < times; i++) {
		t2[i] = udp_receive(*sock, useless_buffer, FIXED_BUFFER);
    send_packet(sock,"OK", FIXED_BUFFER, NULL, master);
	}

  receive_packet(sock, useless_buffer, FIXED_BUFFER, NULL, NULL);
	for(i = 0; i < times; i++) {
		send_packet(sock,"delay", FIXED_BUFFER, NULL, master);
    get_time_real(time1);
    t3[i] = TO_NSEC(time1);
    receive_packet(sock, useless_buffer, FIXED_BUFFER, NULL, NULL);
	}

  usleep(200);

  send_packet(sock,"Continue", FIXED_BUFFER, NULL, master);
	for(i = 0; i < times; i++) {
		receive_packet(sock, t1+i, sizeof(time), NULL, NULL);
    send_packet(sock,"OK", FIXED_BUFFER, NULL, master);
	}
    
	for(i = 0; i < times; i++) {
		receive_packet(sock, t4+i, sizeof(time), NULL, NULL);
    send_packet(sock,"OK", FIXED_BUFFER, NULL, master);
	}

	for(i = 0; i < times; i++) {
		ms = t2[i]-t1[i];
		sm = t4[i]-t3[i];
		delay = (ms+sm)/2;
		offset = (ms-sm)/2;
		if(abs(delay) < abs(min_delay) && delay > 0){
			min_delay = delay;
			min_offset = offset;
			min_sm = sm;
			min_ms = ms;
		}
	}

    if(min_delay > 10000 && min_delay < 40000){
        if(abs(min_offset) > 20000){
            clockadj_step(CLOCK_REALTIME, -1*min_offset);
            total_offset = 0;
        }
        else{
            total_offset = total_offset + min_offset;
            clockadj_step(CLOCK_REALTIME, -1*min_offset*0.4-total_offset*0.1);
        }
    }

    printf("Offset = %ldns\n", min_offset);
    printf("Delay = %ldns\n", min_delay);
	  printf("ms_diff = %ldns\n", min_ms);
    printf("sm_diff = %ldns\n", min_sm);
    printf("Done!\n");
}


static void stats_poll()
{
    int sock;
    struct sockaddr_in bind_addr;
    int buf_size = 32*1024;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(unlikely(sock == -1)) {
        ERROR("ERROR creating socket!");
        exit(1);
    } else {
        printf("Socket created!\n");
    }

    memset(&bind_addr, '\0', sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;  
    bind_addr.sin_port = htons(PORT);
    int so_timestamping_flags =
    SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_TX_SOFTWARE |
    SOF_TIMESTAMPING_SOFTWARE | SOF_TIMESTAMPING_RX_HARDWARE |
    SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE | 0;
    setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &so_timestamping_flags, sizeof(so_timestamping_flags));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)& buf_size, sizeof(int));

    if(unlikely(bind(sock, (struct sockaddr *) &bind_addr, sizeof(bind_addr)) < 0)) {
        close_socket(sock);
        ERROR("ERROR binding!\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Bound successfully!\n");
    }
    
    while(1) {
        printf("Ready to receive requests on port %d...\n", PORT);
        struct sockaddr_in addr = {0};
        char buffer[FIXED_BUFFER] = {0};
        receive_packet(&sock, buffer, FIXED_BUFFER, NULL, &addr);
        if(strcmp(buffer, "sync") == 0) {
            send_packet(&sock, "ready", 5, NULL, &addr);
            int t = 0;
            receive_packet(&sock, &t, sizeof(t), NULL, NULL);
            sync_clock(t, &sock, &addr);
        }
        else {
            printf("Received invalid request...\n");
            send_packet(&sock, HELLO, sizeof(HELLO), NULL, &addr);
        }
    }
}

int main(int argc, char *argv[])
{
	stats_poll();
	return 0;
}
