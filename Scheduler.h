#ifndef __Scheduler_h__
#define __Scheduler_h__

const int MAX_SCHEDULED_ITEMS = 16;
const int MILLIS_SCALE_FACTOR = 50;

extern unsigned long currentMillis;

class Scheduler2;
class ScheduledProcessor {
  public:
  virtual void  timeout(unsigned int data) = 0;
};

class TickProcessor {
  friend class Scheduler2;
private:
  TickProcessor*  nextTick;
public:
  TickProcessor() : nextTick(NULL) {}
  virtual void  tick() = 0;
};

struct ScheduledItem {
  unsigned int timeout;
  unsigned int data;
  ScheduledProcessor* callback;

  ScheduledItem() {
    timeout = 0;
    data = 0;
    callback = NULL;
  }
  ScheduledItem(unsigned int aT, ScheduledProcessor* aP, unsigned int aD):
    timeout(aT), callback(aP), data(aD) {}
};

class Scheduler2 {
  static TickProcessor* tickHead;
  static ScheduledItem work[];
  static byte scheduledBottom;
  static byte scheduledCount;
  static byte cancelled;
  static void printQ();
  static void compact(ScheduledItem* startAt);
public:
  Scheduler2() {}
  static void boot();
  void schedulerTick();
  static bool schedule(unsigned int timeout, ScheduledProcessor* callback, unsigned int data);
  static void cancel(ScheduledProcessor* callback, unsigned int data);
  static void addTick(TickProcessor *t);
  static void removeTick(TickProcessor* t);
};

extern Scheduler2 scheduler;

#endif

