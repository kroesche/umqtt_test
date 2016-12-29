
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"

static void
RunAllTests(void)
{
    RUN_TEST_GROUP(PacketHandling);
    RUN_TEST_GROUP(Instance);
    RUN_TEST_GROUP(Connect);
    RUN_TEST_GROUP(Publish);
    RUN_TEST_GROUP(Subscribe);
    RUN_TEST_GROUP(Unsubscribe);
    RUN_TEST_GROUP(Decode);
    RUN_TEST_GROUP(Run);
}

int
main(int argc, const char *argv[])
{
    return UnityMain(argc, argv, RunAllTests);
}
