#ifndef __xpressnet_h__
# define __xpressnet_h__

#include "Debug.h"
#include "Notifications.h"

const int maxReceivePackets = 20;
const int maxSendPackets = 20;
const int maxFeedbackListeners = 10;

// address of the 1st feedback
const int firstFeedback = 64 * 8;
const int feedbackCount = 64 * 8;

class RingBuffer;

enum XCommand : unsigned int {
  // Broadcasts
  trackPowerOff       = 0x6100,
  operationsResumed   = 0x6101,
  emergencyStop       = 0x8100,
  serviceModeEntryInfo = 0x6102,

  prgInfoShort        = 0x6112,
  prgInfoBusy         = 0x6113,
  prgInfoReady        = 0x611f,
  transferError       = 0x6180,
  stationBusy         = 0x6181,
  commandNotSupported = 0x6182,

  softwareVersion12   = 0x6310,
  softwareVersion3    = 0x6314,

  accDecoderResponse  = 0x4200,

  accDecoderInfo      = 0x42,
  accDecoderCommand   = 0x52,
};

static boolean isBroadcast(int callByte, byte headerByte) {
  return (callByte & 0x11f) == 0x100;
}

static boolean isFeedbackBroadcast(int callByte, byte headerByte) {
  return (callByte == 0xA0) && ((headerByte & 0xe0) == 0x40);
}

enum XInstruction : unsigned int {
  emergStopLocomotive3    = 0x9200,

  locoSpeedDirection14    = 0xe410,
  locoSpeedDirection27    = 0xe411,
  locoSpeedDirection28    = 0xe412,
  locoSpeedDirection128   = 0xe413,
};

inline byte headerFrom(XInstruction i) {
  return (i >> 8) & 0xff;
}

class XPressNetPacket {
private:
  byte  call;
  byte  header;
  
public:
  XPressNetPacket(byte h, int size) : header((h & 0xf0) | (size & 0x1f)) {};
  byte size() const { return header & 0x0f; }
  const byte& operator[] (const int index) const;
  const byte* data() const { return &header; }

  byte callByte() { return call; }
  byte headerByte() { return header; }
  byte ident() { return data()[1]; }

  /**
   * True, if this packet is a broadcast. Usable only for station
   * originated packets (which have call byte)
   */
  inline boolean isBroadcast() {
    return ::isBroadcast(call, header);
  }
  
  inline boolean isFeedback() {
    return isFeedbackBroadcast(call, header);
  }

  void printPacket();
};

#define READ_XNET_PACKET(var, packet, type) const type& var = (*(const type*)(&packet))


class FeedbackBroadcast : public XPressNetPacket {
private:
  const byte  data[16];
  FeedbackBroadcast() : XPressNetPacket(0x20, 0) {}
public:
  int module(byte i) const ; // module address (divided by 8)
  int start(byte i) const;  // first feedback
  int end(byte i) const;    // last feedback
  int count() const;  // number of feedbacks
  byte  bits(byte i) const; // bits of feedback group
  boolean feedbackModule(byte i) const; 
  boolean nibble(byte i) const; 
  byte mask(byte i) const; 
  
  boolean isSet(int id) const;
  boolean isSetIndex(int index) const;
};

inline boolean FeedbackBroadcast::feedbackModule(byte i) const {
  const byte* p = data + (i * 2) + 1;
  return ((*p) & 0x60) == 0x20;
}

inline boolean _feedbackNibble(byte b) {
  return b & 0x10;
}

inline int FeedbackBroadcast::module(byte i) const {
  if (i >= count()) {
    return 0;
  }
  const byte* p = data + (i * 2);
  return *p;
}

inline boolean FeedbackBroadcast::nibble(byte i) const {
  if (i >= count()) {
    return false;
  }
  const byte* p = data + (i * 2) + 1;
  return _feedbackNibble(*p);
}

inline byte FeedbackBroadcast::mask(byte i) const {
  const byte* p = data + (i * 2) + 1;
  boolean nibble = _feedbackNibble(*p);
  return nibble ? 0xf0 : 0x0f;
}

inline byte FeedbackBroadcast::bits(byte i) const {
  if (i >= count()) {
    return 0;
  }
  const byte* p = data + (i * 2) + 1;
  boolean nibble = _feedbackNibble(*p);
  if (nibble) {
    return (*p & 0x0f) << 4;
  } else {
    return (*p & 0x0f);
  }
}

inline int FeedbackBroadcast::start(byte i) const {
  if (i >= count()) {
    return 0;
  }
  const byte* p = data + (i * 2);
  int res = (*p++) << 3;
  if (_feedbackNibble(*p)) {
    res += 4;
  }
  return res;
}

inline int FeedbackBroadcast::end(byte i) const {
  return start(i) + 4;
}

inline FeedbackBroadcast::count() const {
  // do not count header
  return size() / 2;
}

const int _feedbackBitSize = (feedbackCount + 7) / 8;
const int _feedbackSlotCount = (feedbackCount + 3) / 4;
const int _feedbackSlotSize = (_feedbackSlotCount + 7) / 8;

class XPressNet {
private:
  enum RecvState {
    idle = 0,     // idle state
    header,       // expecting header
    payload,      // receiving payload
    checksum,     // payload done, expecting checksum
    skip,         // skipping data
    ignoreToMaster, // all data ignored until master speaks. Used to resynchronize after errors
  };
  const byte directionPin;
  const byte deviceId;

  ListItem  listeners[maxFeedbackListeners];
  Notifier  feedbackNotifier;

  // Ctena data se preskakuji, az do nasledujiciho Call byte
  boolean skipping : 1;
  // Data prichazeji od mastera
  boolean fromMaster;

  // Buffer pro prichozi data, postaci pro jediny packet, ktery
  // se stejne synchronne zpracuje. Low-level buffer pro prijem z USARTu
  // resi HardwareSerial.
  byte  recvPacket[20];
  // alias pro call byte
  byte& recvCallByte = recvPacket[0];
  // alias pro header byte
  byte& recvHeaderByte = recvPacket[1];
  // Muze byt pouzito pro interpretaci bufferu jako XPressnet packetu
  const XPressNetPacket& packet = *(const XPressNetPacket*)(void*)(&recvPacket[0]);

  // prubezna hodnota kontrolniho souctu
  byte computedXor;
  // pocet ocekavanych byte
  byte recvCount;
  // index do recvPacket kam se zaznamena dalsi byte
  byte recvIndex;
  // aktualni zarizeni. Pri sniffovani zde zustava zarizeni z naposledy zachyceneho
  // call byte
  byte currentDevice;
  
  RecvState state;

  void clear();
  
  RingBuffer& sendBuffer;
  HardwareSerial& comm;

  byte feedbackInitSlots[_feedbackSlotSize];
  byte feedbackState[_feedbackBitSize];

  void clearReceivedPacket();
  boolean checkReceivedPacket();

  void updateFeedbackState();
  void processStationPacket();
  void processDevicePacket();
  boolean verifyCallByte(int data);

  void setFeedback(int s, boolean state);
  void addFeedbackChange(int s, boolean state);
  boolean shouldPrintMsg() {
    return (deviceId == currentDevice) || debugXnetOtherDevices || (packet.isBroadcast() || packet.isFeedback());
  }
  void sendQueuedRequest();

  void processAccessoryResponse();
  void doProcessFeedback(const FeedbackBroadcast&);

  void processLocoSpeed();
  void processLocoFunction();
public:
  XPressNet(RingBuffer& send, HardwareSerial& serial, int directionPin, int addr);
  void setup();
  void receiveEvent();

  virtual boolean shouldSpyMessage(byte target, XCommand cmd) { return false; }

  boolean isFeedbackReady(int number) const;
  boolean isFeedback(int number) const;

  boolean  sendPacket(const XPressNetPacket& packet);

  Notifier& feedbackNotify() {
    return feedbackNotifier;
  }

  void  processWriteEmptyIrq();
};

inline boolean XPressNet::isFeedbackReady(int n) const {
  if (n >= feedbackCount) {
#ifdef DEBUG_MEM
    Serial.println("*** Feedback # high");
#endif
    return false;
  }
  byte si = n / 4;
  byte a = si >> 3;
  byte m = (1 << (si & 0x07));
#ifdef DEBUG_MEM
  if (a >= sizeof(feedbackInitSlots)) {
    Serial.print(F("*** Feedback # high 2: ")); Serial.print(n); Serial.print('-'); Serial.println(a);
  }
#endif
  return (feedbackInitSlots[a] & m) > 0;
}

inline boolean XPressNet::isFeedback(int s) const {
  if (s >= feedbackCount) {
#ifdef DEBUG_MEM
    Serial.println("*** Feedback # high 3");
#endif
    return false;
  }
  byte shift = s & 0x07; 
  byte pos = s >> 3;
#ifdef DEBUG_MEM
  if (pos >= sizeof(feedbackState)) {
    Serial.println("*** Feedback # high 4");
  }
#endif
  return (feedbackState[pos] & (1 << shift)) > 0;
}

class SoftwareVersionQuery : public XPressNetPacket {
private:
  byte    cmd;
public:
  SoftwareVersionQuery() : XPressNetPacket(0x21, 1), cmd(0x21) {}  
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FeedbackStateRequest : public XPressNetPacket {
private:
  byte    slot;
  byte    nibble : 1;
  byte    pad : 7;
public:
  FeedbackStateRequest() : XPressNetPacket(accDecoderInfo, 2), pad(0x40), nibble(1) {}

  FeedbackStateRequest& sensor(int f);
};

class AccessoryCommand : public XPressNetPacket {
private:
  byte    slot;

  byte    output: 1;
  byte    part : 2;
  byte    activate: 1;
  byte    pad : 4;
public:
  AccessoryCommand() : XPressNetPacket(0x52, 2), pad(0x08) {}

  AccessoryCommand& device(int id);
  AccessoryCommand& makeThrown();
  AccessoryCommand& makeStraight();
  AccessoryCommand& start();
  AccessoryCommand& stop();

  int getDevice();
  boolean isThrown();
  boolean isStart();
};

static_assert (sizeof(AccessoryCommand) == 4, "Invalid size of accessory command");

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Locomotive commands
class LocomotiveEmergencyStop : public XPressNetPacket {
private:
  byte    locoHigh;
  byte    locoLow;
public:
    LocomotiveEmergencyStop(int lid) : XPressNetPacket(0x92, 2), locoHigh(lid >> 8), locoLow(lid & 0xff) {};
};

class LocomotiveSpeedAndDirection : public XPressNetPacket {
private:
  byte    operId;
  byte    locoHigh;
  byte    locoLow;

  byte  spd  : 7;
  byte  dir  : 1;

  
public:
  LocomotiveSpeedAndDirection() : LocomotiveSpeedAndDirection(0) {}
  LocomotiveSpeedAndDirection(int lid) : XPressNetPacket(headerFrom(locoSpeedDirection28), 4), operId(locoSpeedDirection28 & 0xff), 
    locoHigh(lid >> 8), locoLow(lid & 0xff) {};
  void setInstruction(XInstruction x);
  LocomotiveSpeedAndDirection& speed(int n);
  LocomotiveSpeedAndDirection& direction(boolean backwards);
};

static_assert (sizeof(LocomotiveEmergencyStop) == 4, "Invalid size of emergency stop");
static_assert (sizeof(LocomotiveSpeedAndDirection) == 6, "Invalid size of locomotive speed");

#endif // __xpressnet_h__

