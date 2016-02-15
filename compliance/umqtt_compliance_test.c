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

#include "unity_fixture.h"

#define MQTT_SERVER "localhost"
#define MQTT_PORT "1883"

int
ConnectToServer(void)
{
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

    int s = socket(pInfo->ai_family, pInfo->ai_socktype, pInfo->ai_protocol);
    if (s < 0)
    {
        printf("socket error %d\n", s);
        return s;
    }

    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    res = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (res < 0)
    {
        printf("setsockopt(RCVTIMEO) failed %d (%s)\n", errno, strerror(errno));
        return res;
    }
    res = setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (res < 0)
    {
        printf("setsockopt(SNDTIMEO) failed %d (%s)\n", errno, strerror(errno));
        return res;
    }

    res = connect(s, pInfo->ai_addr, pInfo->ai_addrlen);
    if (res < 0)
    {
        printf("connect error %d (%s)\n", errno, strerror(errno));
        close(s);
        return res;
    }
    return s;
}

int
WriteToServer(int socket, const uint8_t *buf, uint32_t len)
{
    int res = write(socket, buf, len);
    if (res == -1)
    {
        printf("write error %d (%s)\n", errno, strerror(errno));
    }
    else if (res != len)
    {
        printf("write incomplete, attempted %d, wrote %d\n", len, res);
    }
    return res;
}

int
ReadFromServer(int socket, uint8_t *buf, uint32_t len)
{
    int res = read(socket, buf, len);
    if (res == -1)
    {
        printf("read error %d (%s)\n", errno, strerror(errno));
    }
    else if (res == 0)
    {
        printf("read 0 bytes\n");
    }
    return res;
}

void
DisconnectServer(int socket)
{
    close(socket);
}

static void
RunAllTests(void)
{
    RUN_TEST_GROUP(Connect);
}

int
main(int argc, const char *argv[])
{
    return UnityMain(argc, argv, RunAllTests);
}
