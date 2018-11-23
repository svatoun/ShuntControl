
#include "Scheduler.h"

Scheduler2 scheduler;

long baseMillis = 0;

static ScheduledItem Scheduler2::work[MAX_SCHEDULED_ITEMS];
byte Scheduler2::scheduledBottom = 0;
byte Scheduler2::scheduledCount = 0;
byte Scheduler2::cancelled = 0;
TickProcessor* Scheduler2::tickHead = NULL;

#define MILLIS_FIXUP_THRESHOLD 15000

void Scheduler2::boot() {
}

bool Scheduler2::schedule(unsigned int timeout, ScheduledProcessor* callback, unsigned int data) {
  if (scheduledCount >= MAX_SCHEDULED_ITEMS) {
    return false;
  }
  if (callback == NULL) {
    return false;
  }
  timeout = timeout / MILLIS_SCALE_FACTOR;
  ScheduledItem* top = work + MAX_SCHEDULED_ITEMS;
  ScheduledItem* item = work + scheduledBottom;
  if (debugSchedule) {
    Serial.print(F("Sched-T:")); Serial.print(timeout); Serial.print(F(" CB:")); Serial.print((int)callback, HEX); Serial.print(F(" D:")); Serial.println(data, HEX);
    Serial.print(F("Bot:")); Serial.println(scheduledBottom);
    printQ();
  }
  if (scheduledCount == 0) {
    baseMillis = currentMillis;
    if (debugSchedule) {
      Serial.print(F("BMil:")); Serial.println(baseMillis);
    }
  }
  long x = (currentMillis - baseMillis) / MILLIS_SCALE_FACTOR;
  x += timeout;
  if (debugSchedule) {
    Serial.print(F("T:")); Serial.println(x);
  }
  if (scheduledCount > 0) {
    const ScheduledItem* maxItem = work + scheduledCount;
    while (item < maxItem) {
      if (item->timeout > x) {
        memmove(item + 1, item, (maxItem - item) * sizeof(ScheduledItem));
        break;
      }
      item++;
    }
    if (debugSchedule) {
      printQ();
    }
  }
  ScheduledItem nItem(x, callback, data);
  if (x > 32767) {
    return;
  }
  int id = item - work;
  *item = nItem;
  scheduledCount++;
  if (debugSchedule) {
    Serial.print(F("Sched@")); Serial.println(id);
  }
  printQ();
  return true;
}

void Scheduler2::schedulerTick() {
  if (scheduledCount == 0) {
    return;
  }
  int delta = (currentMillis - baseMillis) / MILLIS_SCALE_FACTOR;

  if (debugTrace) {
    Serial.println("Sched: tick");
  }

  if (cancelled > 0) {
    if (debugSchedule) {
      Serial.print(F("Compacting"));
    }
    compact(work);
    printQ();
  }
  ScheduledItem* item = work;
  signed char idx;
  for (idx = 0; idx < scheduledCount; idx++) {
    if (item->timeout > delta) {
      break;
    }
    item++;
  }
  if (idx == 0) {
    // check if baseMillis is not TOO low
    if (delta > MILLIS_FIXUP_THRESHOLD) {
      item = work + scheduledCount - 1;
      if (debugSchedule) {
        Serial.print(F("Fixup:")); Serial.println(delta);
      }
      while (item >= work) {
        item->timeout -= delta;
        item--;
      }
      baseMillis = currentMillis;
    }
    if (debugTrace) {
      Serial.println("Sched: Endtick 2");
    }
    return;
  }
  
  scheduledBottom = idx;
  if (debugSchedule) {
    Serial.print(F("Exp:")); Serial.println(idx);
  }
  ScheduledItem* expired = work;
  for (signed char x = 0; x < idx; x++) {
    if (debugSchedule) {
      Serial.print('#'); Serial.println(x);
    }
    ScheduledProcessor* cb = expired->callback;
    if (cb != NULL) {
      cb->timeout(expired->data);
    }
    expired->callback = NULL;
    expired++;
  }
  if (debugSchedule) {
    Serial.print(F("Expired cnt: ")); Serial.println(expired - work);
    printQ();
  }
  

  for (TickProcessor* p = tickHead; p != NULL; p = p->nextTick) {
    p->tick();
  }
  
  if (debugSchedule) {
    Serial.print(F("Scheduled cnt: ")); Serial.println(scheduledCount);
  }
  compact(expired);
  // collect the non-expired at the start:
  scheduledBottom = 0;
  printQ();
  if (debugTrace || debugSchedule) {
    Serial.println("EndTick");
  }
}

void Scheduler2::compact(ScheduledItem* expired) {
  byte nCount = 0;
  const ScheduledItem* top = work + MAX_SCHEDULED_ITEMS;
  ScheduledItem* dest = work;
  for (; expired < top; expired++) {
      if (expired->callback != NULL) {
        if (dest < expired) {
          *dest = *expired;
        }
        dest++;
        nCount++;
      }
  }
  // erase the rest
  while (dest < top) {
    dest->callback = NULL;
    dest->timeout = 0;
    dest++;
  }
  scheduledCount = nCount;
  cancelled = 0;
}

void Scheduler2::printQ() {
  if (debugSchedule) {
    Serial.print(F("Sched queue, count=")); Serial.print(scheduledCount); Serial.print(F(" bottom=")); Serial.println(scheduledBottom);
    for (int i = 0; i < scheduledCount; i++) {
      const ScheduledItem& s = work[i];
      Serial.print(F("I:")); Serial.print(i); Serial.print('\t');
      Serial.print(F("T:")); Serial.print(s.timeout); Serial.print(F(" A:")); Serial.print((unsigned int)s.callback, HEX); Serial.print(F(" D:")); Serial.println(s.data, HEX);
    }
  }
}

void Scheduler2::cancel(ScheduledProcessor* callback, unsigned int data) {
  if (debugSchedule) {
    Serial.print(F("SchCancel:")); Serial.print((int)callback, HEX); Serial.print(':'); Serial.print(data, HEX); Serial.print(':'); Serial.println(scheduledCount);
    printQ();
  }
  ScheduledItem* item = work + scheduledBottom;
  const ScheduledItem* top = work + MAX_SCHEDULED_ITEMS;
  for (int id = 0; item < top; id++) {
      if ((item->callback == callback) && (item->data == data)) {
        item->callback = NULL;
        cancelled++;
        if (debugSchedule) {
          Serial.print(F("Del:")); Serial.print(id); Serial.print(','); Serial.print(scheduledCount); Serial.print(','); Serial.print((unsigned int)callback, HEX); Serial.print('/'); Serial.println(data);
          printQ();
        }
        return;
      }
      item++;
  }
}

void Scheduler2::addTick(TickProcessor* t) {
  t->nextTick = tickHead;
  tickHead = t;
}

void Scheduler2::removeTick(TickProcessor* t) {
  if (t == NULL) {
    return;
  }
  TickProcessor *n = t->nextTick;
  t->nextTick = NULL;
  if (tickHead == t) {
    tickHead = n;
    return;
  } 
  for (TickProcessor *p = tickHead; p != NULL; p = p->nextTick) {
    if (p->nextTick == t) {
      p->nextTick = n;
      return;
    }
  }
}

