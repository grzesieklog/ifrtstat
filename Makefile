#!/bin/make

GMP=`pkg-config --cflags --libs gmp`
NETLINK=`pkg-config --cflags --libs libnl-3.0 libnl-route-3.0`

default: ifrtstat

ifrtstat:
	gcc ifrtstat.c -o ifrtstat $(NETLINK) $(GMP) -D OVERFLOW

test_nl: 
	gcc test_nl.c -o test_nl $(NETLINK)
test_over: 
	gcc test_over.c -o test_over $(GMP)

clean:
	rm ifrtstat
clean_nl:
	rm test_nl
clean_over:
	rm test_over
