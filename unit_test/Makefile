EXE:=umqtt_unit_test

CFLAGS:=-g -Wall -O0 -DUNITY_EXCLUDE_FLOAT -I../ -I../../Unity/src -I../../Unity/extras/fixture/src

SRCS=$(EXE).c
SRCS+=umqtt_instance_test.c umqtt_connect_test.c umqtt_publish_test.c
SRCS+=umqtt_subscribe_test.c umqtt_unsubscribe_test.c umqtt_decode_test.c
SRCS+=../umqtt/umqtt.c ../../Unity/src/unity.c ../../Unity/extras/fixture/src/unity_fixture.c

all: $(EXE)

$(EXE): $(SRCS)
	gcc $(CFLAGS) $^ -o $@

clean:
	rm -f *.o $(EXE)

.PHONY: all clean
