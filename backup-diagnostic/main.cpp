// EXPERIMENTAL: DS3-like USB HID, not a verified DualShock 3 replacement.
// LOLIN32 Lite Bluepad32 firmware is not modified.
#include <Arduino.h>
#include <USB.h>
#include <USBHID.h>
#include "pad_packet.h"
#include <string.h>

static constexpr int UART_RX = 16, UART_TX = 17;
static constexpr uint32_t BAUD = 115200;
static constexpr uint32_t INPUT_TIMEOUT_MS = 120;
static constexpr uint32_t REPORT_INTERVAL_MS = 10;

// Diagnostic HID descriptor: explicitly exposes 24 buttons and four axes.
// This is NOT an authentic DualShock 3 descriptor or complete DS3 emulation.
// Payload layout remains 48 bytes, keeping existing UART->report mapping.
static const uint8_t ds3_like_descriptor[] = {
  0x05, 0x01,       // Usage Page (Generic Desktop)
  0x09, 0x05,       // Usage (Game Pad)
  0xA1, 0x01,       // Collection (Application)
  0x85, 0x01,       // Report ID 1
  // Byte 0: reserved
  0x75, 0x08, 0x95, 0x01, 0x81, 0x03,
  // Bytes 1..3: 24 digital buttons
  0x05, 0x09,       // Usage Page (Button)
  0x19, 0x01, 0x29, 0x18,
  0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x18,
  0x81, 0x02,
  // Byte 4: reserved
  0x75, 0x08, 0x95, 0x01, 0x81, 0x03,
  // Bytes 5..8: left/right X/Y (same mapping as existing prototype)
  0x05, 0x01,
  0x09, 0x30, 0x09, 0x31, 0x09, 0x33, 0x09, 0x34,
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x04,
  0x81, 0x02,
  // Bytes 9..47: reserved / analog trigger data, not exposed yet
  0x75, 0x08, 0x95, 0x27, 0x81, 0x03,
  0xC0
};

static USBHID HID;
class DS3LikeDevice : public USBHIDDevice {
 public:
  DS3LikeDevice() { HID.addDevice(this, sizeof(ds3_like_descriptor)); }
  void begin() { HID.begin(); }
  uint16_t _onGetDescriptor(uint8_t* buffer) override {
    memcpy(buffer, ds3_like_descriptor, sizeof(ds3_like_descriptor));
    return sizeof(ds3_like_descriptor);
  }
  bool send(const uint8_t *reportPayload) {
    return HID.SendReport(1, reportPayload, 48);
  }
};
static DS3LikeDevice pad;
static uint8_t uartPacket[PAD_PACKET_SIZE];
static uint8_t uartPos = 0;
static uint32_t lastGoodMs=0, lastReportMs=0;
static bool connected=false;
static uint8_t report[48]; // excludes report ID; ID added by SendReport

static uint8_t signedToUnsigned(uint8_t v) {
  // UART axes carry two's-complement signed int8, center ~0.
  int n = (int)(int8_t)v + 128;
  return (uint8_t)constrain(n, 0, 255);
}
static void neutral() {
  memset(report, 0, sizeof(report));
  report[5] = report[6] = report[7] = report[8] = 128;
  // DS3 raw report bytes 41..48 (incl ID) are sensors; neutral-ish value.
  for (int i=40; i<48; i+=2) {report[i]=0x02; report[i+1]=0x00;}
}
static void accept(const uint8_t* f) {
  if (!(f[12]&PAD_CONNECTED)) {connected=false;neutral();return;}
  connected=true;
  lastGoodMs=millis();
  neutral();
  const uint16_t b = (uint16_t)f[3] | ((uint16_t)f[4]<<8);
  const uint8_t hat = f[5]; // 0 neutral, 1 up, 2 NE, ... 8 NW
  // DS3 raw USB report positions INCLUDING report ID:
  // index 2: Select/L3/R3/Start/Dpad; index 3: L2/R2/L1/R1/triangle/circle/cross/square
  // index 4: PS button.
  uint8_t b2=0, b3=0, b4=0;
  if (b & PAD_SELECT) b2 |= 0x01;
  if (b & PAD_L3) b2 |= 0x02;
  if (b & PAD_R3) b2 |= 0x04;
  if (b & PAD_START) b2 |= 0x08;
  if (hat==1 || hat==2 || hat==8) b2 |= 0x10; // up
  if (hat==2 || hat==3 || hat==4) b2 |= 0x20; // right
  if (hat==4 || hat==5 || hat==6) b2 |= 0x40; // down
  if (hat==6 || hat==7 || hat==8) b2 |= 0x80; // left
  if (b & PAD_ZL) b3 |= 0x01; // L2
  if (b & PAD_ZR) b3 |= 0x02; // R2
  if (b & PAD_L) b3 |= 0x04;  // L1
  if (b & PAD_R) b3 |= 0x08;  // R1
  if (b & PAD_Y) b3 |= 0x10;  // Triangle
  if (b & PAD_B) b3 |= 0x20;  // Circle
  if (b & PAD_A) b3 |= 0x40;  // Cross
  if (b & PAD_X) b3 |= 0x80;  // Square
  if (b & PAD_HOME) b4 |= 0x01;
  report[1]=b2;report[2]=b3;report[3]=b4;
  // DS3 USB axes: byte indexes 6..9 including report ID.
  report[5]=signedToUnsigned(f[6]); // LX
  report[6]=signedToUnsigned(f[7]); // LY
  report[7]=signedToUnsigned(f[8]); // RX
  report[8]=signedToUnsigned(f[9]); // RY
  // Analog button pressure values: byte 23 (L2) & 24 (R2), incl ID
  report[22]=f[10];report[23]=f[11];
}
static void receiveUART() {
  while (Serial1.available()>0) {
    uint8_t c=(uint8_t)Serial1.read();
    if (uartPos==0 && c!=PAD_SYNC0) continue;
    if (uartPos==1 && c!=PAD_SYNC1) {uartPos=(c==PAD_SYNC0)?1:0;continue;}
    uartPacket[uartPos++]=c;
    if (uartPos==PAD_PACKET_SIZE) {
      if (pad_valid(uartPacket)) accept(uartPacket);
      uartPos=0;
    }
  }
}
void setup() {
  Serial1.begin(BAUD, SERIAL_8N1, UART_RX, UART_TX);
  neutral();
  // Debug profile for Windows control verification, not a DS3 clone.
  // Espressif VID/PID used for development; do not impersonate a Sony DS3.
  USB.VID(0x303A);
  USB.PID(0x4002);
  USB.manufacturerName("ESP32 Test Bridge");
  USB.productName("UART Gamepad HID Diagnostic");
  pad.begin();
  USB.begin();
}
void loop() {
  receiveUART();
  if (connected && millis()-lastGoodMs>INPUT_TIMEOUT_MS) {
    connected=false;neutral();
  }
  if (millis()-lastReportMs>=REPORT_INTERVAL_MS) {
    lastReportMs=millis();
    if (HID.ready()) pad.send(report);
  }
  delay(1);
}
