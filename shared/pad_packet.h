#pragma once
#include <stdint.h>
#include <stddef.h>
// Little-endian wire protocol. Exactly 20 bytes.
// Sync A5 5A | seq | buttons (uint16) | hat | lx ly rx ry | lt rt |
// flags | reserved[5] | crc8
// Axes are signed int8; triggers are 0..255. No Bluetooth => flags bit0=0.
#define PAD_PACKET_SIZE 20
#define PAD_SYNC0 0xA5
#define PAD_SYNC1 0x5A
#define PAD_CONNECTED 1

enum PadButtons {
  PAD_A=1u<<0, PAD_B=1u<<1, PAD_X=1u<<2, PAD_Y=1u<<3,
  PAD_L=1u<<4, PAD_R=1u<<5, PAD_ZL=1u<<6, PAD_ZR=1u<<7,
  PAD_START=1u<<8, PAD_SELECT=1u<<9, PAD_L3=1u<<10, PAD_R3=1u<<11,
  PAD_HOME=1u<<12
};
static inline uint8_t pad_crc8(const uint8_t *p, size_t n) {
  uint8_t crc=0;
  for(size_t i=0;i<n;i++) {crc^=p[i]; for(int j=0;j<8;j++) crc=(crc&0x80)?(uint8_t)((crc<<1)^0x07):(uint8_t)(crc<<1);}
  return crc;
}
static inline void pad_encode(uint8_t *p, uint8_t seq, uint16_t buttons,
                              uint8_t hat, int8_t lx,int8_t ly,int8_t rx,int8_t ry,
                              uint8_t lt,uint8_t rt, bool connected) {
  for(int i=0;i<PAD_PACKET_SIZE;i++)p[i]=0;
  p[0]=PAD_SYNC0;p[1]=PAD_SYNC1;p[2]=seq;p[3]=buttons&255;p[4]=buttons>>8;
  p[5]=hat;p[6]=(uint8_t)lx;p[7]=(uint8_t)ly;p[8]=(uint8_t)rx;p[9]=(uint8_t)ry;
  p[10]=lt;p[11]=rt;p[12]=connected?PAD_CONNECTED:0;
  p[19]=pad_crc8(p,19);
}
static inline bool pad_valid(const uint8_t *p) {
 return p[0]==PAD_SYNC0 && p[1]==PAD_SYNC1 && p[5]<=8 && p[19]==pad_crc8(p,19);
}
