#include "pipewire_backend.h"

mutexcond_t rx_run_mutex;


enum spa_audio_format format_vban_to_spa(enum VBanBitResolution format_vban)
{
    switch (format_vban)
    {
    case VBAN_BITFMT_8_INT:
        return SPA_AUDIO_FORMAT_S8;
    case VBAN_BITFMT_16_INT:
        return SPA_AUDIO_FORMAT_S16_LE;
    case VBAN_BITFMT_24_INT:
        return SPA_AUDIO_FORMAT_S24_LE;
    case VBAN_BITFMT_32_INT:
        return SPA_AUDIO_FORMAT_S32_LE;
    case VBAN_BITFMT_32_FLOAT:
        return SPA_AUDIO_FORMAT_F32_LE;
    case VBAN_BITFMT_64_FLOAT:
        return SPA_AUDIO_FORMAT_F64_LE;
    default:
        return SPA_AUDIO_FORMAT_F32_LE;
    }
}


void help_receptor(void)
{
    fprintf(stderr, "VBAN Pipewire receptor for network and pipes/fifos\n\nBy Billy the Hippo\n\nusage: vban_receptor_pw <args>\r\n\n");
    fprintf(stderr, "-m - multistream mode on\r\n");
    fprintf(stderr, "-i - ip address or pipe name (default ip=0, default pipename - stdin\r\n");
    fprintf(stderr, "-p - ip port (if 0 - pipe mode)\r\n");
    fprintf(stderr, "-s - Stream/Receptor name, up to 16 symbols\r\n");
    fprintf(stderr, "-r - samplerate (default 48000)\r\n");
    fprintf(stderr, "-q - quantum, buffer size (Attention!!! Default is 128!!! Made for musicians.)\r\n");
    fprintf(stderr, "-c - number of channels/clients\r\n");
    fprintf(stderr, "-n - redundancy 1 to 10 (\"net quality\")\r\n");
    fprintf(stderr, "-d - device mode for pipewire ports\r\n");
    //fprintf(stderr, "-f - format: 16, 24, 32f\r\n");
    fprintf(stderr, "-e - enable frame plucking\r\n");
    fprintf(stderr, "-h - show this help\r\n");
    exit(0);
}


int get_receptor_options(vban_stream_context_t* stream, int argc, char *argv[])
{
    int index = 0;
    char c;
    static const struct option options[] =
        {
            {"multistream", required_argument,  0, 'm'},
            {"ipaddr",      required_argument,  0, 'i'},
            {"port",        required_argument,  0, 'p'},
            {"streamname",  required_argument,  0, 's'},
            {"samplerate",  required_argument,  0, 'r'},
            {"bufsize",     required_argument,  0, 'q'},
            {"nbchannels",  required_argument,  0, 'c'},
            {"redundancy",  required_argument,  0, 'n'},
            {"device",      required_argument,  0, 'd'},
            //{"format",      required_argument,  0, 'f'},
            {"plucking",    required_argument,  0, 'e'},
            {"help",        no_argument,        0, 'h'},
            {0,             0,                  0,  0 }
        };

    c = getopt_long(argc, argv, "m:i:p:s:r:q:c:n:d:f:e:h", options, &index);
    if (c==-1) c = 'h';

    while(c!=-1)
    {
        switch (c)
        {
        case 'm': // multistream mode
            if ((optarg[0]!='0')&&(optarg[0]!='n')&&(optarg[0]!='N')) stream->flags|= MULTISTREAM;
            else stream->flags&=~MULTISTREAM;
            break;
        case 'i': // ip addr to filter / input pipe name
            if (stream->rxport==0) // PIPE mode
            {
                memset(stream->pipename, 0, 32);
                strncpy(stream->pipename, optarg, (strlen(optarg)>32 ? 32 : strlen(optarg)));
                stream->iprx = 0;
            }
            else // UDP mode
            {
                if (strlen(optarg)>15)
                {
                    fprintf(stderr, "Wrong IP address!!!\r\n");
                    return 1;
                }
                stream->iprx = inet_addr(optarg);
            }
            break;
        case 'p': // RX port to listen
            stream->rxport = atoi(optarg);
            break;
        case 's': // Streamname (in multistream mode - receptor name)
            memset(stream->rx_streamname, 0, VBAN_STREAM_NAME_SIZE);
            memcpy(stream->rx_streamname, optarg, (strlen(optarg)>VBAN_STREAM_NAME_SIZE ? VBAN_STREAM_NAME_SIZE : strlen(optarg)));
            break;
        case 'r': // Samplerate
            stream->samplerate = atoi(optarg);
            break;
        case 'q': // Quantum (buffer size)
            stream->nframes = atoi(optarg);
            break;
        case 'c': // Channel number / Clients number
            stream->nboutputs = atoi(optarg);
            break;
        case 'n': // Redundancy (Network Quality)
            stream->redundancy = atoi(optarg);
            break;
        case 'd': // Device mode in graph
            if ((optarg[0]!='0')&&(optarg[0]!='n')&&(optarg[0]!='N')) stream->flags|= DEVICE_MODE;
            else stream->flags&=~DEVICE_MODE;
            break;
        case 'f':
            break;
        case 'e':
            if ((optarg[0]!='0')&&(optarg[0]!='n')&&(optarg[0]!='N')) stream->flags|= PLUCKING_EN;
            else stream->flags&=~PLUCKING_EN;
            break;
        case 'h':
            help_receptor();
            return 1;
        default:
            fprintf(stderr, "Unrecognized parameter -%c", c);
            break;
        }
        c = getopt_long(argc, argv, "m:i:p:s:r:q:c:n:d:f:e:h", options, &index);
    }
    return 0;
}



static void on_rx_process(void *userdata)
{
    pw_stream_data_t *data = (pw_stream_data_t*)userdata;
    vban_stream_context_t* context = (vban_stream_context_t*)data->user_data;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    int stride, n_frames;
    char* samples_ptr;
    size_t bufreadspace;
    uint16_t frame;
    uint16_t lost = 0;

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL)
    {
        pw_log_warn("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    samples_ptr = (char*)(b->buffer->datas[0].data);
    if (samples_ptr == NULL)
        return;

    stride = (context->nboutputs)*sizeof(float); //sizeof(float) * DEFAULT_CHANNELS;
    if (stride==0)
        return;

    n_frames = buf->datas[0].maxsize / stride;
    //if (b->requested) n_frames = SPA_MIN((int)b->requested, n_frames);

    if (n_frames!=context->nframes)
    {
        context->nframes = n_frames;
        vban_compute_rx_buffer(n_frames, context->nboutputs, &context->rxbuf, &context->rxbuflen);
        vban_compute_rx_ringbuffer(n_frames, context->vban_nframes_pac, context->nboutputs, context->redundancy, &context->ringbuffer);
        fprintf(stderr, "Warning: buffer size is changed to %d!\n", n_frames);
    }

    if (context->flags&PLUCKING_EN)
    {
        bufreadspace = ringbuffer_read_space(context->ringbuffer);
        if (bufreadspace>(context->ringbuffer->size*3/4)) context->flags|= PLUCKING_ON;
        else if (bufreadspace<(context->ringbuffer->size*1/2)) context->flags&=~PLUCKING_ON;
    }

    for (frame=0; frame<n_frames; frame++)
    {
        if (vban_read_frame_from_ringbuffer(&context->rxbuf[frame*context->nboutputs], context->ringbuffer, context->nboutputs))
        {
            lost++;
            if (frame!=0) memcpy(&context->rxbuf[frame*context->nboutputs], &context->rxbuf[(frame - 1)*context->nboutputs], sizeof(float)*context->nboutputs);
        }
    }

    if (lost==0)
    {
        context->lost_pac_cnt = 0;
        if ((context->flags&(PLUCKING_EN|PLUCKING_ON))!=0)
        {
            vban_add_frame_from_ringbuffer(&context->rxbuf[(n_frames - 1)*context->nboutputs], context->ringbuffer, context->nboutputs);
//            fprintf(stderr, "%d bytes in buffer. Sample added!\r\n",bufreadspace );
        }
    }
    else
    {
        if (context->lost_pac_cnt<9) fprintf(stderr, "%d samples lost\n", lost);
        if (lost==n_frames)
        {
            if (context->lost_pac_cnt<10) context->lost_pac_cnt++;
            if (context->lost_pac_cnt==9)
                memset(context->rxbuf, 0, context->rxbuflen*sizeof(float));
        }
    }
    if (context->lost_pac_cnt<9) memcpy(samples_ptr, (char*)context->rxbuf, context->rxbuflen*sizeof(float));
    else memset(samples_ptr, 0, context->rxbuflen*sizeof(float));

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = n_frames * stride;//*/

    pw_stream_queue_buffer(data->stream, b);
}


static void do_quit(void *userdata, int signal_number)
{
    pw_stream_data_t *data = (pw_stream_data_t*)userdata;
    pw_main_loop_quit(data->loop);
}


void* pw_rx_run_thread_handler(void* arg)
{
    pw_stream_data_t* data = (pw_stream_data_t*)arg;
    // GOOOOOO!!!!!!1111
    pw_main_loop_run(data->loop);

    fprintf(stderr, "STOP!!!\n");
    pw_stream_destroy(data->stream);
    pw_main_loop_destroy(data->loop);
    pw_deinit();
    fprintf(stderr, "Stop thread!\r\n");
    return NULL;
}


int pw_init_rx_stream(pw_stream_data_t *data, struct pw_stream_events *stream_events, vban_stream_context_t* vban_stream, spa_audio_format format)
{
    // struct data data = { 0, };
    const struct spa_pod *params[1];
    uint8_t buffer[16384];
    struct pw_properties *props;
    struct spa_pod_builder b;
    struct spa_audio_info_raw audio_info;
    char tmpstring[16];

    // INIT PIPEWIRE NODE
    spa_pod_builder_init(&b, buffer, sizeof(buffer));
    pw_init(NULL, NULL); //(&argc, &argv);

    data->loop = pw_main_loop_new(NULL);

    pw_loop_add_signal(pw_main_loop_get_loop(data->loop), SIGINT, do_quit, data);
    pw_loop_add_signal(pw_main_loop_get_loop(data->loop), SIGTERM, do_quit, data);

    sprintf(tmpstring, "%d/%d", vban_stream->nframes, vban_stream->samplerate);
    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                              PW_KEY_MEDIA_CATEGORY, "Playback",
                              PW_KEY_MEDIA_ROLE, "Music",
                              PW_KEY_NODE_LATENCY, tmpstring, // nframes/samplerate
                              PW_KEY_NODE_NAME, vban_stream->rx_streamname,
                              NULL);
    fprintf(stderr, "PIPEWIRE_LATENCY=%s\n", tmpstring);
    //if (argc > 1) pw_properties_set(props, PW_KEY_TARGET_OBJECT, argv[1]);

    stream_events->version = PW_VERSION_STREAM_EVENTS;
    stream_events->process = on_rx_process;

    data->stream = pw_stream_new_simple(
        pw_main_loop_get_loop(data->loop),
        NULL,
        props,
        stream_events,
        data);

    audio_info.channels = vban_stream->nboutputs;
    audio_info.format = format;//format_vban_to_spa((enum VBanBitResolution)config.format_vban);
    //audio_info.rate = config.samplerate;
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    pw_stream_connect(data->stream,
                      PW_DIRECTION_OUTPUT,
                      PW_ID_ANY,
                      //PW_STREAM_FLAG_AUTOCONNECT | //(1 << 0)
                      //PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS,
                      (pw_stream_flags)((1 << 2)|(1 << 4)),
                      params, 1);

    return 0;
}


int pw_run_rx_stream(pw_stream_data_t *data, mutexcond_t* mutex)
{
    pthread_attr_init(&mutex->attr);
    pthread_create(&mutex->tid, &mutex->attr, pw_rx_run_thread_handler, (void*)data);

    return 0;
}


int pw_stop_rx_stream(pw_stream_data_t *data, mutexcond_t* mutex)
{
    pw_main_loop_quit(data->loop);
    pthread_join(mutex->tid, NULL);

    return 0;
}
