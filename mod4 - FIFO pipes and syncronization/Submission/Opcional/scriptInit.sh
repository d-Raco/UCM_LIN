#!/bin/bash
make
sudo insmod fifodev.ko
sudo mknod -m 0666 /dev/fifodev c 248 0

