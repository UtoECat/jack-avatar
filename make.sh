#!/bin/bash

rm -f galogen.o avatar.o spectrobar.o stb_impl.o

echo "COMPILING"
cc -c galogen.c avatar.c fft.c stb_impl.c -Wall -Wextra -I.

echo "LINKING"
cc avatar.o galogen.o fft.o stb_impl.o -o jackavatar -lX11 -lglfw -lGL -ljack -lm
