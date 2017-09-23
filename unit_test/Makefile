###############################################################################
#
# Makefile - Makefile for umqtt unit test suite
#
# Copyright (c) 2016, Joseph Kroesche (tronics.kroesche.io)
# All rights reserved.
#
# This software is released under the FreeBSD license, found in the
# accompanying file LICENSE.txt and at the following URL:
#      http://www.freebsd.org/copyright/freebsd-license.html
#
# This software is provided as-is and without warranty.
#
###############################################################################

EXE:=umqtt_unit_test
EXEDIR:=build

CFLAGS:=-g -std=c99 -pedantic-errors -Wall -Wextra -Werror -O0 -DUNITY_EXCLUDE_FLOAT -I../ -I../Unity/src -I../Unity/extras/fixture/src

SRCS=$(EXE).c
SRCS+=umqtt_mocks.c umqtt_instance_test.c umqtt_connect_test.c umqtt_publish_test.c
SRCS+=umqtt_subscribe_test.c umqtt_unsubscribe_test.c umqtt_packet_handling_test.c
SRCS+=umqtt_decode_test.c umqtt_run_test.c
SRCS+=umqtt_wrapper.c ../Unity/src/unity.c ../Unity/extras/fixture/src/unity_fixture.c
#SRCS+=../umqtt/umqtt.c ../Unity/src/unity.c ../Unity/extras/fixture/src/unity_fixture.c

all: $(EXEDIR)/$(EXE)

$(EXEDIR):
	mkdir $(EXEDIR)

$(EXEDIR)/$(EXE): $(EXEDIR) $(SRCS)
	gcc $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f *.o
	rm -rf $(EXEDIR)

.PHONY: all clean
