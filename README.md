# Bisqwit’s CRT filter

This is the CRT filter that I used in my ”What is That Editor” video,
at https://www.youtube.com/watch?v=ZMBQmhO8KqI.

It received some accolades, but I forgot to publish it.
Here it is finally.

## To build

Run this command to build the filter:

    g++ -o crt-filter crt-filter.cc -fopenmp -Ofast -march=native -Wall -Wextra

## Usage

You can find some example usage in make-reencoded.sh.

The filter takes BGRA (RGB32) video from stdin,
and produces BGRA video into stdout.

The filter takes five commandline parameters:

    ./crt-filter <sourcewidth> <sourceheight> <outputwidth> <outputheight> <scanlines>

the sourcewidth and sourceheight denote the size of the original video.
The outputwidth and outputheight denote the size that you want to produce.
Generally speaking you want to produce as high quality as possible.
Vertical resolution is more important than horizontal resolution.

Scanlines is the number of scanlines you wish to simulate.
Generally that would be the same as the vertical resolution of the source video,
but that is not a requirement.

The number of horizontal pixels simulated is hard-coded at 640.

## Screenshots

![Original1](img/mpv-shot0001.jpg)
![Filtered1](img/mpv-shot0002.jpg)

![Original2](img/mpv-shot0003.jpg)
![Filtered2](img/mpv-shot0004.jpg)
