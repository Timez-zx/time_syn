/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2022 Jesper Dangaard Brouer */
#include <stdio.h>
#include <time.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <math.h>
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <stdlib.h>
#include <fcntl.h>
#include "libs/common.h"
#include "libs/stdnetwork.h"


long long handle_scm_timestamping(struct scm_timestamping *ts) {
  return ts->ts[0].tv_sec*1000000000+ts->ts[0].tv_nsec;
}
 

long long handle_time(struct msghdr *msg) {
  long long timeT = 0;

  for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)){
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

static void sync_clock(int *sock, struct sockaddr_in *slave, int dev) {
	int i; 
  char useless_buffer[FIXED_BUFFER];
  long time;
	long long int buf[101]={0};
	int time1[2], time2[2];
	get_time_real(time1);
  get_time_mono(time2);
	long diff = TO_NSEC(time1)-TO_NSEC(time2);
  printf("Running IEEE1588 PTP %d times...\n", NUM_OF_TIMES);
  receive_packet(sock, useless_buffer, FIXED_BUFFER, NULL, NULL);
	long t1[NUM_OF_TIMES], t_drv[NUM_OF_TIMES];
	long long int t_sf[NUM_OF_TIMES];

	for(i = 0; i < NUM_OF_TIMES; i++) {
		send_packet(sock, "sync_packet", 11, NULL, slave);
    get_time_real(time1);
    t1[i] = TO_NSEC(time1);
	}
  

	for(i = 0; i < NUM_OF_TIMES; i++) {
		t_sf[i] = udp_receive(*sock, useless_buffer, FIXED_BUFFER);
	}

  usleep(50);

  read(dev,buf,101);
  int receive_count = buf[100];
  for(i = NUM_OF_TIMES-1; i >= 0; i--) {
    t_drv[i] = buf[receive_count] + diff;
    if(receive_count == 0){
      receive_count = 99;
    }
    else{
      receive_count--;
    }
  }

	for(i = 0; i < NUM_OF_TIMES; i++) {
		printf("%lld\n",t_sf[i]-t_drv[i]);
	}

	for(i = 0; i < NUM_OF_TIMES; i++) {
		send_packet(sock, t1+i, sizeof(time), NULL, slave);
    receive_packet(sock, useless_buffer, FIXED_BUFFER, NULL, NULL);
	}


	for(i = 0; i < NUM_OF_TIMES; i++) {
		send_packet(sock, t_drv+i, sizeof(time), NULL, slave);
    receive_packet(sock, useless_buffer, FIXED_BUFFER, NULL, NULL);
	}
  printf("Done!\n");
}

static void stats_poll()
{
    int sock;
    struct sockaddr_in slave_addr = {0};
    int dev_hello;
    int buf_size = 32*1024;

  	dev_hello = open("/dev/hellodev", O_RDONLY);
    if(dev_hello<0)
    {
      perror("open fail \n");
    }

    slave_addr.sin_family = AF_INET;
    slave_addr.sin_addr.s_addr = inet_addr(SLAVE_IP);  /* send to slave address */
    slave_addr.sin_port = htons(PORT);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    int so_timestamping_flags =
    SOF_TIMESTAMPING_RX_SOFTWARE |
    SOF_TIMESTAMPING_SOFTWARE;
    // SOF_TIMESTAMPING_SOFTWARE | SOF_TIMESTAMPING_RX_HARDWARE |
    // SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE |
    // SOF_TIMESTAMPING_OPT_TSONLY |
    // 0;
    setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &so_timestamping_flags, sizeof(so_timestamping_flags));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)& buf_size, sizeof(int));
    if(unlikely(sock == -1)) {
        ERROR("ERROR creating socket!");
        exit(1);
    } else {
        printf("Socket created!\n");
    }
    
    printf("Syncing time with %s:%d...\n\n", SLAVE_IP, PORT);
    char buffer[FIXED_BUFFER] = {0};
    while(1){
        send_packet(&sock, "sync", 4, NULL, &slave_addr);
        receive_packet(&sock, buffer, FIXED_BUFFER, NULL, &slave_addr);
        int num = NUM_OF_TIMES;
        send_packet(&sock, &num, sizeof(num), NULL, &slave_addr);
        sync_clock(&sock, &slave_addr,dev_hello);
        sleep(2);
    }
    close_socket(sock);
}

int main(int argc, char *argv[])
{
	stats_poll();
	return 0;
}
