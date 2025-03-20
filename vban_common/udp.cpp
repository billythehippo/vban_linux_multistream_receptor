#include "udp.h"


udpc_t* udp_init(uint16_t rx_port, uint16_t tx_port, char* __restrict rx_ip, char* __restrict tx_ip, suseconds_t t, uint8_t priority, int broadcast)
{
    uint8_t prio = priority;
    int bcast = broadcast;

    udpc_t* c = (udpc_t*)malloc(sizeof(udpc_t));
    if(!c)
    {
        MSG("error rx_port=%04x", rx_port);
        return 0;
    }
    memset(c, 0, sizeof(udpc_t));
    //
    c->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(c->fd < 0)
    {
        MSG("error rx_port=%04x", rx_port);
        free(c);
        return 0;
    }

    if (bcast)
    {
        if(setsockopt(c->fd, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast))<0)
        {
            MSG("error rx_port=%04x", rx_port);
            free(c);
            return 0;
        }
        fprintf(stderr, "Broadcast mode successfully set!\n");
    }
    //
#ifdef __linux__
    if(t != 0)
    {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = t;
        setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    }
    //
    if (priority>7) MSG("Wrong Priority value, usind default\n");
    else
    {
        setsockopt(c->fd, SOL_SOCKET, SO_PRIORITY, (const char*)&prio, sizeof(prio));
        MSG("Socket Priority is %d\n", prio);
    }
#endif
    //
    if(rx_port!= 0)
    {
        struct sockaddr_in s_addr;
        memset(&s_addr, 0, sizeof(struct sockaddr_in));
        s_addr.sin_family = AF_INET;
        s_addr.sin_addr.s_addr = htonl(inet_addr(rx_ip));//htonl(INADDR_LOOPBACK);
        s_addr.sin_port = htons(rx_port);
        int ret = bind(c->fd, (struct sockaddr*)&s_addr, sizeof(struct sockaddr_in));
        if(ret < 0)
        {
            MSG("error rx_port=%04x", rx_port);
            free(c);
            return 0;
        }
    }
    //
    if(tx_port != 0)
    {
        c->c_addr.sin_family = AF_INET;
        c->c_addr.sin_addr.s_addr = htonl(inet_addr(tx_ip));
        c->c_addr.sin_port = htons(tx_port);
    }
    //
    return c;
}


void udp_free(udpc_t* c)
{
    close(c->fd);
    free(c);
    c = NULL;
}

