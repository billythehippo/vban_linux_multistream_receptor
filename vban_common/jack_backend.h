#ifndef JACK_BACKEND_H
#define JACK_BACKEND_H

#include <jack/jack.h>
#include <jack/midiport.h>

#include "vban_functions.h"

typedef struct jack_stream_data_t
{
    jack_client_t* client;
    jack_options_t options = JackNullOption;
    jack_port_t** input_ports;
    jack_port_t** output_ports;
    jack_status_t status;
    void* user_data;
} jack_stream_data_t;


__always_inline int jack_run_rx_stream(jack_stream_data_t* client)
{
    int ret = jack_activate(client->client);
    if (ret) fprintf(stderr, "Cannot activate client\n");
    return ret;
}


__always_inline void jack_stop_rx_stream(jack_stream_data_t* client)
{
    jack_deactivate(client->client);
    jack_client_close(client->client);
    if (client->output_ports!=nullptr) free(client->output_ports);
}


void help_receptor(void);
int get_receptor_options(vban_stream_context_t* stream, int argc, char *argv[]);
int jack_init_rx_stream(jack_stream_data_t* client);
int jack_connect_rx_stream(jack_stream_data_t* client);
int on_rx_process(jack_nframes_t nframes, void *arg);

#endif // JACK_BACKEND_H
