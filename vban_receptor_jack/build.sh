#!/bin/bash
g++ -Wall main.cpp ../vban_common/vban_functions.cpp ../vban_common/udp.cpp ../vban_common/jack_backend.cpp ../vban_common/vban_client_list.cpp -ljack -o3 -o vban_receptor_jack
