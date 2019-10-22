#!/bin/sh

# You can do this
#tools/toolchains/esp8266-linux-x86_64-20181106.0/bin/xtensa-lx106-elf-objdump -D app/modules/.output/eagle/debug/obj/wifi.o

# But ScratchABit resolves references etc, so
cp app/modules/.output/eagle/debug/obj/wifi.o .
python /home/pieter/software/ScratchABit/ScratchABit.py ./esp8266.def
