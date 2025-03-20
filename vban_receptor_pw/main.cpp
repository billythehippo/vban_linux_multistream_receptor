#include <time.h>
#include <sys/timerfd.h>
#include <arpa/inet.h>

#include "../vban_common/pipewire_backend.h"
#include "../vban_common/udp.h"
#include "../vban_common/popen2.h"


// void* timerThread(void* arg)
// {
//     vban_multistream_context_t* context = (vban_multistream_context_t*)arg;
//     //client_id_t* clients = (client_id_t*)arg;
//     client_id_t* client = NULL;
//     // client_id_t* next = NULL;
//     pid_t pidtokill;
//     uint index;
//     int polln;

//     while(1) // Thread is enabled cond
//     {
//         polln = poll(context->offtimer.tds, context->offtimer.tlen, -1);
//         for (int i = 0; i < context->offtimer.tlen && polln-- > 0; ++i)
//         {
//             if (context->offtimer.tds[i].revents & POLLIN)
//             {
//                 int tret = read(context->offtimer.tfd, &context->offtimer.tval, sizeof(context->offtimer.tval));
//                 if (tret != sizeof(context->offtimer.tval)) // ret should be 8
//                 {
//                     fprintf(stderr, "Timer error\n");
//                     break;
//                 }
//                 else // on timer actions
//                 {
//                     client = context->clients;
//                     if (context->active_clients_num)
//                         for (index=0; index<context->active_clients_num; index++)
//                         {
//                             if (client->timer==10)
//                             {
//                                 if (index==0) // Remove 1-st
//                                 {
//                                     pidtokill = context->clients->pid;
//                                     pop(&context->clients);
//                                 }
//                                 else
//                                 {
//                                     pidtokill = client->pid;
//                                     if (index==context->active_clients_num) remove_last(context->clients);
//                                     remove_by_index(&context->clients, index);
//                                 }
//                                 kill(pidtokill, SIGINT);
//                                 context->active_clients_num--;
//                                 break;
//                             }
//                             else client->timer++;
//                             //if (active_clients_num!=0)
//                             client = client->next;
//                         }
//                 }
//             }
//         }
//     }
// }


// void* rxThread(void* arg)
// {
//     vban_stream_context_t* stream = (vban_stream_context_t*)arg;
//     VBanPacket packet;
//     int packetlen;
//     int datalen;
//     uint32_t ip_in = 0;
//     //uint16_t port_in = 0;

//     while((stream->flags&RECEIVING)!=0)
//     {
//         while ((poll(stream->pd, 1, 500))&&((stream->flags&RECEIVING)!=0))
//         {
//             if (stream->rxport!=0) // UDP
//             {
//                 packetlen = udp_recv(stream->rxsock, &packet, VBAN_PROTOCOL_MAX_SIZE);
//                 ip_in = stream->rxsock->c_addr.sin_addr.s_addr;
//                 if (stream->iprx==0) stream->iprx = ip_in;
//                 stream->txport = htons(stream->rxsock->c_addr.sin_port);
//             }
//             else
//             {
//                 packetlen = read(stream->pd[0].fd, &packet, VBAN_HEADER_SIZE);
//                 if (((packet.header.format_SR&VBAN_PROTOCOL_MASK)==VBAN_PROTOCOL_AUDIO)||((packet.header.format_SR&VBAN_PROTOCOL_MASK)==VBAN_PROTOCOL_TXT))
//                 {
//                     datalen = VBanBitResolutionSize[packet.header.format_bit&VBAN_BIT_RESOLUTION_MASK]*(packet.header.format_nbc+1)*(packet.header.format_nbs+1);
//                     if (datalen==read(stream->pd[0].fd, packet.data, datalen)) packetlen+= datalen;
//                 }
//             }
//             if ((packetlen>=VBAN_HEADER_SIZE)&&(packet.header.vban==VBAN_HEADER_FOURC))
//             {
//                 vban_rx_handle_packet(&packet, stream, ip_in, stream->txport);
//             }
//         }
//     }

//     return NULL;
// }


int main(int argc, char *argv[])
{
    vban_stream_context_t stream;
    vban_multistream_context_t mscontext;

    memset(&stream, 0, sizeof(vban_stream_context_t));
    memset(&mscontext, 0, sizeof(vban_multistream_context_t));
    
    mscontext.offtimer.timmutex.tid = 0;
    mscontext.offtimer.timmutex.threadlock = PTHREAD_MUTEX_INITIALIZER;
    mscontext.offtimer.timmutex.dataready = PTHREAD_COND_INITIALIZER;

    stream.vban_output_format = VBAN_BITFMT_32_FLOAT;
    stream.samplerate = 48000;
    stream.iprx = 0;

    if (get_receptor_options(&stream, argc, argv)) return 1;
    vban_fill_receptor_info(&stream);

    if (stream.rxport!=0) // UDP rx mode
    {
        stream.rxsock = udp_init(stream.rxport, 0, "0.0.0.0", NULL, 0, 6, 1);
        if (stream.rxsock==NULL)
        {
            fprintf(stderr, "Cannot init UDP socket!\r\n");
            return 1;
        }
        stream.pd[0].fd = stream.rxsock->fd;
    }
    else // PIPE rx mode
    {
        if (strncmp(stream.pipename, "stdin", 6)) // named pipe
        {
            stream.pipedesc = open(stream.pipename, O_RDONLY);
            mkfifo(stream.pipename, 0666);
        }
        else stream.pipedesc = 0; // stdin
        stream.pd[0].fd = stream.pipedesc;
    }

    stream.pd[0].events = POLLIN;
    pthread_attr_init(&stream.rxmutex.attr);
//    pthread_create(&stream.rxmutex.tid, &stream.rxmutex.attr, rxThread, (void*)&stream);
    stream.rxmutex.threadlock = PTHREAD_MUTEX_INITIALIZER;
    stream.rxmutex.dataready  = PTHREAD_COND_INITIALIZER;
    stream.flags|= RECEIVING;

    if (stream.flags&MULTISTREAM)
    {
        VBanPacket packet;
        int packetlen = 0;
        int datalen = 0;
        union
        {
            uint32_t ip_in;
            uint8_t ip_in_bytes[4];
        };
        char cmd[CMDLEN_MAX];
        memset(cmd, 0, CMDLEN_MAX);
        uint16_t cmdlen = strlen(argv[0]);
        memcpy(cmd, argv[0], cmdlen);
        sprintf(cmd+strlen(cmd), " -p0 -istdin -q%d -n%d", stream.nframes, stream.redundancy);

        memset(&packet, 0, VBAN_PROTOCOL_MAX_SIZE);
        //pthread_create(&stream.rxmutex.tid, &stream.rxmutex.attr, mrxThread, (void*)&stream);
        // TODO
        mscontext.offtimer.tfd = timerfd_create(CLOCK_REALTIME,  0);
        mscontext.offtimer.ts.it_interval.tv_sec = 0;
        mscontext.offtimer.ts.it_interval.tv_nsec = 10000000;
        mscontext.offtimer.ts.it_value.tv_sec = 0;
        mscontext.offtimer.ts.it_value.tv_nsec = 10000000;
        timerfd_settime(mscontext.offtimer.tfd, 0, &mscontext.offtimer.ts, NULL);
        mscontext.offtimer.tlen = sizeof(mscontext.offtimer.tds)/sizeof(mscontext.offtimer.tds[0]);
        mscontext.offtimer.tds[0].fd = mscontext.offtimer.tfd;
        mscontext.offtimer.tds[0].events = POLLIN;
        pthread_attr_init(&mscontext.offtimer.timmutex.attr);
        pthread_create(&mscontext.offtimer.timmutex.tid, &mscontext.offtimer.timmutex.attr, timerThread, (void*)&mscontext);


        while(1)
        {
            //        sleep(1);
            while(poll(stream.pd, 1, 0))
            {
                if (stream.rxport!=0) // UDP
                {
                    packetlen = udp_recv(stream.rxsock, &packet, VBAN_PROTOCOL_MAX_SIZE);
                    ip_in = stream.rxsock->c_addr.sin_addr.s_addr;
                    //stream.txport = htons(stream.rxsock->c_addr.sin_port);
                }
                else
                {
                    packetlen = 0;
                    packetlen = read(stream.pd[0].fd, &packet, VBAN_HEADER_SIZE);
                    if (((packet.header.format_SR&VBAN_PROTOCOL_MASK)==VBAN_PROTOCOL_AUDIO)||((packet.header.format_SR&VBAN_PROTOCOL_MASK)==VBAN_PROTOCOL_TXT))
                    {
                        datalen = VBanBitResolutionSize[packet.header.format_bit&VBAN_BIT_RESOLUTION_MASK]*(packet.header.format_nbc+1)*(packet.header.format_nbs+1);
                        if (datalen==read(stream.pd[0].fd, packet.data, datalen)) packetlen+= datalen;
                    }
                }

                if (packetlen)
                {
                    if (packet.header.vban==VBAN_HEADER_FOURC) // VBAN packet
                    {
                        switch(packet.header.format_SR&VBAN_PROTOCOL_MASK)
                        {
                        case VBAN_PROTOCOL_AUDIO:
                            mscontext.client = mscontext.clients;
                            for (mscontext.active_clients_ind=0; mscontext.active_clients_ind<mscontext.active_clients_num; mscontext.active_clients_ind++)
                            {
                                if  ((ip_in==mscontext.client->ip)&&
                                    (((uint32_t*)packet.header.streamname)[0]==((uint32_t*)mscontext.client->header.streamname)[0])&&
                                    (((uint32_t*)packet.header.streamname)[1]==((uint32_t*)mscontext.client->header.streamname)[1])&&
                                    (((uint32_t*)packet.header.streamname)[2]==((uint32_t*)mscontext.client->header.streamname)[2])&&
                                    (((uint32_t*)packet.header.streamname)[3]==((uint32_t*)mscontext.client->header.streamname)[3]))
                                {
                                    mscontext.client->timer = 0;
                                    write(mscontext.client->pipedesc, &packet, packetlen);
                                    // size_t retval = fwrite(&packet, 1, packetlen, client->process);
                                    // if (retval!=packetlen) fprintf(stderr, "Short write!!!\n");
                                    // fflush(client->process);
                                    break;
                                }
                                else mscontext.client = mscontext.client->next;
                                //if (client==NULL) break;
                            }
                            if (mscontext.active_clients_ind==mscontext.active_clients_num) // create new client (append)
                            {
                                if (mscontext.clients==NULL) // first one
                                {
                                    mscontext.clients = (client_id_t*)malloc(sizeof(client_id_t));
                                    memset(mscontext.clients, 0, sizeof(client_id_t));

                                    mscontext.clients->ip = ip_in;
                                    mscontext.clients->header = packet.header;
                                    mscontext.clients->header.nuFrame = 0;
                                    //memset(&cmd[cmdlen], 0, CMDLEN_MAX-cmdlen);
                                    mscontext.clients->pid = popen2(cmd, &mscontext.clients->pipedesc, NULL);
                                    if (mscontext.clients->pid==NULL) fprintf(stderr, "pizdetz!\n");
                                    else fprintf(stderr, "client created!\n");
                                    fprintf(stderr, "Getting incoming stream from %d.%d.%d.%d:%d\r\n", ((uint8_t*)&ip_in)[0], ((uint8_t*)&ip_in)[1], ((uint8_t*)&ip_in)[2], ((uint8_t*)&ip_in)[3], htons(stream.rxsock->c_addr.sin_port));
                                    mscontext.active_clients_num++;
                                }
                                else if (mscontext.active_clients_num<stream.nboutputs)
                                {
                                    mscontext.client = mscontext.clients;
                                    while(mscontext.client->next!=NULL) mscontext.client = mscontext.client->next;
                                    mscontext.client->next = (client_id_t*)malloc(sizeof(client_id_t));
                                    memset(mscontext.client->next, 0, sizeof(client_id_t));

                                    mscontext.client->next->ip = ip_in;
                                    mscontext.client->next->header = packet.header;
                                    mscontext.client->next->header.nuFrame = 0;
                                    //memset(&cmd[cmdlen], 0, CMDLEN_MAX-cmdlen);
                                    mscontext.client->next->pid = popen2(cmd, &mscontext.client->next->pipedesc, NULL);
                                    // if (client->next->pid==nullptr) fprintf(stderr, "pizdetz!\n");
                                    // else fprintf(stderr, "client created!\n");
                                    fprintf(stderr, "Getting incoming stream from %d.%d.%d.%d:%d\r\n", ((uint8_t*)&ip_in)[0], ((uint8_t*)&ip_in)[1], ((uint8_t*)&ip_in)[2], ((uint8_t*)&ip_in)[3], htons(stream.rxsock->c_addr.sin_port));
                                    mscontext.active_clients_num++;
                                }
                                else
                                {
                                    // TODO
                                    //send message that not enougth clients
                                }
                            }
                            break;
                        case VBAN_PROTOCOL_TXT:
                            if (((strncmp(packet.header.streamname, "INFO", 4)==0)||(strncmp(packet.header.streamname, "info", 4)==0))&&((strncmp(packet.data, "/info", 5)==0)||(strncmp(packet.data, "/INFO", 5)==0)))
                            {
                                vban_fill_receptor_info(&stream);
                                sprintf(stream.info.data+strlen(stream.info.data), " max_clients=%d free_clients=%d", stream.nboutputs, stream.nboutputs-mscontext.active_clients_num);
                                fprintf(stderr, "Info request from %d.%d.%d.%d:%d\n", ip_in_bytes[0], ip_in_bytes[1], ip_in_bytes[2], ip_in_bytes[3], htons(stream.rxsock->c_addr.sin_port));
                                udp_send(stream.rxsock, stream.txport, (char*)&stream.info, VBAN_HEADER_SIZE+strlen(stream.info.data));
                            }
                            //else if ((strncmp(packet.header.streamname, "message", 4)==0)||(strncmp(packet.header.streamname, "MESSAGE", 4)==0)) fprintf(stderr, "%s", packet.data);
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
        }

        mscontext.client = mscontext.clients;
        for (uint8_t c=0; c<stream.nboutputs; c++)
            if (mscontext.client->process!=NULL)
            {
                kill(mscontext.client->pid, SIGINT);
                mscontext.client = mscontext.client->next;
            }

        while(mscontext.clients!=NULL) remove_last(mscontext.clients);
        return 0;
    }
    else // SINGLE STREAM
    {
        pthread_create(&stream.rxmutex.tid, &stream.rxmutex.attr, rxThread, (void*)&stream);
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        pthread_mutex_lock(&stream.rxmutex.threadlock);
        while (((stream.rx_streamname[0]==0)||(stream.nboutputs==0))&&((stream.flags&CMD_PRESENT)==0))
        {
            pthread_cond_wait(&stream.rxmutex.dataready, &stream.rxmutex.threadlock);
            if ((stream.flags&CMD_PRESENT)!=0)
            {
                stream.flags&=~CMD_PRESENT;
                if (stream.rxport!=0)
                    udp_send(stream.rxsock, stream.txport, (char*)&stream.info, VBAN_HEADER_SIZE+strlen(stream.info.data));
            }
        }
        pthread_mutex_unlock(&stream.rxmutex.threadlock);

        if ((stream.iptx!=0)&&(stream.rxport!=0)) fprintf(stderr, "Getting incoming stream from %d.%d.%d.%d:%d\r\n", ((uint8_t*)&stream.iptx)[0], ((uint8_t*)&stream.iptx)[1], ((uint8_t*)&stream.iptx)[2], ((uint8_t*)&stream.iptx)[3], stream.rxport);
        vban_compute_rx_buffer(stream.nframes, stream.nboutputs, &stream.rxbuf, &stream.rxbuflen);
        vban_compute_rx_ringbuffer(stream.nframes, 0, stream.nboutputs, 0, &stream.ringbuffer);

        //Create stream
        pw_stream_data_t data = { 0, };
        static struct pw_stream_events stream_events;
        data.user_data = (void*)&stream;
        pw_init_rx_stream(&data, &stream_events, &stream, SPA_AUDIO_FORMAT_F32_LE);

        mutexcond_t runmutex;
        pw_run_rx_stream(&data, &runmutex);

        while (1)
        {
            pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
            pthread_mutex_lock(&stream.rxmutex.threadlock);
            while ((stream.flags&CMD_PRESENT)==0)
                pthread_cond_wait(&stream.rxmutex.dataready, &stream.rxmutex.threadlock);
            stream.flags&=~CMD_PRESENT;
            if (stream.rxport!=0)
                udp_send(stream.rxsock, stream.txport, (char*)&stream.info, VBAN_HEADER_SIZE+strlen(stream.info.data));
            pthread_mutex_unlock(&stream.rxmutex.threadlock);
        }

        pw_stop_rx_stream(&data, &runmutex);

        udp_free(stream.rxsock);
        ringbuffer_free(stream.ringbuffer);
        free(stream.rxbuf);
        pthread_join(stream.rxmutex.tid, NULL);
        return 0;
    }
}
