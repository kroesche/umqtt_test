/******************************************************************************
 * umqtt_compliance_test.c - umqtt compliance test
 *
 * Copyright (c) 2016, Joseph Kroesche (tronics.kroesche.io)
 * All rights reserved.
 *
 * This software is released under the FreeBSD license, found in the
 * accompanying file LICENSE.txt and at the following URL:
 *      http://www.freebsd.org/copyright/freebsd-license.html
 *
 * This software is provided as-is and without warranty.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "unity_fixture.h"

#define MQTT_SERVER "localhost"
#define MQTT_PORT "1883"

// helper to establish network connection to MQTT broker
int
ConnectToServer(void)
{
    // set up socket options
    struct addrinfo hints;
    hints.ai_flags = 0;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_addrlen = 0;
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;
    hints.ai_next = NULL;
    struct addrinfo *pInfo;
    int res = getaddrinfo(MQTT_SERVER, MQTT_PORT, &hints, &pInfo);
    if (res != 0)
    {
        printf("getaddrinfo error %d\n", res);
        return 0;
    }

    // create socket
    int s = socket(pInfo->ai_family, pInfo->ai_socktype, pInfo->ai_protocol);
    if (s < 0)
    {
        printf("socket error %d\n", s);
        return s;
    }

    // attempt connection to server
    res = connect(s, pInfo->ai_addr, pInfo->ai_addrlen);
    if (res < 0)
    {
        printf("connect error %d (%s)\n", errno, strerror(errno));
        close(s);
        return res;
    }

    // set socket to non-blocking
    int flags = fcntl(s, F_GETFL, 0);
    flags |= O_NONBLOCK;
    res = fcntl(s, F_SETFL, flags);
    if (res)
    {
        close(s);
        return -1;
    }

    return s;
}

// write a packet to mqtt broker via network
int
WriteToServer(int socket, const uint8_t *buf, uint32_t len)
{
    int res = write(socket, buf, len);
    if (res == -1)
    {
        printf("write error %d (%s)\n", errno, strerror(errno));
    }
    else if ((uint32_t)res != len)
    {
        printf("write incomplete, attempted %d, wrote %d\n", len, res);
    }
    return res;
}

// read a packet from mqtt broker via network
// since socket is non-blocking, it can return without
// reading anything.  In this case it should return 0
int
ReadFromServer(int socket, uint8_t *buf, uint32_t len)
{
    int res = read(socket, buf, len);
    // possible error
    if (res == -1)
    {
        // however if EAGAIN, this just means no data for nob-blocking socket
        if (errno == EAGAIN)
        {
            return 0;
        }
        else
        {
            printf("read error %d (%s)\n", errno, strerror(errno));
        }
    }
    else if (res == 0)
    {
        printf("read 0 bytes\n");
    }
    return res;
}

// test helper to perform network disconnect from server
void
DisconnectServer(int socket)
{
    close(socket);
}

// run the compliance test
static void
RunAllTests(void)
{
    RUN_TEST_GROUP(Connect);
}

// Unity entry point
int
main(int argc, const char *argv[])
{
    return UnityMain(argc, argv, RunAllTests);
}
