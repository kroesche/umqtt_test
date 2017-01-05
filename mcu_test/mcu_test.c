/******************************************************************************
 * mcu_test.c - MCU demo app for using umqtt library
 *
 * Copyright (c) 2017, Joseph Kroesche (tronics.kroesche.io)
 * All rights reserved.
 *
 * This software is released under the FreeBSD license, found in the
 * accompanying file LICENSE.txt and at the following URL:
 *      http://www.freebsd.org/copyright/freebsd-license.html
 *
 * This software is provided as-is and without warranty.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include "inc/hw_types.h"
#include "inc/hw_memmap.h"

#include "driverlib/flash.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"

#include "utils/lwiplib.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"

#include "../umqtt/umqtt.h"
#include "net_client.h"
#include "swtimer.h"

// define timing used for tick timer
#define TICKS_PER_SECOND 100
#define MS_PER_TICK (1000 / (TICKS_PER_SECOND))

// define address and port of MQTT server
#ifndef CFG_SERVER_IPADDR
#define CFG_SERVER_IPADDR { 10, 0, 0, 12 }
#endif
#ifndef CFG_SERVER_PORT
#define CFG_SERVER_PORT 1883
#endif

// tick timer variables, one is milliseconds, the other seconds
static uint32_t msTicks = 0;
static uint32_t upTime = 0;

// the MAC and IP address of this node
static uint8_t macAddr[6];
static uint32_t ipAddr = 0;

// net_client instance handle, used for accessing network connection
static uint8_t netInst[NETCLIENT_INSTANCE_SIZE];
static NetClient_Handle_t hNet;

// network connection state
static bool netWasDisconnected = false;

// holds the server IP address and TCP port
static uint8_t serverAddr[] = CFG_SERVER_IPADDR;
static uint16_t serverPort = CFG_SERVER_PORT;

/**
 * The net_client event callback
 *
 * @param event the network event
 * @param app provided data pointer (not used here)
 */
static void
net_EventCb(NetClient_Event_t event, void *pUser)
{
    switch (event)
    {
        case NET_EVENT_CONNECTED:
        {
            break;
        }
        case NET_EVENT_DISCONNECTED:
        {
            netWasDisconnected = true;
            break;
        }
        case NET_EVENT_POLL:
        default:
        {
            break;
        }
    }
}

/**
 * The following are UMQTT callback functions for memory and transport
 */

// memory allocation for umqtt
static void *
app_malloc(size_t size)
{
    // since lwip already provides allocator, use that
    return mem_malloc(size);
}

// memory free for umqtt
static void
app_free(void *ptr)
{
    mem_free(ptr); // lwip allocator
}

// function to read a packet from the network
static int
netReadPacket(void *pNet, uint8_t **ppBuf)
{
    // get the length of the next incoming packet
    uint16_t len  = net_GetReadLen(pNet);
    if (len)
    {
        // allocate some memory to hold the packet data
        uint8_t *buf = app_malloc(len);
        if (buf)
        {
            // call net_client read function to get the packet data
            len = net_ReadPacket(pNet, buf, len);
            *ppBuf = buf;
        }
        else // allocation failure
        {
            UARTprintf("netRead malloc fail for %u\n", len);
        }
    }
    // return the number of bytes that were copied into the packet buffer
    // umqtt will free the packet buffer when it is finished
    return len;
}

// umqtt function to write a packet to the network
static int
netWritePacket(void *pNet, const uint8_t *pBuf, uint32_t len, bool isMore)
{
    len = net_WritePacket(pNet, pBuf, len, isMore);
    return len;
}

// define the transport callback structure that is used to
// initialize umqtt
static umqtt_TransportConfig_t transportConfig =
{
    NULL, app_malloc, app_free, netReadPacket, netWritePacket
    // hNet is populated at run time after network is opened
};

/**
 * The following are UMQTT callback functions to notify of MQTT events.
 * These are all optional.  They are all information except for PublishCb()
 * which is needed to received any published data from the remote MQTT
 * broker.
 */

// called when MQTT server acknowledges MQTT protocol connection
// (this is not the same thing as network connection to server)
static void
ConnackCb(umqtt_Handle_t h, void *pUser, bool sessionPresent, uint8_t retCode)
{
    UARTprintf("CONNACK\n");
}

// called by umqtt when a publish message is received from remote MQTT
static void
PublishCb(umqtt_Handle_t h, void *pUser, bool dup, bool retain, uint8_t qos,
          const char *pTopic, uint16_t topicLen, const uint8_t *pMsg, uint16_t msgLen)
{
    UARTprintf("PUBLISH: dup=%c retain=%c QoS=%u\n", dup?'y':'n', retain?'y':'n', qos);

    // allocate enough memory to hold the larger of the topic or message
    // need to add +1 to hold a null terminator for a string
    // MQTT format does not include a null terminator
    uint16_t len = ((topicLen > msgLen) ? topicLen : msgLen) + 1;
    char *buf = app_malloc(len);
    if (!buf)
    {
        UARTprintf("[could not print topic]\n");
        return;
    }
    // need to copy the topic and message into a buffer so that a
    // null terminator can be added and they can be treated as strings.
    // Note: if strncmp is used to test for certain topics or messages
    // then it may not be necessary to copy the data like this.
    // However, the pointers passed by umqtt do not persist beyond this
    // function so if the data is needed outside this callback then it
    // does need to be copied somewhere.
    // This example only prints the received topic and message.
    memcpy(buf, pTopic, topicLen);
    buf[topicLen] = 0;
    UARTprintf("[%s --> ", buf);
    memcpy(buf, pMsg, msgLen);
    buf[msgLen] = 0;
    UARTprintf("%s]\n", buf);
    app_free(buf);
}

// called by umqtt when the remote MQTT acknowledges a publish
static void
PubackCb(umqtt_Handle_t h, void *pUser, uint16_t pktId)
{
    UARTprintf("PUBACK(%u)\n", pktId);
}

// called by umqtt when the remote MQTT acknowledges a subscribe
static void
SubackCb(umqtt_Handle_t h, void *pUser, const uint8_t *retCodes,
         uint16_t retCount, uint16_t pktId)
{
    UARTprintf("SUBACK(%u)", pktId);
    for (unsigned int i = 0; i < retCount; i++)
    {
        UARTprintf(" %u", retCodes[i]);
    }
    UARTprintf("\n");
}

// called by umqtt when the remote MQTT acknowledges a ping request
static void
PingrespCb(umqtt_Handle_t h, void *pUser)
{
    UARTprintf("PINGRESP\n");
}

// populate callback structure needed to init umqtt
static umqtt_Callbacks_t callbacks =
{
    ConnackCb, PublishCb, PubackCb, SubackCb, NULL, PingrespCb
};

/**
 * SysTick interrupt handler
 *
 * This systick handler maintains a milliseconds counter and a seconds
 * counter used to track uptime.
 */
void
SysTickHandler(void)
{
    static uint32_t subseconds = 0;
    lwIPTimer(MS_PER_TICK);
    SwTimer_Tick();
    msTicks += MS_PER_TICK;
    ++subseconds;
    if (subseconds == TICKS_PER_SECOND)
    {
        subseconds = 0;
        ++upTime;
    }
}

/**
 * Initialize system clocks and timers, set up UART console.
 */
static void
initSys(void)
{
    // set clock to run from crystal
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC |
                   SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);

    // configure systick for 100 Hz (10 ms)
    SysTickPeriodSet(SysCtlClockGet() / TICKS_PER_SECOND);
    SysTickEnable();
    SysTickIntEnable();

    // set up UART console
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioConfig(0, 115200, SysCtlClockGet());
}

/**
 * Initialize the ethernet peripheral, get MAC address.
 */
static void
initEthernet(void)
{
    // enable ethernet
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);
    SysCtlPeripheralReset(SYSCTL_PERIPH_ETH);

    // Enable Port F for Ethernet LEDs.
    //  LED0        Bit 3   Output
    //  LED1        Bit 2   Output
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypeEthernetLED(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3);

    // Get the MAC address from the flash user registers
    unsigned long flashUser0, flashUser1;
    FlashUserGet(&flashUser0, &flashUser1);
    macAddr[0] = flashUser0 & 0xFF;
    macAddr[1] = (flashUser0 >> 8) & 0xFF;
    macAddr[2] = (flashUser0 >> 16) & 0xFF;
    macAddr[3] = flashUser1 & 0xFF;
    macAddr[4] = (flashUser1 >> 8) & 0xFF;
    macAddr[5] = (flashUser1 >> 16) & 0xFF;
}

/**
 * Define states used for application state machine
 */
typedef enum
{
    STATE_WAIT_IP,
    STATE_WAIT_NET,
    STATE_WAIT_MQTT,
    STATE_RUN_MQTT,
    STATE_RECOVERY,
    STATE_DELAY,
} AppState_t;
AppState_t runState = STATE_WAIT_IP;

/**
 * MAIN program - runs umqtt example
 */
int
main(void)
{
    static char nodeName[] = "mcu_test-000000000000";
    static char nodeTopic[] = "mcu-test/000000";
    static char topicBuf[32];
    static char msgBuf[32];
    static SwTimer_t upTimeReportTimer;
    static SwTimer_t errorTimer;

    // basic init
    initSys();
    UARTprintf("\n========\nmcu_test\n--------\n");

    // initialize the ethernet and then use the MAC address as a serial
    // number used to identify this MQTT client and also as the root topic
    initEthernet();
    UARTprintf("MAC: %02X-%02X-%02X-%02X-%02X-%02X\n", macAddr[0], macAddr[1],
               macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    usnprintf(nodeName, sizeof(nodeName), "mcu_test-%02X%02X%02X%02X%02X%02X",
              macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    usnprintf(nodeTopic, sizeof(nodeTopic), "mcu_test/%02X%02X%02X",
              macAddr[3], macAddr[4], macAddr[5]);

    // initialize lwip network stack
    lwIPInit(macAddr, 0, 0, 0, IPADDR_USE_DHCP);

    // start interrupts running
    IntMasterEnable();

    // Initialize the software timer tick period
    SwTimer_SetMsPerTick(MS_PER_TICK);

    // Initialize our network and save the network handle in the
    // umqtt transport structure
    hNet = net_Init(netInst, net_EventCb, NULL);
    transportConfig.hNet = hNet;

    // Initialize umqtt (MQTT client module)
    umqtt_Handle_t hu = umqtt_New(&transportConfig, &callbacks, NULL);
    if (!hu)
    {
        UARTprintf("umqtt_New() failed\n");
        for (;;) {}
    }

    // Set an initial uptime report interval of 5 seconds
    SwTimer_SetTimeout(&upTimeReportTimer, 5000);

    // main loop - runs state machine
    for (;;)
    {
        // If network is disconnected at any point,
        // then reset the state machine
        if (netWasDisconnected)
        {
            netWasDisconnected = false;
            if (runState != STATE_DELAY)
            {
                runState = STATE_RECOVERY;
            }
        }

        // state processing
        switch (runState)
        {
            // waiting for IP address assignment
            case STATE_WAIT_IP:
            {
                // check for IP address assignment (via dhcp)
                ipAddr = lwIPLocalIPAddrGet();
                if (ipAddr)
                {
                    // once IP address is assigned, initiate network
                    // connection to MQTT broker/server
                    UARTprintf("IP: %u.%u.%u.%u\n", ipAddr & 0xFF,
                              (ipAddr >> 8) & 0xFF, (ipAddr >> 16) & 0xFF,
                              (ipAddr >> 24) & 0xFF);
                    int ret = net_Connect(hNet, serverAddr, serverPort);
                    if (ret == 0)
                    {
                        runState = STATE_WAIT_NET; // wait for net connection
                    }
                    else
                    {
                        // handle error
                        runState = STATE_RECOVERY;
                    }
                }
                break;
            }

            // waiting for network connection
            case STATE_WAIT_NET:
            {
                // check for network connection
                if (net_IsConnected(hNet))
                {
                    // create will topic and initiate MQTT protocol
                    // the will topic sets our status to '0' to indicate
                    // to other subscribers that this node is offline
                    usnprintf(topicBuf, sizeof(topicBuf), "%s/status", nodeTopic);
                    uint8_t willPayload[] = { '0' };
                    umqtt_Error_t err;
                    err = umqtt_Connect(hu, true, true, 0, 60, nodeName,
                                        topicBuf, willPayload, 1, NULL, NULL);
                    if (err == UMQTT_ERR_OK)
                    {
                        runState = STATE_WAIT_MQTT; // wait for MQTT connect
                    }
                    else
                    {
                        UARTprintf("Connect() error: %s\n", umqtt_GetErrorString(err));
                        // handle error
                        runState = STATE_RECOVERY;
                    }
                }
                break;
            }

            // waiting for MQTT connection
            case STATE_WAIT_MQTT:
            {
                umqtt_Error_t err;
                // check for MQTT connection complete
                err = umqtt_GetConnectedStatus(hu);
                if (err == UMQTT_ERR_CONNECTED)
                {
                    // once connected, go to run state
                    runState = STATE_RUN_MQTT;

                    // subscribe to a topic that is used to send MQTT
                    // messages to this node
                    uint8_t qoss[1] = { 0 };
                    char *topics[1] = { topicBuf };
                    usnprintf(topicBuf, sizeof(topicBuf), "%s/set/#", nodeTopic);
                    umqtt_Subscribe(hu, 1, topics, qoss, NULL);

                    // publish some stuff, dont check err
                    // since qos is 0, we dont need to worry about
                    // piling up pending packets so we can just publish
                    // a bunch of stuff at once

                    // publish our status as 1 so that our running state
                    // can be detected by other subscribers
                    usnprintf(topicBuf, sizeof(topicBuf), "%s/status", nodeTopic);
                    msgBuf[0] = '1';
                    umqtt_Publish(hu, topicBuf, (uint8_t*)msgBuf, 1, 0, true, NULL);

                    // publish our MAC address and IP address
                    usnprintf(topicBuf, sizeof(topicBuf), "%s/mac", nodeTopic);
                    usnprintf(msgBuf, sizeof(msgBuf), "%02X-%02X-%02X-%02X-%02X-%02X",
                              macAddr[0], macAddr[1], macAddr[2],
                              macAddr[3], macAddr[4], macAddr[5]);
                    umqtt_Publish(hu, topicBuf, (uint8_t*)msgBuf, strlen(msgBuf), 0, true, NULL);
                    usnprintf(topicBuf, sizeof(topicBuf), "%s/ip", nodeTopic);
                    usnprintf(msgBuf, sizeof(msgBuf), "%u.%u.%u.%u", ipAddr & 0xFF,
                          (ipAddr >> 8) & 0xFF, (ipAddr >> 16) & 0xFF,
                          (ipAddr >> 24) & 0xFF);
                    umqtt_Publish(hu, topicBuf, (uint8_t*)msgBuf, strlen(msgBuf), 0, true, NULL);
                }
            } // deliberate fall-through

            // running state
            // no break in above case (this disables code analysis warning)
            case STATE_RUN_MQTT:
            {
                // call the umqtt run function so that it can do processing
                umqtt_Error_t err = umqtt_Run(hu, msTicks);
                if (err != UMQTT_ERR_OK)
                {
                    UARTprintf("Run() error: %s\n", umqtt_GetErrorString(err));
                    runState = STATE_RECOVERY;
                }

                // check for uptime report timeout
                // send topic that indicates our uptime in h:m:s
                if (SwTimer_IsTimedOut(&upTimeReportTimer))
                {
                    SwTimer_SetTimeout(&upTimeReportTimer, 5000);
                    usnprintf(topicBuf, sizeof(topicBuf), "%s/uptime", nodeTopic);
                    usnprintf(msgBuf, sizeof(msgBuf), "%02u:%02u:%02u",
                              upTime / 3600, (upTime / 60) % 60, upTime % 60);
                    umqtt_Publish(hu, topicBuf, (uint8_t*)msgBuf, strlen(msgBuf), 0, false, NULL);
                }
                break;
            }

            // disconnect and try to clean up everything
            case STATE_RECOVERY:
            {
                umqtt_Disconnect(hu);
                net_Disconnect(hNet);
                // more extreme step would be to delete umqtt instance
                // then re-New
                // also, this does nothing about restarting lwip stack
                // could track time or number of errors and just
                // force sw reset as final recovery step
                SwTimer_SetTimeout(&errorTimer, 3000);
                runState = STATE_DELAY;
                break;
            }

            // wait a while before starting again
            case STATE_DELAY:
            {
                if (SwTimer_IsTimedOut(&errorTimer))
                {
                    runState = STATE_WAIT_IP;
                }
                break;
            }
        }
    }
}
