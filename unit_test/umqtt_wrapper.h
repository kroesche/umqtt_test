
#ifndef __UMQTT_WRAPPER_H__
#define __UMQTT_WRAPPER_H__

extern uint8_t *wrap_newPacket(umqtt_Handle_t h, size_t len);
extern void wrap_deletePacket(umqtt_Handle_t h, uint8_t *pbuf);
extern void wrap_enqueuePacket(umqtt_Handle_t h, uint8_t *pbuf, uint16_t packetId, uint32_t ticks);
extern uint8_t *wrap_dequeuePacketById(umqtt_Handle_t h, uint16_t packetId);
extern uint8_t *wrap_dequeuePacketByType(umqtt_Handle_t h, uint8_t type);
extern PktBuf_t *wrap_getNextPktBuf(umqtt_Handle_t h);
extern void wrap_setConnected(umqtt_Handle_t h, bool connected);
extern void wrap_setKeepAlive(umqtt_Handle_t h, uint16_t keepAlive);

#endif
