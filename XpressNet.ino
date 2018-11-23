#include "XPressNet.h"

void XPressNetPacket::printPacket() {
  const byte *p = data();
  byte s = size() + 1;
  for (byte c = s; c > 0; c--, p++) {
    printHex2(Serial, *p); Serial.print(' ');
  }
}

XPressNet::XPressNet(RingBuffer& send, HardwareSerial& serial, int dir, int id) : 
  sendBuffer(send), 
  comm(serial), 
  directionPin(dir),
  deviceId(id), 
  currentDevice(0xff),
  feedbackNotifier(maxFeedbackListeners, listeners, ListenerType::feedback)
{
  // zero out feedback states
  for (byte i = 0; i < _feedbackSlotCount; feedbackInitSlots[i++] = 0) ;
  for (byte i = 0; i < _feedbackBitSize; feedbackState[i++] = 0) ;
  clear();
  clearReceivedPacket();
}

void XPressNet::clear() {
  recvCallByte = recvHeaderByte = 0xff;
  state = idle;
  recvCount = recvIndex = 0;
}

void XPressNet::clearReceivedPacket() {
  recvCallByte = 0xff;
  recvHeaderByte = 0x01;
  recvPacket[2] = 0;
  fromMaster = false;
  computedXor = 0;
  recvCount = recvIndex = 0;
  skipping = false;
  state = idle;
  currentDevice = 0;
}

boolean XPressNet::checkReceivedPacket() {
  return computedXor == 0;
}

boolean XPressNet::verifyCallByte(int cb) {
  cb = cb & 0xff;
  byte oneCnt = 0;
  for (byte m = 0x80; m > 0; m >>= 1) {
    if ((cb & m) > 0) {
      oneCnt++;
    }
  }
  // true on odd parity
  return (oneCnt & 0x01) == 0;
}

void XPressNet::sendQueuedRequest() {
  if (!sendBuffer.available()) {
    return;
  }
  const byte* x = sendBuffer.current();
  int sz = sendBuffer.currentSize();

  digitalWrite(directionPin, HIGH);
  delayMicroseconds(10);

  for (byte c = sz; c > 0; c--, x++) {
    comm.write(*x);
  }
  comm.flush();
  delayMicroseconds(10);
  digitalWrite(directionPin, LOW);
  
  if (debugSendRequest) {
    Serial.print(F("XN: sendRequest size:"));
    Serial.print(sz); Serial.print('\t');
    x = sendBuffer.current();
    for (int c = sz; c > 0; c--, x++) {
      printHex2(Serial, *x); Serial.print(' ');
    }
    Serial.println();
  }
  sendBuffer.free();
  if (debugSendRequest) {
    Serial.print(F("XN: send remain:")); Serial.println(sendBuffer.queued());
  }
}

void XPressNet::receiveEvent() {
  while (comm.available()) {
    int data = comm.read();

    if (state == idle) {
      clearReceivedPacket();
    }
    if (state == idle || state == header) {
      computedXor = data & 0xff;
    } else {
      computedXor = computedXor ^ (data & 0xff);
    }

    boolean fromStation = data > 0x100;

    if (fromStation) {
      // reset the state to idle, so that 1st byte will
      // be received as callbyte
      // TODO: raise an error
      state = idle;
    }

    switch (state) {
      case ignoreToMaster:
        if (!fromStation) {
          // just ignore
          continue;
        }
        // fall through to idle, since master has started
      case idle:
        if (fromStation) {
          if (verifyCallByte(data)) {
            state = header;
            recvCallByte = data;
            if ((data & 0x60) == 0x40) {
              if ((data & 0x1f) == deviceId) {
                sendQueuedRequest();
                if (debugXnetCall) {
                  Serial.print(F("XN: Call ")); Serial.println(data, HEX);
                }
              }
              state = idle;
              break;
            }
            if (debugXnetComm) {
              if (shouldPrintMsg()) {    
                Serial.print(F("XN: Call ")); Serial.println(data, HEX);
              }
            }
            this->fromMaster = true;
            currentDevice = data & 0x1f;
          } else {
            if (debugXnetComm) {
              Serial.println(F("XN: Parity error"));
            }
            state = ignoreToMaster;
          }
          break;
        } 
        this->fromMaster = false;
        recvCallByte = 0xff;
        // fall through
      case header:
        
        recvHeaderByte = data;
        // decide whether to receive the data or just skip
        state = payload;
        recvIndex = 2;  // skip the call and header 
        {
          byte c = packet.size();
          // avoid buffer overflow
          if (c > sizeof(recvPacket) - 2) {
            c = sizeof(recvPacket) - 2;
          }
          recvCount = c;
        }
        if (debugXnetComm) {
          if (shouldPrintMsg()) {
            Serial.print(F("XN: device: "));
            if (fromMaster) {
              Serial.print('*');
            }
            Serial.println(currentDevice);
            Serial.print(F("XN: Header ")); 
            Serial.print(data, HEX); Serial.print(F(" size:")); Serial.println(recvCount);
            Serial.print(F("XN: Data:  "));
          }
        }
        break;

      case skip:
        if (--recvCount == 0) {
          state = checksum;
        }
        break;

      case payload:
        if (recvCount == 0) {
          // ignore the data
          break;
        }
        if (debugXnetComm) {
          if (shouldPrintMsg()) {
            printHex2(Serial, data); Serial.print(' ');
          }
        }
#ifdef DEBUG_MEM
        if (recvIndex >= sizeof(recvPacket)) {
            Serial.println("**** RecvPacket overflow");
        }
#endif
        recvPacket[recvIndex++] = data & 0xff;
        if (--recvCount == 0) {
          state = checksum;
        }
        break;
        
      case checksum:
        if (debugXnetComm) {
          if (shouldPrintMsg()) {
            Serial.print(" : "); printHex2(Serial, data); Serial.println();
          }
        }
        if (!checkReceivedPacket()) {
          // discard the packet
          if (debugXnetComm) {
            Serial.println(F("XN: Checsum error"));
          }
          state = ignoreToMaster;
        } else {
          if (fromMaster) {
            if (header != 0xe3) {
              processStationPacket();
            }
          } else {
            processDevicePacket();
          }
          state = idle;
        }
        clearReceivedPacket();
        break;
    }
  }
}

void XPressNet::processStationPacket() {
//  Serial.print("Broadcast: "); Serial.print(packet.isBroadcast()); Serial.print(" Feedback: "); Serial.println(packet.isFeedback()); 
  updateFeedbackState();
  processAccessoryResponse();
  if (packet.isBroadcast()) {
    
  }
}

void XPressNet::processLocoFunction() {
  
}

void XPressNet::processLocoSpeed() {
  
}

void XPressNet::processDevicePacket() {
  byte ident;
  switch (packet.headerByte()) {
    case 0xe4:
      ident = packet.ident();
      switch (ident) {
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
          processLocoSpeed();

        case 0x20:
        case 0x21:
        case 0x22:
          processLocoFunction();

        case 0x24:
        case 0x25:
        case 0x26:
//          processLocoFunction();

        default:
          return;
      }
  }
}

void XPressNet::addFeedbackChange(int f, boolean state) {
  byte a = f >> 3;
  byte m = (1 << (f & 0x07));
  byte o = state ? m : 0;
  if (a < sizeof(feedbackState)) {
#ifdef DEBUG_MEM
    if (a >= sizeof(feedbackState)) {
        Serial.print("**** feedback overflow 1");
    }
#endif
    feedbackState[a] = (feedbackState[a] & ~m) | o;
  }
  if (debugFeedbackChange) {
    Serial.print(F("XN: Feedback change: ")); Serial.print(f); Serial.print(F(" -> ")); Serial.println(state);
  }
  feedbackNotifier.fire(state, f, 0xffff);
}

void XPressNet::processAccessoryResponse() {
  // it may be an accessory info response:
  if (((packet.callByte() & 0x60) == 0x60) && (packet.headerByte() == 0x42)) {
    if (debugFeedbackInfo) {
      Serial.println(F("XN: Accessory response"));
    }
    READ_XNET_PACKET(feedback, packet, FeedbackBroadcast);
    int s = feedback.start(0);
    if (s >= firstFeedback) {
      doProcessFeedback(feedback);
    } else {
      if (debugXnetComm) {
        Serial.print(F("XN: not feedback: ")); Serial.println(s);
      }
    }
  }
}

/**
 * Updates the feedback bits based on the current feedback packet. 
 * When feedback changes, or just becomes available, the function will
 * queue a 
 */
void XPressNet::updateFeedbackState() {
  if (packet.isFeedback()) {
    if (debugFeedbackBcastLow) {
      Serial.println(F("XN: Feedback update"));
    }
    READ_XNET_PACKET(feedback, packet, FeedbackBroadcast);
    doProcessFeedback(feedback);
  }
}

void XPressNet::doProcessFeedback(const FeedbackBroadcast& feedback) {
  for (byte i = 0; i < feedback.count(); i++) {
    if (debugFeedbackBcastLow) {
      byte *p = &recvHeaderByte;
      Serial.print(F("* Feedback info: "));
      for (byte i = 0; i < packet.size() + 1; i++) {
        printHex2(Serial, *p); Serial.print(' ');
        p++;
      }
      Serial.println();
    }
    int s = feedback.start(i);
    if (s < firstFeedback) {
      if (debugFeedbackBcastLow) {
        Serial.print(F("Device ")); Serial.print(s); Serial.println(F(" is not feedback mod"));
      }
      continue;
    }
    s -= firstFeedback;

    byte bits = feedback.bits(i);
    byte mask = feedback.mask(i);
    byte count = 4;
    boolean n = feedback.nibble(i);
    byte m = n ? 0x10 : 0x01;
    byte& slot = feedbackState[s >> 3];
    byte flips = (slot ^ bits) & mask;
    int no = s;

    if (debugFeedbackBcastLow) {
      Serial.print(F("start: ")); Serial.println(s);
      Serial.print(F("bits: ")); Serial.println(bits, BIN);
      Serial.print(F("mask: ")); Serial.println(mask, BIN);
      Serial.print(F("nibble: ")); Serial.println(n);
      Serial.print(F("changes: ")); Serial.println(flips, BIN);
      Serial.print(F("old: ")); Serial.println(slot, BIN);
      Serial.print(F("new: ")); Serial.println(slot ^ flips, BIN);
      Serial.print(F("state index: ")); Serial.print(s >> 3); Serial.print(F(" state size: ")); Serial.println(sizeof(feedbackState));
      Serial.print(F("ready: ")); Serial.println(isFeedbackReady(no), BIN);
    }
    slot = slot ^ flips;
    if (debugFeedbackBcastLow) {
      Serial.print(F("slot1: ")); Serial.println(slot, BIN);
    }
    if (isFeedbackReady(no)) {
      for (byte b= 0; b < 4; b++, no++) {
        boolean state = bits & m;
        if (flips & m) {
          addFeedbackChange(no, state);
        }
        m <<= 1;
      }
    } else {
      // add all items since the feedback state is not known
      int initSlotNo = no / 4;
      int sa = initSlotNo >> 3;
      byte initm = (1 << (initSlotNo & 0x07));
      if (debugFeedbackBcastLow) {
        Serial.print(F("init-slot: ")); Serial.println(initSlotNo);
        Serial.print(F("init-sa: ")); Serial.print(sa);  Serial.print(F(" init-size: ")); Serial.println(sizeof(feedbackInitSlots));
        Serial.print(F("mask: ")); Serial.println(initm, BIN);
        Serial.print(F("old: ")); Serial.println(feedbackInitSlots[sa], BIN);
      }
      if (sa < sizeof(feedbackInitSlots)) {
        feedbackInitSlots[sa] |= initm;
      }
      if (debugFeedbackBcastLow) {
        Serial.print(F("slot2: ")); Serial.println(slot, BIN);
        Serial.print(F("bits: ")); Serial.println(bits, BIN);
        Serial.print(F("m: ")); Serial.println(m, BIN);
      }
      
      for (byte b= 0; b < 4; b++, no++) {
        boolean state = bits & m;
        addFeedbackChange(no, state);
        m <<= 1;
      }
    }
    if (debugFeedbackBcastLow) {
      Serial.print(F("slot3: ")); Serial.println(slot, BIN);
    }
  }
}

boolean XPressNet::sendPacket(const XPressNetPacket& packet) {
  if (debugSendRequest) {
    Serial.print(F("XN: queue "));
    packet.printPacket();
    Serial.println();
  }
  byte sz = packet.size() + 1 /* header byte */;
  byte* out = sendBuffer.malloc(sz + 1 /* xor byte */);
  if (out == NULL) {
    return false;
  }
  byte x = 0;
  const byte* p = packet.data();
  for (byte i = sz; i > 0; p++, i--) {
    byte d = *p;
    x = x ^ d;
    *(out++) = d;
  }
  *out = x;
  return true;
}

void XPressNet::setup() {
  digitalWrite(directionPin, LOW);
  pinMode(directionPin, OUTPUT);
}

void XPressNet::processWriteEmptyIrq() {
  comm._tx_udr_empty_irq();
  int c = comm.availableForWrite();
  if (comm.availableForWrite() == 0) {
    digitalWrite(directionPin, LOW);
  }
}


FeedbackStateRequest& FeedbackStateRequest::sensor(int id) {
  int no = id + firstFeedback;
  slot = no / 8;
  nibble = (no & 0x04) ? 1 : 0;

  if (debugFeedbackInfo) {
    Serial.print(F("FB: request: ")); Serial.print(id); Serial.print(F(" slot:")); Serial.print(slot); Serial.print('/'); Serial.println(nibble);
  }
  return *this;
}

AccessoryCommand& AccessoryCommand::device(int id) {
  int n = id - 1;
  int s = n / 4;
  int m = n & 0x03;

  slot = s;
  part = m;
  Serial.print(F("ACC: Command dev=")); Serial.print(id); Serial.print(F(" slot:")); Serial.print(slot); Serial.print(F(" part:")); Serial.println(part);
  return *this;
}

AccessoryCommand& AccessoryCommand::makeThrown() {
  output = 0;
  return *this;
}
  
AccessoryCommand& AccessoryCommand::makeStraight() {
  output = 1;
  return *this;
}

int AccessoryCommand::getDevice() {
  return 1 + (slot << 2) | part;  
}

boolean AccessoryCommand::isThrown() {
  return output > 0;
}

boolean AccessoryCommand::isStart() {
  return activate == 0;
}

AccessoryCommand&  AccessoryCommand::start() {
  activate = 1;
  return *this;
}


AccessoryCommand&  AccessoryCommand::stop() {
  activate = 0;
  return *this;
}



