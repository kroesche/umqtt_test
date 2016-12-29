
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"
#include "umqtt_mocks.h"
#include "umqtt_wrapper.h"

// TODO: need test case to verify does not connect if already connected
// or connect is pending

TEST_GROUP(Connect);

static umqtt_Handle_t h = NULL;

static void *instBuf = NULL;
#define SIZE_INSTBUF 200
static uint8_t *pktBuf = NULL;
#define SIZE_PKTBUF 1024
static char *clientBuf = NULL;
#define SIZE_CLIENTBUF 256

TEST_SETUP(Connect)
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
    clientBuf = malloc(SIZE_CLIENTBUF);
    TEST_ASSERT_NOT_NULL(clientBuf);
    memset(clientBuf, 'A', SIZE_CLIENTBUF); // used for dummy strings
}

TEST_TEAR_DOWN(Connect)
{
    if (instBuf) { free(instBuf); instBuf = NULL; }
    if (pktBuf) { free(pktBuf); pktBuf = NULL; }
    if (clientBuf) { free(clientBuf); clientBuf = NULL; }
    h = NULL;
}

TEST(Connect, NullParms)
{
    // call in various combinations that should cause parameter error
    umqtt_Error_t err;
    err = umqtt_Connect(NULL, false, false, 0, 0, NULL, NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    err = umqtt_Connect(h, false, false, 0, 0, NULL, NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
}

TEST(Connect, AllocFail)
{
    // call with valid parameters but simulate malloc failure
    umqtt_Error_t err;
    err = umqtt_Connect(h, false, false, 0, 0, "unit_test", NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
}

TEST(Connect, CalcLength)
{
    umqtt_Error_t err;
    uint16_t expectedLen;
    char *clientId;

    // base value for expected len.  every packet should have this
    // 1 byte fix header, 4 bytes remaining length (we allow the max of 4)
    // 10 bytes var header, 2 bytes topic len, internal packet overhead
    expectedLen = 1 + 4 + 10 + 2 + sizeof(PktBuf_t);

    // check that length is calculated correctly
    // since malloc fails, it will not try to send a packet but we
    // can verify the requested size
    clientId = "client";
    expectedLen += strlen(clientId);
    err = umqtt_Connect(h, false, false, 0, 0, clientId, NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_EQUAL(expectedLen, mock_malloc_in_size);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_DISCONNECTED, err);

    // add a will topic,  will topic without message is an error
    mock_malloc_Reset();
    char *willTopic = "topic";
    expectedLen += 2 + strlen(willTopic);
    err = umqtt_Connect(h, false, false, 0, 0, clientId, willTopic, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);

    // add a will message
    mock_malloc_Reset();
    uint8_t *willMessage = (uint8_t *)"message";
    uint32_t willMessageLen = strlen((const char *)willMessage);
    expectedLen += 2 + willMessageLen;
    err = umqtt_Connect(h, false, false, 0, 0, clientId, willTopic,
                        willMessage, willMessageLen, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_EQUAL(expectedLen, mock_malloc_in_size);

    // add a username
    mock_malloc_Reset();
    char *username = "username";
    expectedLen += 2 + strlen(username);
    err = umqtt_Connect(h, false, false, 0, 0, clientId, willTopic,
                        willMessage, willMessageLen, username, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_EQUAL(expectedLen, mock_malloc_in_size);

    // add a password
    mock_malloc_Reset();
    char *password = "password";
    expectedLen += 2 + strlen(password);
    err = umqtt_Connect(h, false, false, 0, 0, clientId, willTopic,
                        willMessage, willMessageLen, username, password);
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
    // computer how long client ID should be to get desire rem length
    unsigned int clientLen = remLen - (10 + 2);
    // null terminate the client ID string
    clientBuf[clientLen] = 0;

    // connect function will attempt two allocations so we need
    // two buffers for malloc to return
    mock_malloc_shouldReturn[0] = pktBuf;
    mock_malloc_shouldReturn[1] = &pktBuf[900];
    umqtt_Error_t err = umqtt_Connect(h, false, false, 0, 0, clientBuf,
                                    NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    TEST_ASSERT_EQUAL(2, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);

    // return the buffer that was passed to NetWrite
    // it is the mqtt packet and caller will verify rem length
    // was correctly encoded
    return mock_NetWrite_in_pBuf;
}

// basic rem length encoded in one byte
TEST(Connect, EncodedLengthOneByte)
{
    // set up packet for rem length and verify encoded value
    const uint8_t *netBuf = EncodedTest(76);
    TEST_ASSERT_EQUAL(76, netBuf[1]);
}

// max size encoded in one byte
TEST(Connect, EncodedLengthOneByteMax)
{
    // set up packet for rem length and verify encoded value
    const uint8_t *netBuf = EncodedTest(127);
    TEST_ASSERT_EQUAL(127, netBuf[1]);
}

// min size that rolls over to two bytes
TEST(Connect, EncodedLengthTwoByteMin)
{
    // set up packet for rem length and verify encoded value
    const uint8_t *netBuf = EncodedTest(128);
    TEST_ASSERT_EQUAL(0x80, netBuf[1]);
    TEST_ASSERT_EQUAL(1, netBuf[2]);
}

// nominal rem length that takes two bytes
TEST(Connect, EncodedLengthTwoByte)
{
    // set up packet for rem length and verify encoded value
    const uint8_t *netBuf = EncodedTest(135);
    TEST_ASSERT_EQUAL(0x87, netBuf[1]);
    TEST_ASSERT_EQUAL(1, netBuf[2]);
}

// basic connect with only client id
static uint8_t connectPacket0[] =
{
    0x10, 19, // fixed header
    0, 4, 'M', 'Q', 'T', 'T', // protocol name
    4, 0, // protocol level and connect flags
    0, 30, // keepalive
    0, 7, 'p','a','c','k','e','t','0',
};

// connect with all strings and some flags
static uint8_t connectPacket1[] =
{
    0x10, 65,
    0, 4, 'M', 'Q', 'T', 'T', // protocol name
    4, 0xee, // protocol level, connect flags user/pass retain qos1 will clean
    300 / 256, 300 % 256, // keepalive
    0, 7, 'p','a','c','k','e','t','1',
    0, 10, 'w','i','l','l','/','t','o','p','i','c',
    0, 12, 'w','i','l','l','-','m','e','s','s','a','g','e',
    0, 8, 'u','s','e','r','n','a','m','e',
    0, 8, 'p','a','s','s','w','o','r','d',
};

static uint8_t disconnectPacket[] = { 14 << 4, 0 };

TEST(Connect, Basic)
{
    umqtt_Error_t err;

    // set up for basic connect packet and call connect function
    unsigned int pktSize = sizeof(connectPacket0);
    mock_NetWrite_shouldReturn = pktSize;
    mock_malloc_shouldReturn[0] = pktBuf;
    mock_malloc_shouldReturn[1] = &pktBuf[900];
    err = umqtt_Connect(h, false, false, 0, 30, "packet0", NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify expected allocations/free
    TEST_ASSERT_EQUAL(2, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_TRUE(mock_free_wasCalled);
    TEST_ASSERT_EQUAL_PTR(pktBuf, mock_free_in_ptr);

    // verify net write was called and with expected size
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(pktSize, mock_NetWrite_in_len);
    TEST_ASSERT_FALSE(mock_NetWrite_in_isMore);

    // check the packet contents
    TEST_ASSERT_EQUAL_MEMORY(connectPacket0, mock_NetWrite_in_pBuf, pktSize);

    // verify connection status
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECT_PENDING, err);
}

TEST(Connect, Features)
{
    umqtt_Error_t err;

    // set up for connect packet with features and call connect function
    unsigned int pktSize = sizeof(connectPacket1);
    mock_NetWrite_shouldReturn = pktSize;
    mock_malloc_shouldReturn[0] = pktBuf;
    mock_malloc_shouldReturn[1] = &pktBuf[900];
    err = umqtt_Connect(h, true, true, 1, 300, "packet1",
                        "will/topic", (uint8_t *)"will-message", strlen("will-message"),
                        "username", "password");
    TEST_ASSERT_EQUAL(pktSize, mock_NetWrite_in_len);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify expected allocations/free
    TEST_ASSERT_EQUAL(2, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_TRUE(mock_free_wasCalled);
    TEST_ASSERT_EQUAL_PTR(pktBuf, mock_free_in_ptr);

    // verify net write was called and with expected size
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(pktSize, mock_NetWrite_in_len);
    TEST_ASSERT_FALSE(mock_NetWrite_in_isMore);

    // check the packet contents
    TEST_ASSERT_EQUAL_MEMORY(connectPacket1, mock_NetWrite_in_pBuf, pktSize);

    // verify connection status
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECT_PENDING, err);
}

// verify behavior if connect already pending
TEST(Connect, Pending)
{
    umqtt_Error_t err;

    // set up for basic connect packet and call connect function
    unsigned int pktSize = sizeof(connectPacket0);
    mock_NetWrite_shouldReturn = pktSize;
    mock_malloc_shouldReturn[0] = pktBuf;
    mock_malloc_shouldReturn[1] = &pktBuf[900];
    err = umqtt_Connect(h, false, false, 0, 30, "packet0", NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // this was already checked with Connect Basic test so dont need to
    // repeat all the checks.  Just verify a connect is pending
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECT_PENDING, err);

    // try connect again and verify fails due to pending
    pktSize = sizeof(connectPacket0);
    mock_NetWrite_shouldReturn = pktSize;
    mock_malloc_shouldReturn[0] = pktBuf;
    mock_malloc_shouldReturn[1] = &pktBuf[900];
    err = umqtt_Connect(h, false, false, 0, 30, "packet0", NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECT_PENDING, err);
}

// verify behavior if already connected
TEST(Connect, Connected)
{
    umqtt_Error_t err;

    // fake existing connection
    wrap_setConnected(h, true);

    // set up for basic connect packet and call connect function
    // verify already connected
    unsigned int pktSize = sizeof(connectPacket0);
    mock_NetWrite_shouldReturn = pktSize;
    mock_malloc_shouldReturn[0] = pktBuf;
    mock_malloc_shouldReturn[1] = &pktBuf[900];
    err = umqtt_Connect(h, false, false, 0, 30, "packet0", NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
}

TEST(Connect, DisconnectBadParms)
{
    umqtt_Error_t err;
    err = umqtt_Disconnect(NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
}

TEST(Connect, DisconnectBadNet)
{
    umqtt_Error_t err;
    err = umqtt_Disconnect(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
}

TEST(Connect, Disconnect)
{
    umqtt_Error_t err;
    // set fake connect
    wrap_setConnected(h, true);
    mock_NetWrite_shouldReturn = 2;
    err = umqtt_Disconnect(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify net write was called and with expected size
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(2, mock_NetWrite_in_len);
    TEST_ASSERT_FALSE(mock_NetWrite_in_isMore);

    // check the packet contents
    TEST_ASSERT_EQUAL_MEMORY(disconnectPacket, mock_NetWrite_in_pBuf, 2);

    // verify connection status
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_DISCONNECTED, err);
}

TEST_GROUP_RUNNER(Connect)
{
    RUN_TEST_CASE(Connect, NullParms);
    RUN_TEST_CASE(Connect, AllocFail);
    RUN_TEST_CASE(Connect, CalcLength);
    RUN_TEST_CASE(Connect, EncodedLengthOneByte);
    RUN_TEST_CASE(Connect, EncodedLengthOneByteMax);
    RUN_TEST_CASE(Connect, EncodedLengthTwoByteMin);
    RUN_TEST_CASE(Connect, EncodedLengthTwoByte);
    RUN_TEST_CASE(Connect, Basic);
    RUN_TEST_CASE(Connect, Features);
    RUN_TEST_CASE(Connect, Pending);
    RUN_TEST_CASE(Connect, Connected);
    RUN_TEST_CASE(Connect, DisconnectBadParms);
    RUN_TEST_CASE(Connect, DisconnectBadNet);
    RUN_TEST_CASE(Connect, Disconnect);
}
