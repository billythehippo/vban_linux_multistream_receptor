#pragma once

#ifndef UDP_H_
#define UDP_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "../vban_common/vban_functions.h"

#ifndef MSG
    #define MSG(fmt, ...) do { fprintf(stderr, "(%s:%d %s()) " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while(0)
#endif


udpc_t* udp_init(uint16_t rx_port, uint16_t tx_port, char* __restrict rx_ip, char* __restrict tx_ip, suseconds_t t, uint8_t priority, int broadcast);
void udp_free(udpc_t* c);

__always_inline int udp_send(udpc_t* c, uint16_t txport, char* data, size_t n)
{
    if(txport!= 0) c->c_addr.sin_port = htons(txport);
    int ret = sendto(c->fd, data, n, 0, (struct sockaddr*)&(c->c_addr), sizeof(struct sockaddr_in));
    if(ret < 0) return 0;
    return ret;
}

__always_inline int udp_recv(udpc_t* c, void* data, size_t n)
{
	unsigned int c_addr_size = sizeof(struct sockaddr_in);
    int ret = recvfrom(c->fd, data, n, 0, (struct sockaddr*)&(c->c_addr), &c_addr_size);
	if(ret < 0) return 0;
	return ret;
}

__always_inline uint32_t udp_get_sender_ip(udpc_t* c)
{
    return htonl(c->c_addr.sin_addr.s_addr);
}

__always_inline uint16_t udp_get_sender_port(udpc_t* c)
{
    return htons(c->c_addr.sin_port);
}

#endif
