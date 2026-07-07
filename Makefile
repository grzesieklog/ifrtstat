#!/bin/make

NETLINK=`pkg-config --cflags --libs libnl-3.0 libnl-route-3.0`

default: main

main:
	gcc main.c -o ifrtstat $(NETLINK)


clean:
	rm -v ifrtstat
