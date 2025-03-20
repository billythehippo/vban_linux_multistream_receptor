#include "vban_functions.h"
#include "udp.h"
#include <signal.h>


void vban_fill_receptor_info(vban_stream_context_t* context)
{
    memset(&context->info, 0, VBAN_PROTOCOL_MAX_SIZE);
    context->info.header.vban = VBAN_HEADER_FOURC;
    context->info.header.format_SR = VBAN_PROTOCOL_TXT;
    strcpy(context->info.header.streamname, "INFO");
    if (context->flags&MULTISTREAM)
    {
        sprintf(context->info.data, "servername=%s ", context->rx_streamname);
    }
    else
    {
        sprintf(context->info.data, "streamnamerx=%s nbchannels=%d ", context->rx_streamname, context->nboutputs);
    }
    sprintf(context->info.data+strlen(context->info.data), "samplerate=%d format=%d flags=%d", context->samplerate, context->vban_output_format, context->flags);
}


void* timerThread(void* arg)
{
    vban_multistream_context_t* context = (vban_multistream_context_t*)arg;
    //client_id_t* clients = (client_id_t*)arg;
    client_id_t* client = NULL;
    // client_id_t* next = NULL;
    pid_t pidtokill;
    uint index;
    int polln;

    while(1) // Thread is enabled cond
    {
        polln = poll(context->offtimer.tds, context->offtimer.tlen, -1);
        for (int i = 0; i < context->offtimer.tlen && polln-- > 0; ++i)
        {
            if (context->offtimer.tds[i].revents & POLLIN)
            {
                int tret = read(context->offtimer.tfd, &context->offtimer.tval, sizeof(context->offtimer.tval));
                if (tret != sizeof(context->offtimer.tval)) // ret should be 8
                {
                    fprintf(stderr, "Timer error\n");
                    break;
                }
                else // on timer actions
                {
                    client = context->clients;
                    if (context->active_clients_num)
                        for (index=0; index<context->active_clients_num; index++)
                        {
                            if (client->timer==10)
                            {
                                if (index==0) // Remove 1-st
                                {
                                    pidtokill = context->clients->pid;
                                    pop(&context->clients);
                                }
                                else
                                {
                                    pidtokill = client->pid;
                                    if (index==context->active_clients_num) remove_last(context->clients);
                                    remove_by_index(&context->clients, index);
                                }
                                kill(pidtokill, SIGINT);
                                context->active_clients_num--;
                                break;
                            }
                            else client->timer++;
                            //if (active_clients_num!=0)
                            client = client->next;
                        }
                }
            }
        }
    }
}


void* rxThread(void* arg)
{
    vban_stream_context_t* stream = (vban_stream_context_t*)arg;
    VBanPacket packet;
    int packetlen;
    int datalen;
    uint32_t ip_in = 0;
    //uint16_t port_in = 0;

    while((stream->flags&RECEIVING)!=0)
    {
        while ((poll(stream->pd, 1, 500))&&((stream->flags&RECEIVING)!=0))
        {
            if (stream->rxport!=0) // UDP
            {
                packetlen = udp_recv(stream->rxsock, &packet, VBAN_PROTOCOL_MAX_SIZE);
                ip_in = stream->rxsock->c_addr.sin_addr.s_addr;
                if (stream->iprx==0) stream->iprx = ip_in;
                stream->txport = htons(stream->rxsock->c_addr.sin_port);
            }
            else
            {
                packetlen = read(stream->pd[0].fd, &packet, VBAN_HEADER_SIZE);
                if (((packet.header.format_SR&VBAN_PROTOCOL_MASK)==VBAN_PROTOCOL_AUDIO)||((packet.header.format_SR&VBAN_PROTOCOL_MASK)==VBAN_PROTOCOL_TXT))
                {
                    datalen = VBanBitResolutionSize[packet.header.format_bit&VBAN_BIT_RESOLUTION_MASK]*(packet.header.format_nbc+1)*(packet.header.format_nbs+1);
                    if (datalen==read(stream->pd[0].fd, packet.data, datalen)) packetlen+= datalen;
                }
            }
            if ((packetlen>=VBAN_HEADER_SIZE)&&(packet.header.vban==VBAN_HEADER_FOURC))
            {
                vban_rx_handle_packet(&packet, packetlen, stream, ip_in, stream->txport);
            }
        }
    }

    return NULL;
}
