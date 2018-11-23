
#include "Debug.h"
#include "RingBuffer.h"

byte *RingBuffer::malloc(byte size) {
  if (debugRingBuffer) {
    Serial.print(F("RB: malloc ")); Serial.println(size);
    printBufStats();
  }
  if (size == 0) {
    return NULL;
  }
  int s = size + 1;
  if (count + s > bufSize) {
    if (debugRingBuffer) {
      Serial.print(F("RB: overflow"));
    }
    return NULL;
  }
  byte *ptr;
  
  if (head + s > bufSize) {
    if (tail > head || tail < s) {
      if (debugRingBuffer) {
        Serial.print(F("RB: overflow 2"));
      }
      return NULL;
    }
    if (debugRingBuffer) {
      Serial.println(F("RB: rollover "));
    }
    if (tail != head) {
      slackAtEnd = head;
      count += (bufSize - head);
    } else {
      tail = 0;
    }
    head = 0;
  }
  count += s;
#ifdef DEBUG_MEM
  if (head >= bufSize - 1) {
      Serial.println("**** Ring buffer overflow 1");
  }
#endif
  ptr = buffer + head;
  if (debugRingBuffer) {
    Serial.print(F("RB: alloc @")); Serial.print(head);
  }
  *(ptr++) = size;
  head += s;
#ifdef DEBUG_MEM
  if (head > bufSize) {
      Serial.println("**** Ring buffer overflow 2");
  }
  if (((ptr - buffer) + size) > bufSize) {
      Serial.println("**** Ring buffer overflow 10");
  }
#endif
  if (debugRingBuffer) {
    printBufStats();
  }
  return ptr;
}

void RingBuffer::printBufStats() {
  Serial.print(F("\tbufsize:")); Serial.print(bufSize); Serial.print(F(" count:")); Serial.print(count);
  Serial.print(F(" head:")); Serial.println(head); Serial.print(F(" tail:")); Serial.print(tail);
  Serial.print(F(" slack:")); Serial.print(slackAtEnd); Serial.print(F(" count:")); Serial.println(count); 
}

const byte *RingBuffer::current() {
#ifdef DEBUG_MEM
  if (tail + 1 >= bufSize) {
      Serial.println("**** Ring buffer overflow 3");
  }
#endif
  return buffer + tail + 1;
}

int RingBuffer::currentSize() {
#ifdef DEBUG_MEM
  if (tail >= bufSize || (tail + *(buffer + tail) > bufSize)) {
      Serial.println("**** Ring buffer overflow 4");
  }
#endif
  return *(buffer + tail);
}

void RingBuffer::free() {
  if (count < 1) {
    return;
  }
#ifdef DEBUG_MEM
  if (tail >= bufSize || (tail + *(buffer + tail) > bufSize)) {
      Serial.println("**** Ring buffer overflow 5");
  }
#endif
  int size = *(buffer + tail);
  int s = size + 1;
  if (debugRingBuffer) {
    Serial.print(F("RB: free@")); Serial.print(tail); Serial.print(F(" : ")); Serial.print(s);
    printBufStats();
  }
  tail += s;
  count -= s;
  if (slackAtEnd > 0 && tail >= slackAtEnd) {
    count -= (bufSize - slackAtEnd);
    slackAtEnd = 0;
    tail = 0;
  }
  if (debugRingBuffer) {
    Serial.print(F("RB: free done: ")); Serial.print(s);
    printBufStats();
  }
}

