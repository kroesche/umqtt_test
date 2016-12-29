
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"
#include "umqtt_mocks.h"

TEST_GROUP(Instance);

static void *allocBuf;

TEST_SETUP(Instance)
{
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetRead_Reset();
    mock_NetWrite_Reset();
    // the hNet field needs to be set up at run time
    mock_hNet = &mock_hNet;
    transportConfig.hNet = mock_hNet;
    allocBuf = malloc(256);
}

TEST_TEAR_DOWN(Instance)
{
    free(allocBuf);
}

TEST(Instance, InitNullInstance)
{
    umqtt_Handle_t h = umqtt_New(NULL, NULL, NULL);
    TEST_ASSERT_NULL(h);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
}

TEST(Instance, InitMallocFail)
{
    mock_malloc_shouldReturn[0] = NULL;
    umqtt_Handle_t h = umqtt_New(&transportConfig, NULL, NULL);
    TEST_ASSERT_NULL(h);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
}

TEST(Instance, InitNominal)
{
    mock_malloc_shouldReturn[0] = allocBuf;
    umqtt_Handle_t h = umqtt_New(&transportConfig, NULL, NULL);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_EQUAL_PTR(allocBuf, h);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    umqtt_Error_t err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_DISCONNECTED, err);
}

TEST(Instance, Delete)
{
    TEST_IGNORE();
}

TEST_GROUP_RUNNER(Instance)
{
    RUN_TEST_CASE(Instance, InitNullInstance);
    RUN_TEST_CASE(Instance, InitMallocFail);
    RUN_TEST_CASE(Instance, InitNominal);
    RUN_TEST_CASE(Instance, Delete);
}
