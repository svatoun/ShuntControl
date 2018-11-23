#ifndef __loco_manager_h__
#define __loco_manager_h__

#include "Notifications.h"

const int locoPoolSize = 10;

class LocoManager;

class Locomotive {
friend class LocoManager;
public:
  enum Steps : byte {
    step14,
    step27,
    step28,
    step128
  };
  
private:
  int     locoId;
  byte    speed : 5;
  byte    direction: 1;
  Steps   steps : 2;
  // fn0...fn28
  byte function[4];

  boolean speedKnown : 1;
  boolean functionsKnown : 1;
  byte    refs;

  void    setSpeed(int spd) { speed = spd; }
  void    setDirection(boolean dir) { direction = dir; };
  void    setSteps(Steps s) { steps = s; }
  void    addRef() { if (refs < 0xff) refs++; }

  void    clear();
public:
  Locomotive() : Locomotive(-1) {}
  Locomotive(int id) : locoId(id), speed(0x1f), refs(0) {}
  int     id() { return locoId; };
  boolean isSpeedInfo() { return speed != 0x1f; };
  boolean isFunctionInfo() { return functionsKnown; }
  
  byte  getSpeed() { return speed; }
  Steps getSteps() { return steps; }

  boolean isFunctionOn(int f) {
    return isFunctionInfo() && (function[f >> 3] & (1 << (f & 0x07)));
  }

  boolean valid() { return locoId > 0; };
};

extern LocoManager locoManager;

class LocoManager {
private:
  static const Locomotive noLocomotive;
    
  Notifier  commandNotifier;
  Notifier  lockNotifier;
  Locomotive  locoPool[locoPoolSize];

  Locomotive& allocLocomotive(int locoId);

  /**
   * Zmeny provedene prave probihajicim commandem
   */
  Locomotive  currentChanges;
public:
  enum {
    allLocomotives = 0xffff
  };
  
  LocoManager(byte llistSize, ListItem* llist) : 
      commandNotifier(llistSize, llist, ListenerType::locomotive),
      lockNotifier(llistSize, llist, ListenerType::locoDelayed)
      {}

  Notifier&   getCommandNotifier() { return commandNotifier; }
  Notifier&   getLockNotifier() { return lockNotifier; }

  const Locomotive& getChanges() { return currentChanges; }

  void lockLocomotive(int loco, Listener& readyCallback);
  const Locomotive& getLocomotive(int id);

  void setSpeed(int locoId, int speed);
  void setSpeed(int locoId, int speed, int dir);
  void setSpeed(const Locomotive& loco, int speed, int dir);

  void locoSpeedReceived(Locomotive::Steps steps, int locoId, int speed, boolean dir);

  int state2Speed(int state) { return state & 0x1f; }
  int state2Dir(int state) { return state & 0x4000; }
};

#endif

