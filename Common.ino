#include <EEPROM.h>
#include "Debug.h"

void checkInitEEPROM() {
  int savedVer = EEPROM.read(eeaddr_version);
  if (savedVer == CURRENT_DATA_VERSION) {
    return;
  }
  Serial.println(F("Obsolete EEPROM, reinitializing"));
  ModuleChain::invokeAll(ModuleCmd::reset);
  ModuleChain::invokeAll(ModuleCmd::eepromSave);
  EEPROM.write(eeaddr_version, CURRENT_DATA_VERSION);
}


/**
   Writes a block of data into the EEPROM, followed by a checksum
*/
void eeBlockWrite(byte magic, int eeaddr, const void* address, int size) {
  if (debugControl) {
    Serial.print(F("Writing EEPROM ")); Serial.print(eeaddr, HEX); Serial.print(F(":")); Serial.print(size); Serial.print(F(", source: ")); Serial.println((int)address, HEX);
  }
  const byte *ptr = (const byte*) address;
  byte hash = magic;
  EEPROM.write(eeaddr, magic);
  eeaddr++;
  for (; size > 0; size--) {
    byte b = *ptr;
    EEPROM.write(eeaddr, b);
    ptr++;
    eeaddr++;
    hash = hash ^ b;
  }
  EEPROM.write(eeaddr, hash);
}

/**
   Reads block of data from the EEPROM, but only if the checksum of
   that data is correct.
*/
boolean eeBlockRead(byte magic, int eeaddr, void* address, int size) {
  if (debugControl) {
    Serial.print(F("Reading EEPROM ")); Serial.print(eeaddr, HEX); Serial.print(F(":")); Serial.print(size); Serial.print(F(", dest: ")); Serial.println((int)address, HEX);
  }
  int a = eeaddr;
  byte hash = magic;
  byte x;
  boolean allNull = true;
  x = EEPROM.read(a);
  if (x != magic) {
    if (debugControl) {
      Serial.println(F("No magic header found"));
    }
    return false;
  }
  a++;
  for (int i = 0; i < size; i++, a++) {
    x = EEPROM.read(a);
    if (x != 0) {
      allNull = false;
    }
    hash = hash ^ x;
  }
  x = EEPROM.read(a);
  if (hash != x || allNull) {
  if (debugControl) {
      Serial.println(F("Checksum does not match"));
    }
    return false;
  }

  a = eeaddr + 1;
  byte *ptr = (byte*) address;
  for (int i = 0; i < size; i++, a++) {
    x = EEPROM.read(a);
    *ptr = x;
    ptr++;
  }
}


ModuleChain::ModuleChain(const char* name, byte aPrirority,  void (*h)(ModuleCmd)) : next(NULL), priority(aPrirority) {
  static ModuleChain* __last = NULL;
  if (__last == NULL) {
    head = NULL;
  }
  __last = this;
  handler = h;
  if (head == NULL) {
    head = this;
    return;
  }
  if (head->priority >= aPrirority) {
    next = head;
    head = this;
    return;
  }
  ModuleChain* p = head;
  while (p->priority < aPrirority) {
    if (p->next == NULL) {
      p->next = this;
      return;
    }
    p = p->next;
  }
  if (p->next == NULL) {
    p->next = this;
    return;
  }
  next = p->next;
  p->next = this;
}

void ModuleChain::invokeAll(ModuleCmd cmd) {
  ModuleChain*p = head;
  while (p != NULL) {
    if (p->handler != NULL) {
      p->handler(cmd);
    }
    p = p->next;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ACK / status LED handling
const byte* blinkPtr = NULL;
long blinkLastMillis;
byte pos = 0;
boolean ackLedState = false;
byte pulseCount = 0;

const byte blinkShort[] = { 25, 25, 0 };   // blink once, short
const byte blinkLong[] = { 50, 0 };
const byte blinkReject[] = { 25, 25, 25, 25, 0 };
const byte blinkSave[] = { 25, 25, 25, 25, 25, 25, 50, 0 };

void makeLedAck(const byte *ledSequence) {
  blinkPtr = ledSequence;
  pos = 0;
  ackLedState = 1;
  blinkLastMillis = millis();
  digitalWrite(LED_ACK, HIGH);
  if (debugControl) {
    Serial.print(F("LED ACK: ")); Serial.println(blinkPtr[pos]);
  }
}

void handleAckLed() {
  if (blinkPtr == NULL) {
    return;
  }
  long t = millis();
  long l = (t - blinkLastMillis) / 10;
  if (l < *blinkPtr) {
    return;
  }
  blinkLastMillis = t;
  blinkPtr++;
  if (debugControl) {
    Serial.print(F("Next ACK time: ")); Serial.println(*blinkPtr);
  }
  if (*blinkPtr == 0) {
    ackLedState = 0;
    digitalWrite(LED_ACK, LOW);
    if (pulseCount > 0) {
      pulseCount--;
      makeLedAck(&blinkShort[0]);
    } else {
      if (debugControl) {
        Serial.println(F("ACK done"));
      }
      blinkPtr = NULL;
      blinkLastMillis = -1;
    }
    return;
  }
  ackLedState = !ackLedState;
  digitalWrite(LED_ACK, ackLedState ? HIGH : LOW);
}

