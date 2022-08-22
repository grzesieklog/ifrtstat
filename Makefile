#!/bin/make

GMP=`pkg-config --cflags --libs gmp`
NETLINK=`pkg-config --cflags --libs libnl-3.0 libnl-route-3.0`

default: ifrtstat

ifrtstat:
	gcc ifrtstat.c -o ifrtstat $(NETLINK) $(GMP)

test_nl: 
	gcc test_nl.c -o test_nl $(NETLINK)

clean:
	rm ifrtstat
	rm test_nl
