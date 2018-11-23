#define SERIAL9
#define DEBUG_MEM

#include <HardwareSerial.h>

#include "Common.h"
#include "XPressNet.h"
#include "RingBuffer.h"
#include "Scheduler.h"

byte sendBufferStorage[100];
RingBuffer sendRingBuffer(sendBufferStorage, 100);

XPressNet xnet(sendRingBuffer, Serial1, 2, 5);

//void serialEvent1() {
//  xnet.receiveEvent();
//}

unsigned long currentMillis = 0;

void setup() {
//  pinMode(18, OUTPUT);
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  // put your setup code here, to run once:
  Serial.begin(115200);

  // Initialize XPressNet
  Serial1.begin(62500, SERIAL_9N1);

  Serial.println("Booting");
//  pinMode(2, OUTPUT);
//  digitalWrite(2, LOW);

  xnet.setup();
  Scheduler2::boot();
  bootShunter();
  initTerminal();
  setupTerminal();

}

void loop() {
  // put your main code here, to run repeatedly:
  currentMillis = millis();
  xnet.receiveEvent();
  scheduler.schedulerTick();
  processTerminal();
}

