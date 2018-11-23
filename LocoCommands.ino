#include "Debug.h"
#include "XPressNet.h"

LocomotiveSpeedAndDirection& LocomotiveSpeedAndDirection::speed(int n) {
  int mx = 28;
  switch (operId) {
    case locoSpeedDirection14 & 0xff: mx = 14; break;
    case locoSpeedDirection27 & 0xff: mx = 27; break;
    case locoSpeedDirection28 & 0xff: mx = 28; break;
    case locoSpeedDirection128 & 0xff: mx = 126; break;
  }
  if (debugLocoSpeed) {
    Serial.print(F("LOCO: speed =")); Serial.print(n); Serial.print(F(" steps:"));
    Serial.println(mx);
  }
  if (n > 0) {
    if (n > mx) {
      n = mx;
    }
  }
  byte encoded;

  if (n == 0) {
    encoded = 0;
  } else if (n < 0) {
    if (debugLocoSpeed) {
      Serial.print(F("LOCO: speed = EMSTOP")); 
    }
    // emergency stop
    encoded = 0x01;
  } else {
    switch (operId) {
      case locoSpeedDirection14 & 0xff:
        encoded = n + 1;
        break;
      case locoSpeedDirection27 & 0xff:
      case locoSpeedDirection28 & 0xff: {
        int tmp = n + 3;
        encoded = ((tmp & 0x01) << 4) | ((tmp & 0x1e) >> 1);
        break;
      }
      
      case locoSpeedDirection128 & 0xff:
        encoded = n + 2;
        break;
    }
  }  
  if (debugLocoSpeed) {
    Serial.print(F("\tencoded: ")); printHex2(Serial, encoded); Serial.println();
  }
  spd = encoded;
  return *this;
}

LocomotiveSpeedAndDirection& LocomotiveSpeedAndDirection::direction(boolean b) {
  dir = b;
  return *this;
}

