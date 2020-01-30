#!/bin/sh

if [ $# -ne 1 ]; then
    echo "Invalid number of arguments: flash_firmware_to_research.sh <username>"
    exit 1
fi

USER=$1

read -p "This will stop any running experiments. Are you sure? (y/N) " CONFIRM

if [ "$CONFIRM" = "y" ]; then
    echo "Let's go then."
    scp ./bin/0x00000.bin $USER@172.18.15.129:/tmp/0x00000.bin
    scp ./bin/0x10000.bin $USER@172.18.15.129:/tmp/0x10000.bin
    ssh $USER@research 'esptool.py --port /dev/ttyUSB0 write_flash -fm dio 0x00000 /tmp/0x00000.bin'
    ssh $USER@research 'esptool.py --port /dev/ttyUSB0 write_flash -fm dio 0x10000 /tmp/0x10000.bin'
else
    echo "Aborted."
fi
