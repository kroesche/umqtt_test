
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"
#include "umqtt_mocks.h"
#include "umqtt_wrapper.h"

// TODO: put const on constants and where it makes sense

TEST_GROUP(Publish);

static umqtt_Handle_t h = NULL;

static void *instBuf = NULL;
#define SIZE_INSTBUF 200
static uint8_t *pktBuf = NULL;
#define SIZE_PKTBUF 1024
static char *topicBuf = NULL;
#define SIZE_TOPICBUF 256

TEST_SETUP(Publish)
{
    // allocate memory for the instance that will be created
    // init the instance so it will be ready for all tests
    mock_malloc_Reset();
    instBuf = malloc(SIZE_INSTBUF);
    mock_hNet = &mock_hNet;
    transportConfig.hNet = mock_hNet;
    mock_malloc_shouldReturn[0] = instBuf;
    h = umqtt_New(&transportConfig, NULL, NULL);
    TEST_ASSERT_NOT_NULL(h);
    // clear all mocks
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetRead_Reset();
    mock_NetWrite_Reset();
    // ready a buffer that can be used for packet allocation
    pktBuf = malloc(SIZE_PKTBUF);
    TEST_ASSERT_NOT_NULL(pktBuf);
    topicBuf = malloc(SIZE_TOPICBUF);
    TEST_ASSERT_NOT_NULL(topicBuf);
    memset(topicBuf, 'A', SIZE_TOPICBUF);
    // fake an existing connection
    wrap_setConnected(h, true);
}

TEST_TEAR_DOWN(Publish)
{
    if (instBuf) { free(instBuf); instBuf = NULL; }
    if (pktBuf) { free(pktBuf); pktBuf = NULL; }
    if (topicBuf) { free(topicBuf); topicBuf = NULL; }
    h = NULL;
}

TEST(Publish, NullParms)
{
    umqtt_Error_t err;
    err = umqtt_Publish(NULL, NULL, NULL, 0, 0, false, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    err = umqtt_Publish(h, NULL, NULL, 0, 0, false, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
}

TEST(Publish, CalcLength)
{
    umqtt_Error_t err;
    uint16_t expectedLen;
    char *topic;

    // base value for expected len.  every packet should have this
    // 1 byte fix header, 4 bytes remaining length (we allow the max of 4)
    // 2 bytes topic len, internal packet overhead
    expectedLen = 1 + 4 + 2 + sizeof(PktBuf_t);

    // topic only
    // check that length is calculated correctly
    // since malloc fails, it will not try to send a packet but we
    // can verify the requested size
    topic = "topic";
    expectedLen += strlen(topic);
    err = umqtt_Publish(h, topic, NULL, 0, 0, false, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_EQUAL(expectedLen, mock_malloc_in_size);

    // topic and payload
    mock_malloc_Reset();
    uint8_t *message = (uint8_t *)"message";
    uint32_t messageLen = strlen((const char *)message);
    expectedLen += 2 + 7;
    err = umqtt_Publish(h, topic, message, messageLen, 0, false, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_EQUAL(expectedLen, mock_malloc_in_size);

    // add qos which causes packetId field to be added
    mock_malloc_Reset();
    expectedLen += 2;
    err = umqtt_Publish(h, topic, message, messageLen, 1, false, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_EQUAL(expectedLen, mock_malloc_in_size);
}

// set up for encoded length test
// @param remLen is the desired remaining length value
// inputs will be adjusted to achieve desire rem len
static const uint8_t *
EncodedTest(unsigned int remLen)
{
    // compute how long topic ID should be to get desire rem length
    unsigned int topicLen = remLen - 2;
    // null terminate the client ID string
    topicBuf[topicLen] = 0;

    mock_malloc_shouldReturn[0] = pktBuf;
    umqtt_Error_t err = umqtt_Publish(h, topicBuf, NULL, 0, 0, false, NULL);

    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);

    // return the buffer that was passed to NetWrite
    // it is the mqtt packet and caller will verify rem length
    // was correctly encoded
    return mock_NetWrite_in_pBuf;
}

TEST(Publish, EncodedLengthOneByte)
{
    const uint8_t *netBuf = EncodedTest(67);
    TEST_ASSERT_EQUAL(67, netBuf[1]);
}

TEST(Publish, EncodedLengthOneByteMax)
{
    const uint8_t *netBuf = EncodedTest(127);
    TEST_ASSERT_EQUAL(127, netBuf[1]);
}

TEST(Publish, EncodedLengthTwoByteMin)
{
    const uint8_t *netBuf = EncodedTest(128);
    TEST_ASSERT_EQUAL(0x80, netBuf[1]);
    TEST_ASSERT_EQUAL(1, netBuf[2]);
}

TEST(Publish, EncodedLengthTwoByte)
{
    const uint8_t *netBuf = EncodedTest(141);
    TEST_ASSERT_EQUAL(0x8D, netBuf[1]);
    TEST_ASSERT_EQUAL(1, netBuf[2]);
}

static uint8_t publishPacket0[] =
{
    0x30, 7, // fixed header
    0, 5, 't','o','p','i','c',
};

static uint8_t publishPacket1[] =
{
    0x33, 18, // qos 1, retain
    0, 5, 't','o','p','i','c',
    0, 1, // packet id
    0, 7, 'm','e','s','s','a','g','e',
};

TEST(Publish, Basic)
{
    umqtt_Error_t err;
    uint16_t msgId = 5555;

    unsigned int pktSize = sizeof(publishPacket0);
    mock_NetWrite_shouldReturn = pktSize;
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Publish(h, "topic", NULL, 0, 0, false, &msgId);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // since qos was 0, message Id should be 0
    TEST_ASSERT_EQUAL(0, msgId);

    // verify expected allocations/free
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_TRUE(mock_free_wasCalled);
    TEST_ASSERT_EQUAL_PTR(pktBuf, mock_free_in_ptr);

    // verify net write was called and with expected size
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(pktSize, mock_NetWrite_in_len);
    TEST_ASSERT_FALSE(mock_NetWrite_in_isMore);

    // check the packet contents
    TEST_ASSERT_EQUAL_MEMORY(publishPacket0, mock_NetWrite_in_pBuf, pktSize);
}

TEST(Publish, Features)
{
    umqtt_Error_t err;
    uint16_t msgId = 5555;

    unsigned int pktSize = sizeof(publishPacket1);
    mock_NetWrite_shouldReturn = pktSize;
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Publish(h, "topic", (uint8_t *)"message", strlen("message"), 1, true, &msgId);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify message ID non zero
    TEST_ASSERT_NOT_EQUAL(0, msgId);
    TEST_ASSERT_NOT_EQUAL(5555, msgId);

    // verify expected allocations/free
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_FALSE(mock_free_wasCalled);

    // verify net write was called and with expected size
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(pktSize, mock_NetWrite_in_len);
    TEST_ASSERT_FALSE(mock_NetWrite_in_isMore);

    // check the packet contents
    TEST_ASSERT_EQUAL_MEMORY(publishPacket1, mock_NetWrite_in_pBuf, pktSize);
}

TEST_GROUP_RUNNER(Publish)
{
    RUN_TEST_CASE(Publish, NullParms);
    RUN_TEST_CASE(Publish, CalcLength);
    RUN_TEST_CASE(Publish, EncodedLengthOneByte);
    RUN_TEST_CASE(Publish, EncodedLengthOneByteMax);
    RUN_TEST_CASE(Publish, EncodedLengthTwoByteMin);
    RUN_TEST_CASE(Publish, EncodedLengthTwoByte);
    RUN_TEST_CASE(Publish, Basic);
    RUN_TEST_CASE(Publish, Features);
}
