This is a pair of multistream vban_receptors for pipewire and jack on Linux
(the jack version may be simply reworked for Mac OS)

To build just run build.sh script for every one.

 
 usage: vban_receptor_jack <args>

-m - multistream mode on
-i - ip address or pipe name (default ip=0, default pipename - stdin
-p - ip port (if 0 - pipe mode)
-s - Stream/Receptor name, up to 16 symbols
-c - number of channels/clients
-n - redundancy 1 to 10 ("net quality")
-d - device mode for jack ports
-e - enable frame plucking
-h - show this help


usage: vban_receptor_pw <args>

-m - multistream mode on
-i - ip address or pipe name (default ip=0, default pipename - stdin
-p - ip port (if 0 - pipe mode)
-s - Stream/Receptor name, up to 16 symbols
-r - samplerate (default 48000)
-q - quantum, buffer size (Attention!!! Default is 128!!! Made for musicians.)
-c - number of channels/clients
-n - redundancy 1 to 10 ("net quality")
-d - device mode for pipewire ports
-e - enable frame plucking
-h - show this help


Example:
vban_receptor_pw -m1 -p6980 -sFOH -q128 -c8 -n2 -e1
-m1 - multistream mode on
-p6980 - listen to port 6980 (classic VBAN default)
-sFOH - receptor name FOH when scanning by emitters
-q128 - buffer size 128
-c8 - max 8 incoming streams
-n2 - input redundancy 2 (like net quality fast)
-e1 - enable frame plucking

Frame plucking is an experimental feature to prevent latency increase when other size's system clock is littlebit faster than receptor's.
When input ringbuffer is filled more than 3/4 PLUCKING mode switches ON and on every callback we pluck one frame, it is unnoticed by ear.
