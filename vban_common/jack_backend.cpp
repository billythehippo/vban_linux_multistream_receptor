#include "jack_backend.h"


void help_receptor(void)
{
    fprintf(stderr, "VBAN Jack Audio Connection Kit receptor for network and pipes/fifos\n\nBy Billy the Hippo\n\nusage: vban_receptor_jack <args>\r\n\n");
    fprintf(stderr, "-m - multistream mode on\r\n");
    fprintf(stderr, "-i - ip address or pipe name (default ip=0, default pipename - stdin\r\n");
    fprintf(stderr, "-p - ip port (if 0 - pipe mode)\r\n");
    fprintf(stderr, "-s - Stream/Receptor name, up to 16 symbols\r\n");
    //fprintf(stderr, "-r - samplerate (default 48000)\r\n");
    //fprintf(stderr, "-q - quantum, buffer size (reserved)\r\n"); //Attention!!! Default is 128!!! Made for musicians.
    fprintf(stderr, "-c - number of channels/clients\r\n");
    fprintf(stderr, "-n - redundancy 1 to 10 (\"net quality\")\r\n");
    fprintf(stderr, "-d - device mode for jack ports\r\n");
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
            {"jackservname",required_argument,  0, 'j'},
            {"help",        no_argument,        0, 'h'},
            {0,             0,                  0,  0 }
        };

    c = getopt_long(argc, argv, "m:i:p:s:r:q:c:n:d:f:e:j:h", options, &index);
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
        case 'j':
            stream->jack_server_name = (char*)malloc(strlen(optarg));
            strncpy(stream->jack_server_name, optarg, strlen(optarg));
            break;
        case 'h':
            help_receptor();
            return 1;
        default:
            fprintf(stderr, "Unrecognized parameter -%c", c);
            break;
        }
        c = getopt_long(argc, argv, "m:i:p:s:r:q:c:n:d:f:e:j:h", options, &index);
    }
    return 0;
}


int jack_init_rx_stream(jack_stream_data_t* client)
{
    vban_stream_context_t* stream = (vban_stream_context_t*)client->user_data;
    int index;
    char name[32];
    unsigned long port_flags = JackPortIsOutput;

    if ((stream->flags&DEVICE_MODE)!=0) port_flags|= JackPortIsPhysical;

    client->client = jack_client_open(stream->rx_streamname, client->options, &client->status, stream->jack_server_name);
    if (client == NULL)
    {
        fprintf(stderr, "Failed to create JACK client!\n");
        return 1;
    }

    stream->samplerate = jack_get_sample_rate(client->client);
    stream->nframes = jack_get_buffer_size(client->client);

    if (jack_set_process_callback(client->client, on_rx_process, (void*)client))
    {
        fprintf(stderr, "Failed to set JACK audio process callback!\n");
        return 1;
    }
    // if (jack_set_midi_event_callback(client, on_rx_midi_process, (void*)client))
    // {
    //     fprintf(stderr, "Failed to set JACK midi process callback!\n");
    //     return 1;
    // }
    client->output_ports = (jack_port_t**)malloc(sizeof(jack_port_t*)*(stream->nboutputs + 1));
    for (index=0; index<stream->nboutputs; index++)
    {
        memset(name, 0, 32);
        sprintf(name, "Input_%u", (index+1));
        client->output_ports[index] = jack_port_register(client->client, name, JACK_DEFAULT_AUDIO_TYPE, port_flags, 0);
    }
    memset(name, 0, 32);
    sprintf(name, "Input_MIDI");
    client->output_ports[index] = jack_port_register(client->client, name, JACK_DEFAULT_MIDI_TYPE, port_flags, 0);

    return 0;
}


int jack_connect_rx_stream(jack_stream_data_t* client)
{
    vban_stream_context_t* stream = (vban_stream_context_t*)client->user_data;

    if (stream->flags&AUTOCONNECT)
    {
        ;
    }
    return 0;
}


int on_rx_process(jack_nframes_t nframes, void *arg)
{
    jack_stream_data_t *data = (jack_stream_data_t*)arg;
    vban_stream_context_t* context = (vban_stream_context_t*)data->user_data;
    static jack_default_audio_sample_t* buffers[VBAN_CHANNELS_MAX_NB];
    static jack_default_audio_sample_t* midibuf;
    unsigned char* evbuffer;
    size_t bufreadspace;
    uint16_t frame;
    uint16_t channel = 0;
    uint16_t lost = 0;
    int framesize = sizeof(float)*context->nboutputs;

    if (nframes!=context->nframes)
    {
        context->nframes = nframes;
        vban_compute_rx_buffer(nframes, context->nboutputs, &context->rxbuf, &context->rxbuflen);
        vban_compute_rx_ringbuffer(nframes, context->vban_nframes_pac, context->nboutputs, context->redundancy, &context->ringbuffer);
        fprintf(stderr, "Warning: buffer size is changed to %d!\n", nframes);
    }

    if (context->flags&PLUCKING_EN)
    {
        bufreadspace = ringbuffer_read_space(context->ringbuffer);
        if (bufreadspace>(context->ringbuffer->size*3/4)) context->flags|= PLUCKING_ON;
        else if (bufreadspace<(context->ringbuffer->size*1/2)) context->flags&=~PLUCKING_ON;
    }

    for (channel = 0; channel < context->nboutputs; channel++)
        buffers[channel] = (jack_default_audio_sample_t*)jack_port_get_buffer(data->output_ports[channel], nframes);
    midibuf = (jack_default_audio_sample_t*)jack_port_get_buffer(data->output_ports[context->nboutputs], nframes);

    for (frame=0; frame<nframes; frame++)
    {
        if (ringbuffer_read_space(context->ringbuffer)<framesize)
        {
            lost++;
            context->flags&=~PLUCKING_ON;
        }
        else
        {
            vban_read_frame_from_ringbuffer(context->rxbuf, context->ringbuffer, context->nboutputs);
            if ((frame==(nframes-1))&&(context->flags&PLUCKING_ON)) vban_add_frame_from_ringbuffer(context->rxbuf, context->ringbuffer, context->nboutputs);
            for (channel=0; channel<context->nboutputs; channel++) buffers[channel][frame] = context->rxbuf[channel];
        }
    }
    if (lost)
    {
        if (context->lost_pac_cnt<9) fprintf(stderr, "%d samples lost\n", lost);
        if (lost==nframes)
        {
            if (context->lost_pac_cnt<10) context->lost_pac_cnt++;
            if (context->lost_pac_cnt>=9)
            {
                for (channel = 0; channel < context->nboutputs; channel++)
                    for (frame=0; frame<nframes; frame++)
                        buffers[channel][frame] = 0;
            }
        }
    }
    jack_midi_clear_buffer(midibuf);
    while(ringbuffer_read_space(context->ringbuffer_midi)>=3)
    {
        evbuffer = jack_midi_event_reserve(midibuf, 0, 3);
        ringbuffer_read(context->ringbuffer_midi, (char*)evbuffer, 3);
    }

    return 0;
}
