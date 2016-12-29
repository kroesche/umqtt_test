
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"
#include "umqtt_mocks.h"
#include "umqtt_wrapper.h"

TEST_GROUP(PacketHandling);

static umqtt_Handle_t h = NULL;

static void *instBuf = NULL;
#define SIZE_INSTBUF 200
static uint8_t *pktBuf1 = NULL;
static uint8_t *pktBuf2 = NULL;
static uint8_t *pktBuf3 = NULL;
#define SIZE_PKTBUF 1024

TEST_SETUP(PacketHandling)
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
    pktBuf1 = malloc(SIZE_PKTBUF);
    TEST_ASSERT_NOT_NULL(pktBuf1);
    pktBuf2 = malloc(SIZE_PKTBUF);
    TEST_ASSERT_NOT_NULL(pktBuf2);
    pktBuf3 = malloc(SIZE_PKTBUF);
    TEST_ASSERT_NOT_NULL(pktBuf3);
}

TEST_TEAR_DOWN(PacketHandling)
{
    if (instBuf) { free(instBuf); instBuf = NULL; }
    if (pktBuf1) { free(pktBuf1); pktBuf1 = NULL; }
    if (pktBuf2) { free(pktBuf2); pktBuf2 = NULL; }
    if (pktBuf3) { free(pktBuf3); pktBuf3 = NULL; }
    h = NULL;
}

// verify null parms handled ok
TEST(PacketHandling, NewPacketNull)
{
    uint8_t *buf;
    mock_malloc_shouldReturn[0] = pktBuf1;
    buf = wrap_newPacket(NULL, 0);
    TEST_ASSERT_NULL(buf);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
}

TEST(PacketHandling, DeletePacketNull)
{
    wrap_deletePacket(NULL, NULL);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    mock_free_Reset();
    wrap_deletePacket(h, NULL);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
}

TEST(PacketHandling, NewAndDelete)
{
    uint8_t *buf;
    mock_malloc_shouldReturn[0] = pktBuf1;
    buf = wrap_newPacket(h, 20);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    // whatever you ask for, it adds 5 bytes for mqtt packet overhead
    // and bytes for PktBuf_t which is packet management overhead
    TEST_ASSERT_EQUAL(20 + 5 + sizeof(PktBuf_t), mock_malloc_in_size);
    // it should return the "payload" area, the PktBuf_t area is hidden
    TEST_ASSERT_EQUAL_PTR(pktBuf1 + sizeof(PktBuf_t), buf);
    wrap_deletePacket(h, buf);
    TEST_ASSERT_TRUE(mock_free_wasCalled);
    // make sure it tries to free the same buffer we allocated
    // this ensures that with the adding and subtracting PktBuf_t
    // that everything evens out
    TEST_ASSERT_EQUAL(pktBuf1, mock_free_in_ptr);
}

TEST(PacketHandling, EnqueueNull)
{
    PktBuf_t *pkt;
    wrap_enqueuePacket(NULL, NULL, 0, 0);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(pkt);
    wrap_enqueuePacket(h, NULL, 0, 0);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(pkt);
    wrap_enqueuePacket(NULL, pktBuf1, 0, 0);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(pkt);
}

TEST(PacketHandling, EnqueueNominal)
{
    PktBuf_t *pkt;
    uint8_t *buf;
    mock_malloc_shouldReturn[0] = pktBuf1;
    buf = wrap_newPacket(h, 30);
    TEST_ASSERT_NOT_NULL(buf);
    wrap_enqueuePacket(h, buf, 123, 456);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_EQUAL_PTR(pktBuf1, pkt);
    TEST_ASSERT_EQUAL(123, pkt->packetId);
    TEST_ASSERT_EQUAL(456, pkt->ticks);
}

TEST(PacketHandling, EnqueueTwo)
{
    PktBuf_t *pkt;
    uint8_t *buf;
    mock_malloc_shouldReturn[0] = pktBuf1;
    mock_malloc_shouldReturn[1] = pktBuf2;
    buf = wrap_newPacket(h, 30);
    TEST_ASSERT_NOT_NULL(buf);
    wrap_enqueuePacket(h, buf, 123, 456);
    buf = wrap_newPacket(h, 40);
    TEST_ASSERT_NOT_NULL(buf);
    wrap_enqueuePacket(h, buf, 213, 567);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_EQUAL_PTR(pktBuf2, pkt);
    TEST_ASSERT_EQUAL(213, pkt->packetId);
    TEST_ASSERT_EQUAL(567, pkt->ticks);
    pkt = pkt->next;
    TEST_ASSERT_EQUAL_PTR(pktBuf1, pkt);
    TEST_ASSERT_EQUAL(123, pkt->packetId);
    TEST_ASSERT_EQUAL(456, pkt->ticks);
}

// enq a packet, verify bad parms for null inputs
TEST(PacketHandling, DequeueByIdNull)
{
    PktBuf_t *pkt;
    uint8_t *buf;
    mock_malloc_shouldReturn[0] = pktBuf1;
    buf = wrap_newPacket(h, 30);
    TEST_ASSERT_NOT_NULL(buf);
    wrap_enqueuePacket(h, buf, 123, 456);
    buf = wrap_dequeuePacketById(NULL, 0);
    TEST_ASSERT_NULL(buf);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_EQUAL_PTR(pktBuf1, pkt);
}

TEST(PacketHandling, DequeueByIdBadId)
{
    PktBuf_t *pkt;
    uint8_t *buf;
    mock_malloc_shouldReturn[0] = pktBuf1;
    buf = wrap_newPacket(h, 30);
    TEST_ASSERT_NOT_NULL(buf);
    wrap_enqueuePacket(h, buf, 123, 456);
    buf = wrap_dequeuePacketById(h, 0);
    TEST_ASSERT_NULL(buf);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_EQUAL_PTR(pktBuf1, pkt);
}

TEST(PacketHandling, DequeueByIdGood)
{
    PktBuf_t *pkt;
    uint8_t *buf;
    uint8_t *buf2;
    mock_malloc_shouldReturn[0] = pktBuf1;
    buf = wrap_newPacket(h, 30);
    TEST_ASSERT_NOT_NULL(buf);
    wrap_enqueuePacket(h, buf, 123, 456);
    buf2 = wrap_dequeuePacketById(h, 123);
    TEST_ASSERT_EQUAL_PTR(buf, buf2);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(pkt);
}

TEST(PacketHandling, DequeueByTypeNull)
{
    PktBuf_t *pkt;
    uint8_t *buf;
    mock_malloc_shouldReturn[0] = pktBuf1;
    buf = wrap_newPacket(h, 30);
    TEST_ASSERT_NOT_NULL(buf);
    buf[0] = 1;
    wrap_enqueuePacket(h, buf, 123, 456);
    buf = wrap_dequeuePacketByType(NULL, 0);
    TEST_ASSERT_NULL(buf);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_EQUAL_PTR(pktBuf1, pkt);
}

TEST(PacketHandling, DequeueByTypeBadType)
{
    PktBuf_t *pkt;
    uint8_t *buf;
    mock_malloc_shouldReturn[0] = pktBuf1;
    buf = wrap_newPacket(h, 30);
    TEST_ASSERT_NOT_NULL(buf);
    buf[0] = 2;
    wrap_enqueuePacket(h, buf, 123, 456);
    buf = wrap_dequeuePacketByType(h, 1);
    TEST_ASSERT_NULL(buf);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_EQUAL_PTR(pktBuf1, pkt);
}

TEST(PacketHandling, DequeueByTypeGood)
{
    PktBuf_t *pkt;
    uint8_t *buf;
    uint8_t *buf2;
    mock_malloc_shouldReturn[0] = pktBuf1;
    buf = wrap_newPacket(h, 30);
    TEST_ASSERT_NOT_NULL(buf);
    buf[0] = 3 << 4;
    wrap_enqueuePacket(h, buf, 123, 456);
    buf2 = wrap_dequeuePacketByType(h, 3);
    TEST_ASSERT_EQUAL_PTR(buf, buf2);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(pkt);
}

TEST(PacketHandling, DequeueMultiple)
{
    PktBuf_t *pkt;
    uint8_t *buf;
    uint8_t *buf1;
    uint8_t *buf2;
    uint8_t *buf3;
    mock_malloc_shouldReturn[0] = pktBuf1;
    mock_malloc_shouldReturn[1] = pktBuf2;
    mock_malloc_shouldReturn[2] = pktBuf3;
    buf1 = wrap_newPacket(h, 35);
    TEST_ASSERT_NOT_NULL(buf1);
    buf2 = wrap_newPacket(h, 45);
    TEST_ASSERT_NOT_NULL(buf2);
    buf3 = wrap_newPacket(h, 55);
    TEST_ASSERT_NOT_NULL(buf3);
    TEST_ASSERT_EQUAL(3, mock_malloc_count);
    buf1[0] = 1 << 4;
    buf2[0] = 2 << 4;
    buf3[0] = 3 << 4;
    wrap_enqueuePacket(h, buf1, 11, 111);
    wrap_enqueuePacket(h, buf2, 22, 222);
    wrap_enqueuePacket(h, buf3, 33, 333);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(pkt);
    buf = wrap_dequeuePacketById(h, 22);
    TEST_ASSERT_EQUAL_PTR(buf2, buf);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(pkt);
    buf = wrap_dequeuePacketByType(h, 3);
    TEST_ASSERT_EQUAL_PTR(buf3, buf);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(pkt);
    buf = wrap_dequeuePacketByType(h, 3);
    TEST_ASSERT_NULL(buf);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(pkt);
    buf = wrap_dequeuePacketByType(h, 1);
    TEST_ASSERT_EQUAL_PTR(buf1, buf);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(pkt);
}

TEST_GROUP_RUNNER(PacketHandling)
{
    RUN_TEST_CASE(PacketHandling, NewPacketNull);
    RUN_TEST_CASE(PacketHandling, DeletePacketNull);
    RUN_TEST_CASE(PacketHandling, NewAndDelete);
    RUN_TEST_CASE(PacketHandling, EnqueueNull);
    RUN_TEST_CASE(PacketHandling, EnqueueNominal);
    RUN_TEST_CASE(PacketHandling, EnqueueTwo);
    RUN_TEST_CASE(PacketHandling, DequeueByIdNull);
    RUN_TEST_CASE(PacketHandling, DequeueByIdBadId);
    RUN_TEST_CASE(PacketHandling, DequeueByIdGood);
    RUN_TEST_CASE(PacketHandling, DequeueByTypeNull);
    RUN_TEST_CASE(PacketHandling, DequeueByTypeBadType);
    RUN_TEST_CASE(PacketHandling, DequeueByTypeGood);
    RUN_TEST_CASE(PacketHandling, DequeueMultiple);
}
