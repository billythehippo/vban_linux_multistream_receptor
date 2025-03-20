#ifndef VBAN_FUNCTIONS_H
#define VBAN_FUNCTIONS_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <arpa/inet.h>
//#include <errno.h>
//#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

#include "vban.h"
#include "vban_client_list.h"
#include "ringbuffer.h"
//#include "udp.h"

// TYPE DEFS

#define CMDLEN_MAX 600

typedef struct
{
    pthread_t tid;
    pthread_attr_t attr;
    pthread_mutex_t threadlock;// = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  dataready;// = PTHREAD_COND_INITIALIZER;
} mutexcond_t;

typedef struct
{
    int fd;
    struct sockaddr_in c_addr;
} udpc_t;

typedef struct
{
    timer_t t;
    int tfd;
    struct itimerspec ts;
    struct pollfd tds[1];
    uint64_t tval;
    int tlen;
    mutexcond_t timmutex;
} timer_simple_t;

// typedef struct
// {
//     udpc_t* udpsocket;
//     mutexcond_t* mutexcond;
//     ringbuffer_t* ringbuffer;
// } rxcontext_t;

// CONTEXT VBAN

typedef struct vban_stream_context_t {
    char rx_streamname[16];
    char tx_streamname[16];
    uint16_t nbinputs = 0;
    uint16_t nboutputs = 0;
    uint16_t nframes;
    int samplerate;
    char* portbuf;
    int portbuflen;
    char* txbuf;
    int txbuflen;
    float* rxbuf;
    int rxbuflen;
    ringbuffer_t* ringbuffer;
    ringbuffer_t* ringbuffer_midi;
    union
    {
        uint32_t iptx;
        uint8_t iptx_bytes[4];
    };
    uint16_t txport;
    union
    {
        uint32_t iprx;
        uint8_t iprx_bytes[4];
    };
    uint16_t rxport;
    char pipename[32];
    int pipedesc;
    pollfd pd[1];
    uint32_t nu_frame;
    uint8_t vban_input_format;
    uint8_t vban_output_format;
    uint8_t redundancy;
    uint8_t vban_nframes_pac;
    uint8_t lost_pac_cnt;
    udpc_t* rxsock;
    udpc_t* txsock;
    mutexcond_t rxmutex;
    uint16_t flags;
    VBanPacket info;
    char* jack_server_name;
} vban_stream_context_t;
//FLAGS COMMON
#define RECEIVER        0x0001
#define TRANSMITTER     0x0002
#define RECEIVING       0x0004
#define SENDING         0x0008
#define CMD_PRESENT     0x0010

#define PLUCKING_EN     0x0040
#define PLUCKING_ON     0x0080
#define MULTISTREAM     0x0100
#define DEVICE_MODE     0x0200
#define AUTOCONNECT     0x0400

typedef struct vban_multistream_context_t
{
    timer_simple_t offtimer;
    uint8_t vban_clients_min = 0;
    uint8_t vban_clients_max = 0;
    uint8_t active_clients_num = 0;
    uint8_t active_clients_ind = 0;
    client_id_t* clients;
    client_id_t* client;
} vban_multistream_context_t;

#define CARD_NAME_LENGTH 32
//#ifdef __linux__
//#endif

#define CMD_SIZE 300


__always_inline uint16_t int16betole(u_int16_t input)
{
    return ((((uint8_t*)&input)[0])<<8) + ((uint8_t*)&input)[1];
}


__always_inline uint32_t int32betole(uint32_t input)
{
    return (((((((uint8_t*)&input)[0]<<8)+((uint8_t*)&input)[1])<<8)+((uint8_t*)&input)[2])<<8)+((uint8_t*)&input)[3];
}


__always_inline void vban_inc_nuFrame(VBanHeader* header)
{
    header->nuFrame++;
}


__always_inline int vban_sample_convert(void* dstptr, uint8_t format_bit_dst, void* srcptr, uint8_t format_bit_src, int num)
{
    int ret = 0;
    uint8_t* dptr;
    uint8_t* sptr;

    int dst_sample_size;
    int src_sample_size;

    if (format_bit_dst==format_bit_src)
    {
        memcpy(dstptr, srcptr, VBanBitResolutionSize[format_bit_dst]*num);
        return 0;
    }

    dptr = (uint8_t*)dstptr;
    sptr = (uint8_t*)srcptr;

    dst_sample_size = VBanBitResolutionSize[format_bit_dst];
    src_sample_size = VBanBitResolutionSize[format_bit_src];

    switch (format_bit_dst)
    {
    case VBAN_BITFMT_8_INT:
        switch (format_bit_src)
        {
        case VBAN_BITFMT_16_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_24_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[sample*dst_sample_size] = sptr[2 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[sample*dst_sample_size] = sptr[3 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_FLOAT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[sample*dst_sample_size] = (int8_t)roundf((float)(1<<7)*((float*)sptr)[sample]);
            }
            break;
        default:
            fprintf(stderr, "Convert Error! Unsuppotred source format!%d\n", format_bit_src);
            ret = 1;
        }
        break;
    case VBAN_BITFMT_16_INT:
        switch (format_bit_src)
        {
        case VBAN_BITFMT_8_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_24_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
                dptr[1 + sample*dst_sample_size] = sptr[2 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = sptr[2 + sample*src_sample_size];
                dptr[1 + sample*dst_sample_size] = sptr[3 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_FLOAT:
            for (int sample=0; sample<num; sample++)
            {
                ((int16_t*)dptr)[sample*dst_sample_size] = (int16_t)roundf((float)(1<<15)*((float*)sptr)[sample]);
            }
            break;
        default:
            fprintf(stderr, "Convert Error! Unsuppotred source format!%d\n", format_bit_src);
            ret = 1;
        }
        break;
    case VBAN_BITFMT_24_INT:
        switch (format_bit_src)
        {
        case VBAN_BITFMT_8_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = 0;
                dptr[2 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_16_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
                dptr[2 + sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
                dptr[1 + sample*dst_sample_size] = sptr[2 + sample*src_sample_size];
                dptr[2 + sample*dst_sample_size] = sptr[3 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_FLOAT:
            for (int sample=0; sample<num; sample++)
            {
                ((int32_t*)dptr)[sample*dst_sample_size] = (int32_t)roundf((float)(1<<23)*((float*)sptr)[sample]);
            }
            break;
        default:
            fprintf(stderr, "Convert Error! Unsuppotred source format!%d\n", format_bit_src);
            ret = 1;
        }
        break;
    case VBAN_BITFMT_32_INT:
        switch (format_bit_src)
        {
        case VBAN_BITFMT_8_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = 0;
                dptr[2 + sample*dst_sample_size] = 0;
                dptr[3 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_16_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = 0;
                dptr[2 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
                dptr[3 + sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_24_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
                dptr[2 + sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
                dptr[3 + sample*dst_sample_size] = sptr[2 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_FLOAT:
            for (int sample=0; sample<num; sample++)
            {
                ((int32_t*)dptr)[sample*dst_sample_size] = (int32_t)roundf((float)(1<<31)*((float*)sptr)[sample]);
            }
            break;
        default:
            fprintf(stderr, "Convert Error! Unsuppotred source format!%d\n", format_bit_src);
            ret = 1;
        }
        break;
    case VBAN_BITFMT_32_FLOAT:
        switch (format_bit_src)
        {
        case VBAN_BITFMT_8_INT:
            for (int sample=0; sample<num; sample++)
            {
                ((float*)dptr)[sample] = (float)sptr[sample]/(float)(1<<7);
            }
            break;
        case VBAN_BITFMT_16_INT:
            for (int sample=0; sample<num; sample++)
            {
                ((float*)dptr)[sample] = (float)(((int16_t*)sptr)[sample])/(float)(1<<15);
            }
            break;
        case VBAN_BITFMT_24_INT:
            for (int sample=0; sample<num; sample++)
            {
                ((float*)dptr)[sample] = (float)((((int8_t)sptr[2 + sample*src_sample_size])<<16)+(sptr[1 + sample*src_sample_size]<<8)+sptr[0 + sample*src_sample_size])/(float)(1<<23);
            }
            break;
        case VBAN_BITFMT_32_INT:
            for (int sample=0; sample<num; sample++)
            {
                ((float*)dptr)[sample] = (float)(((int32_t*)sptr)[sample])/(float)(1<<31);
            }
            break;
        default:
            fprintf(stderr, "Convert Error! Unsuppotred source format!%d\n", format_bit_src);
            ret = 1;
        }
        break;
    default:
        fprintf(stderr, "Convert Error! Unsuppotred destination format!%d\n", format_bit_dst);
        ret = 1;
    }
    return ret;
}


// __always_inline void sample_float_to_vban_format(uint8_t* ptr, float sample, uint8_t format_bit)
// {
//     int32_t tmp = 0;
// //    uint32_t* tp;

//     switch (format_bit)
//     {
//     case 0: //VBAN_BITFMT_8_INT:
//         tmp = (int8_t)roundf((float)(1<<7)*sample); //((float)(1<<7)*sample);//
//         ptr[0] = tmp&0xFF;
//         break;
//     case 1: //VBAN_BITFMT_16_INT:
//         tmp = (int16_t)roundf((float)(1<<15)*sample); //((float)(1<<15)*sample);//
//         ptr[0] = tmp&0xFF;
//         ptr[1] = (tmp>>8)&0xFF;
//         break;
//     case 2: //VBAN_BITFMT_24_INT:
//         tmp = (int32_t)roundf((float)(1<<23)*sample); //((float)(1<<23)*sample);//
//         ptr[0] = tmp&0xFF;
//         ptr[1] = (tmp>>8)&0xFF;
//         ptr[2] = (tmp>>16)&0xFF;
//         break;
//     case 3: //VBAN_BITFMT_32_INT:
//         ptr[0] = tmp&0xFF;
//         ptr[1] = (tmp>>8)&0xFF;
//         ptr[2] = (tmp>>16)&0xFF;
//         ptr[3] = (tmp>>24)&0xFF;
//         break;
//     case 4: //VBAN_BITFMT_32_FLOAT:
// //        tp = (uint32_t*)(&sample);
//         tmp = (int32_t)roundf((float)(1<<31)*sample);
//         ptr[0] = tmp&0xFF;
//         ptr[1] = (tmp>>8)&0xFF;
//         ptr[2] = (tmp>>16)&0xFF;
//         ptr[3] = (tmp>>24)&0xFF;
//         break;
//     //case 5: //VBAN_BITFMT_64_FLOAT:
//     default:
//         fprintf(stderr, "Convert Error! Unsuppotred source format!%d\n", format_bit);
//         break;
//     }
// }


// __always_inline float sample_vban_format_to_float(uint8_t* ptr, uint8_t format_bit)
// {
//     int value = 0;

//     switch (format_bit)
//     {
//     case 0: //VBAN_BITFMT_8_INT:
//         return (float)(*((int8_t const*)ptr)) / (float)(1 << 7);

//     case 1: //VBAN_BITFMT_16_INT:
//         return (float)(*((int16_t const*)ptr)) / (float)(1 << 15);

//     case 2: //VBAN_BITFMT_24_INT:
//         value = (((int8_t)ptr[2])<<16) + (ptr[1]<<8) + ptr[0];
//         return (float)(value) / (float)(1 << 23);

//     case 3: //VBAN_BITFMT_32_INT:
//         return (float)*((int32_t const*)ptr) / (float)(1 << 31);

//     case 4: //VBAN_BITFMT_32_FLOAT:
//         return *(float const*)ptr;

//         //case 5: //VBAN_BITFMT_64_FLOAT:
//     default:
//         fprintf(stderr, "Convert Error! %d\n", format_bit);
//         return 0.0;
//     }
// }


__always_inline int vban_get_format_SR(long host_samplerate)
{
    int i;
    for (i=0; i<VBAN_SR_MAXNUMBER; i++) if (host_samplerate==VBanSRList[i]) return i;
    return -1;
}


__always_inline uint vban_strip_vban_packet(uint8_t format_bit, uint16_t nbchannels)
{
    uint framesize = VBanBitResolutionSize[format_bit]*nbchannels;
    uint nframes = VBAN_DATA_MAX_SIZE/framesize;
    if (nframes>VBAN_SAMPLES_MAX_NB) nframes = VBAN_SAMPLES_MAX_NB;
    return nframes*framesize;
}


__always_inline uint vban_strip_vban_data(uint datasize, uint8_t format_bit, uint16_t nbchannels)
{
    uint framesize = VBanBitResolutionSize[format_bit]*nbchannels;
    uint nframes = datasize/framesize;
    if (nframes>VBAN_SAMPLES_MAX_NB) nframes = VBAN_SAMPLES_MAX_NB;
    return nframes*framesize;
}


__always_inline uint vban_calc_nbs(uint datasize, uint8_t resolution, uint16_t nbchannels)
{
    return datasize/VBanBitResolutionSize[resolution]*nbchannels;
}


__always_inline uint vban_packet_to_float_buffer(uint pktdatalen, uint8_t resolution)
{
    return sizeof(float)*pktdatalen/VBanBitResolutionSize[resolution];
}


__always_inline int file_exists(const char* __restrict filename)
{
    if (access(filename, F_OK)==0) return 1;
    return 0;
}


__always_inline void vban_free_rx_ringbuffer(ringbuffer_t* ringbuffer)
{
    if (ringbuffer!= NULL) ringbuffer_free(ringbuffer);
}


__always_inline void vban_compute_rx_ringbuffer(int nframes, int nframes_pac, int nbchannels, int redundancy, ringbuffer_t** ringbuffer)
{
    char* zeros;
    char div = 1;
    nframes = (nframes>nframes_pac ? nframes : nframes_pac);
    vban_free_rx_ringbuffer(*ringbuffer);
    *ringbuffer = ringbuffer_create(2 * nframes * nbchannels * (redundancy + 1) * sizeof(float));
    memset((*ringbuffer)->buf, 0, (*ringbuffer)->size);
    if (redundancy>0) // TODO : REWORK THIS!!!
    {
        if (redundancy<2) div = 2;
        zeros = (char*)calloc(1, (*ringbuffer)->size>>div);
        ringbuffer_write(*ringbuffer, zeros, (*ringbuffer)->size>>div);
        free(zeros);
    }
}


__always_inline void vban_free_line_buffer(void* buffer, int* bufsize)
{
    if (buffer!= NULL) free(buffer);
    bufsize = 0;
}


__always_inline void vban_compute_rx_buffer(int nframes, int nbchannels, float** rxbuffer, int* rxbuflen)
{
    vban_free_line_buffer(*rxbuffer, rxbuflen);
    *rxbuflen = nbchannels*nframes;
    *rxbuffer = (float*)malloc(*rxbuflen*sizeof(float));
}


__always_inline int vban_compute_line_buffer(char* buffer, int nframes, int nbchannels, int bitres)
{
    int size = nframes*nbchannels*bitres;
    vban_free_line_buffer(buffer, NULL);
    buffer = (char*)malloc(size);
    return size;
}


__always_inline uint8_t vban_compute_tx_packets(int* pacdatalen, int* pacnum, int nframes, int nbchannels, int bitres)
{
    *pacdatalen = nframes*nbchannels*bitres;
    *pacnum = 1;
    while((*pacdatalen>VBAN_DATA_MAX_SIZE)||((*pacdatalen/(bitres*nbchannels))>256))
    {
        *pacdatalen = *pacdatalen>>1;
        *pacnum = *pacnum<<1;
    }
    return *pacdatalen/(bitres*nbchannels) - 1;
}


__always_inline int vban_read_frame_from_ringbuffer(float* dst, ringbuffer_t* ringbuffer, int num)
{
    size_t size = num*sizeof(float);
    if (ringbuffer_read_space(ringbuffer)>=size)
    {
        ringbuffer_read(ringbuffer, (char*)dst, size);
        return 0;
    }
    return 1;
}


__always_inline int vban_add_frame_from_ringbuffer(float* dst, ringbuffer_t* ringbuffer, int num)
{
    float fsamples[256];
    size_t size = num*sizeof(float);
    if (ringbuffer_read_space(ringbuffer)>=size)
    {
        ringbuffer_read(ringbuffer, (char*)fsamples, size);
        for (int i=0; i<num; i++) dst[i] = (dst[i] + fsamples[i])/2;
        return 0;
    }
    return 1;
}


__always_inline static int vban_rx_handle_packet(VBanPacket* vban_packet, int packetlen, vban_stream_context_t* context, uint32_t ip_in, u_int16_t port_in)
{
    uint16_t nbc;
    static float fsamples[VBAN_CHANNELS_MAX_NB];
    size_t framesize;
    char* srcptr;
    uint16_t eventptr;

    switch (vban_packet->header.format_SR&VBAN_PROTOCOL_MASK)
    {
    case VBAN_PROTOCOL_AUDIO:
        if (context->ringbuffer==nullptr)  // ringbuffer is not created that means client is not created too
        {
            // Init backend
            if (context->rx_streamname[0]==0) strncpy(context->rx_streamname, vban_packet->header.streamname, VBAN_STREAM_NAME_SIZE);
            if (context->nboutputs==0) context->nboutputs = vban_packet->header.format_nbc + 1;
            if ((ip_in!=0)&&(port_in!=0)) fprintf(stderr, "Getting incoming stream from %d.%d.%d.%d:%d\r\n", ((uint8_t*)&ip_in)[0], ((uint8_t*)&ip_in)[1], ((uint8_t*)&ip_in)[2], ((uint8_t*)&ip_in)[3], port_in);
            // Let main loop continue
            if (pthread_mutex_trylock(&context->rxmutex.threadlock)==0)
            {
                pthread_cond_signal(&context->rxmutex.dataready);
                pthread_mutex_unlock(&context->rxmutex.threadlock);
            }
            return 1;
        }
        if ((strncmp(vban_packet->header.streamname, context->rx_streamname, VBAN_STREAM_NAME_SIZE)==0)&& // stream name matches
            (vban_packet->header.format_SR  == vban_get_format_SR(context->samplerate))&& // will be deprecated after resampler
            (vban_packet->header.nuFrame    != context->nu_frame)&& // number of packet is not same
            (context->iprx==ip_in))
        {
            nbc = ((vban_packet->header.format_nbc + 1) < context->nboutputs ? (vban_packet->header.format_nbc + 1) : context->nboutputs);
            framesize = (vban_packet->header.format_nbc + 1)*VBanBitResolutionSize[vban_packet->header.format_bit];
            srcptr = vban_packet->data;
            for (int frame=0; frame<=vban_packet->header.format_nbs; frame++)
            {
                vban_sample_convert(fsamples, VBAN_BITFMT_32_FLOAT, srcptr, vban_packet->header.format_bit, nbc);
                if (ringbuffer_write_space(context->ringbuffer)>=context->nboutputs*sizeof(float)) ringbuffer_write(context->ringbuffer, (char*)fsamples, context->nboutputs*sizeof(float));
                srcptr+= framesize;
            }
            context->nu_frame = vban_packet->header.nuFrame;
            return 0;
        }
        break;
    case VBAN_PROTOCOL_SERIAL:
        if ((vban_packet->header.vban==VBAN_HEADER_FOURC)&&
            (vban_packet->header.format_SR==0x2E)&&
            ((strcmp(vban_packet->header.streamname, context->rx_streamname)==0)||
            (strcmp(vban_packet->header.streamname, "MIDI1")==0)))
        {
            if (context->ringbuffer_midi!=0)
            {
                eventptr = 0;
                while((eventptr+VBAN_HEADER_SIZE)<packetlen)
                {
                    if (ringbuffer_write_space(context->ringbuffer_midi)>=3)
                    {
                        ringbuffer_write(context->ringbuffer_midi, &vban_packet->data[eventptr], 3);
                        eventptr+= 3;
                    }
                }
            }
        }
        break;
    case VBAN_PROTOCOL_TXT:
        if ((memcmp(vban_packet->data, "/info", 5)==0)||(memcmp(vban_packet->data, "/INFO", 5)==0))
        {
            fprintf(stderr, "Info request from %d.%d.%d.%d:%d\n", ((uint8_t*)&ip_in)[0], ((uint8_t*)&ip_in)[1], ((uint8_t*)&ip_in)[2], ((uint8_t*)&ip_in)[3], port_in);
            if (pthread_mutex_trylock(&context->rxmutex.threadlock)==0)
            {
                pthread_cond_signal(&context->rxmutex.dataready);
                pthread_mutex_unlock(&context->rxmutex.threadlock);
                context->flags|= CMD_PRESENT;
            }
        }
        break;
    case VBAN_PROTOCOL_USER:
        break;
    default:
        break;
    }
    return 0;
}


void* timerThread(void* arg);
void* rxThread(void* arg);


void vban_fill_receptor_info(vban_stream_context_t* context);

#endif
