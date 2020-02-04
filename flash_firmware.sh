#!/bin/bash

esptool.py -b 115200 --port /dev/ttyUSB0 write_flash -fm dio 0x00000 ./bin/0x00000.bin
esptool.py -b 115200 --port /dev/ttyUSB0 write_flash -fm dio 0x10000 ./bin/0x10000.bin
