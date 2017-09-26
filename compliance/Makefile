###############################################################################
#
# Makefile - Makefile for umqtt compliance test suite
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

EXE:=umqtt_compliance_test

CFLAGS:=-g -std=gnu99 -pedantic-errors -Wall -Wextra -Werror -O0 -DUNITY_EXCLUDE_FLOAT -I../ -I../Unity/src -I../Unity/extras/fixture/src

SRCS=$(EXE).c
SRCS+=test_connect.c
SRCS+=../umqtt/umqtt.c ../Unity/src/unity.c ../Unity/extras/fixture/src/unity_fixture.c

all: $(EXE)

$(EXE): $(SRCS)
	gcc $(CFLAGS) $^ -o $@

clean:
	rm -f *.o $(EXE)

.PHONY: alli clean

