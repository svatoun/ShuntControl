#include "LocoManager.h"

void Locomotive::clear() {
  locoId = 0;
  speed = 0;
  speedKnown = functionsKnown = false;
}

void LocoManager::lockLocomotive(int loco, Listener& readyCallback) {
  
  getLockNotifier().addListener(loco, readyCallback);
}

const Locomotive& LocoManager::getLocomotive(int id) {
  for (byte b = 0; b < locoPoolSize; b++) {
    Locomotive& l = locoPool[b];
    if (l.id() == id) {
      return l;
    }
  }
  return noLocomotive;
}

void LocoManager::locoSpeedReceived(Locomotive::Steps steps, int locoId, int speed, boolean dir) {
  Locomotive c(locoId);
  c.setSteps(steps);
  c.setSpeed(speed);
  c.setDirection(dir);
  currentChanges = c;
  getCommandNotifier().fire(speed | (dir ? 0x4000 : 0), locoId, 0xffff);
  currentChanges.clear();
}

