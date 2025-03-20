#!/bin/bash
g++ -Wall main.cpp ../vban_common/vban_functions.cpp ../vban_common/udp.cpp ../vban_common/pipewire_backend.cpp ../vban_common/vban_client_list.cpp -o3 -o vban_receptor_pw $(pkg-config --cflags --libs libpipewire-0.3)
