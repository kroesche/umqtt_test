EXE:=umqtt_compliance_test

CFLAGS:=-g -Wall -O0 -DUNITY_EXCLUDE_FLOAT -I../ -I../Unity/src -I../Unity/extras/fixture/src

SRCS=$(EXE).c
SRCS+=test_connect.c
SRCS+=../umqtt/umqtt.c ../Unity/src/unity.c ../Unity/extras/fixture/src/unity_fixture.c

all: $(EXE)

$(EXE): $(SRCS)
	gcc $(CFLAGS) $^ -o $@

clean:
	rm -f *.o $(EXE)

.PHONY: all clean
