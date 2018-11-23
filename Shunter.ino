/**
 * - Pocatecni ziskani hodnot senzoru; pred zapocetim posunu se posle request, pocka na odpoved
 */
#include "Common.h"
#include "Shunter.h"

/**
 * Jak dlouho se ma jet kupredu, pokud byl rozpojovac prave pod
 * sprahlem. Jestlido teto doby prejede celnik vozu pres senzor, cas se prodlouzi
 * o dobu prejezdu vozu
 */
const int timeMoveForwardFromJoiner = 2000;

/**
 * Cas ktery se prida k casu prejezdu odpojovaneho vozu. Pokud do te doby nenajede nad senzor
 * mezera, je rozpojeno.
 */
const int timeAddForVehiclePass = 250;

/**
 * Cas pro zvednuti rozpojovace; behem te doby zustane souprava v klidu
 */
const int timeToRaiseSplitter = 1500;

/**
 * Jak dlouho ma zustat rozpojovac zvednuty behem jizdy vpred.
 */
const int splitterTimeUpAfterMove = 2000;

/**
 * Prida se k casu prejezdu vozu, aby sprahlo zastavilo aspon nejak
 * nad rozpojovacem.
 */
const int retractTimeAddOnce = 100;

/**
 * Kazdy 2. pokus se snizi cas prejezdu dozadu, tzn. sprahlo zastavi vic
 * vepredu na rozpojovaci.
 */
const int retractTimeSubtractUnit = 100;

extern XPressNet xnet;

/**
 * Instance se kterou se pracuje z terminalu.
 */
SplitShunter userShunter;

void SplitShunter::clear() {
  retryCount = 0;
  setPhase(idle);
  locoMoving = splitterUp = false;
  counter = 0;
  retryCount = 0;
  retractTime = 0;
  vehicleMeasuredTime = 0;
}

void SplitShunter::setPhase(Phase p) {
  phase = p;
  if (!debugShunter) {
    return;
  }
  Serial.print(F("* Phase: "));
  switch (p) {
      case idle: Serial.println(F("idle")); break;
      case sensorInit: Serial.println(F("sensorInit")); break;
      case locoApproaching: Serial.println(F("locoApproaching")); break;
      case countingVehicle: Serial.println(F("countingVehicle")); break;
      case countingJoin: Serial.println(F("countingJoin")); break;
      case stopRaiseSplitter: Serial.println(F("stopRaiseSplitter")); break;
      case splitMoveForwardSmall: Serial.println(F("splitMoveForwardSmall")); break;
      case splitMoveForward: Serial.println(F("splitMoveForward")); break;
      case splitForwardError: Serial.println(F("splitForwardError")); break;
      case splitRetract: Serial.println(F("splitRetract")); break;
      case splitterOff: Serial.println(F("splitterOff")); break;

      default:
        Serial.println(p);
        break;
  }
}

boolean SplitShunter::objectChanged(ListenerType typeId, int select, int objectState, Notifier* source) {
  if (debugShunter) {
    Serial.print(F("Sensor change: ")); Serial.print(select); Serial.print('='); Serial.println(objectState);
  }
  if (objectState) {
    splitterOccupied = true;
  }
  switch (phase) {
    case locoApproaching: 
      if (!onLocoApproached(objectState)) {
        stop();
        return true;
      }
      break;
    case countingVehicle:
      onVehiclePassed();
      break;
      
    case countingJoin:
      onVehicleStarted();
      break;

    case splitMoveForward:
    case splitMoveForwardSmall:
      sensorMoveForward();
      break;

    case splitRetract:
      sensorMoveBack();
      break;
  }
  return false;
}

void  SplitShunter::timeout(unsigned int data) {
  switch (phase) {
    case sensorInit:
      checkSensorsUp();
      break;
    case locoApproaching:
      stop();
      break;
    case stopRaiseSplitter:
      onSplitterRaised();
      break;
    case splitMoveForward:
      if (debugShunter) {    
        Serial.println(F("Move forward timeout start"));
      }
      if (data == splitterOff) {
        makeSplitterOff();
      } else {
        onSplitSuccessful();
      }
      break;
    case countingJoin:
    case countingVehicle:
      Serial.print(F("Err: not moving"));
      stop();
      break;
    case splitRetract:
      onTrainRetracted();      
  }
}

void SplitShunter::sensorMoveBack() {
  if (debugShunter) {
    Serial.println(F("Forward: sensor change"));
  }
  if (xnet.isFeedback(pointSensorId)) {
    // to je v poradku, vagon co sensor opustil se vraci.
    return;
  }
  // prisla mezera, to znamena, ze je sprahlo zase asi blbe, 
  // ale je treba skoncit s posunem
  scheduler.cancel(this, splitRetract);
  onTrainRetracted();
}

void SplitShunter::sensorMoveForward() {
  if (debugShunter) {
    Serial.println(F("Forward: sensor change"));
  }
  if (xnet.isFeedback(pointSensorId)) {
    if (debugShunter) {
      Serial.println(F("Vehicle start, prolonged"));
    }
    // Prijelo celo voziku; prodlouzime interval podle voziku
    scheduler.cancel(this, splitMoveForward);  
    scheduler.schedule(timeForVehicle * 1000, this, splitMoveForward); 
    if (phase == splitMoveForwardSmall) {
      setPhase(splitMoveForward);
      baseTime = currentMillis;
    }
    return;
  }
  scheduler.cancel(this, splitMoveForward);
  vehicleMeasuredTime = currentMillis - baseTime;
  retryCount++;

  if (debugShunter) {
    Serial.print(F("Vehicle ended, time = ")); Serial.print(vehicleMeasuredTime); Serial.print(F(", retry count:")); Serial.println(retryCount);
  }
  // cely nasledujici vuz odjel. Musime zastavit vlak, posunovat zpet
  
  if (retryCount > 5) {
    Serial.print(F("*** Split failed, aborted"));
    stop();
    return;
  }
  if (debugShunter) {
    Serial.println(F("Loco: going back"));
  }
  LocomotiveSpeedAndDirection locoBack(locoId);
  locoBack.speed(baseSpeed).direction(!locoDirection);
  setPhase(splitRetract);

  locoMoving = true;
  xnet.sendPacket(locoBack);
  xnet.sendPacket(locoBack);

  if (!(retryCount & 1)) {
    retractTime += 100;
  }
  unsigned int rtime = vehicleMeasuredTime + retractTimeAddOnce - retractTime;
  if (debugShunter) {
    Serial.print(F("Loco: Measured vehicle: ")); Serial.print(vehicleMeasuredTime); Serial.print(F(", Retracting for ")); Serial.println(rtime);
  }
  scheduler.schedule(rtime, this, splitRetract);
  makeSplitterOn();
}

void SplitShunter::onTrainRetracted() {
  if (debugShunter) {
    Serial.println(F("Train retracted, repeating"));
  }
  onSplitterRaised();
}

void SplitShunter::makeSplitterOn() {
  if (debugShunter) {
    Serial.println(F("Raiding splitter"));
  }
  AccessoryCommand splitCmd;
  
  splitCmd.device(splitterId).makeThrown().start();
  xnet.sendPacket(splitCmd);

  splitCmd.stop();
  xnet.sendPacket(splitCmd);
  splitterUp = true;
}


void SplitShunter::makeSplitterOff() {
  if (debugShunter) {
    Serial.println(F("Lowering splitter"));
  }
  AccessoryCommand splitCmd;
  
  splitCmd.device(splitterId).makeStraight().start();
  xnet.sendPacket(splitCmd);

  splitCmd.stop();
  xnet.sendPacket(splitCmd);
  splitterUp = false;
  // do not change phase
}

void SplitShunter::onSplitSuccessful() {
  Serial.println("Split success");
  stop();
}

void SplitShunter::onSplitterRaised() {
  if (debugShunter) {
    Serial.println(F("Splitter raised, moving forward"));
  }
  LocomotiveSpeedAndDirection locoStart(locoId);
  locoStart.speed(baseSpeed).direction(locoDirection);

  setPhase(splitMoveForward);
  xnet.sendPacket(locoStart);
  xnet.sendPacket(locoStart);

  locoMoving = true;

  unsigned int movingTime;
  baseTime = currentMillis;
  if (splitterOccupied || xnet.isFeedback(pointSensorId)) {
    if (debugShunter) {
      Serial.println(F("Vehicle above splitter"));
    }
    if (vehicleMeasuredTime == 0) {
      // jeste nic neprejelo, nemerilo se.
      movingTime = 1000 * timeForVehicle;
    } else {
      movingTime = vehicleMeasuredTime + timeAddForVehiclePass;
    }
    
  } else {
    if (debugShunter) {
      Serial.println(F("Join above splitter"));
    }
    movingTime = timeMoveForwardFromJoiner;
  }
  scheduler.schedule(splitterTimeUpAfterMove, this, splitterOff);
  
  scheduler.schedule(movingTime, this, splitMoveForward);
  if (movingTime == splitterTimeUpAfterMove) {
//    debugSchedule = true;
  }
}

boolean SplitShunter::onLocoApproached(boolean sensorState) {
  if (debugShunter) {
    Serial.print(F("Loco approached: ")); Serial.println(sensorState);
  }
  if (!sensorState) {
    return false;
  }
  setPhase(phase + 1);
  scheduler.cancel(this, locoApproaching);
  counter = 1;
  doVehicleStarted();
  return true;
}

/**
 * Zavola se kdyz odjede vuz. Pokud je counter == splitAtVehicle, tedy
 * se ma rozpojovat, prejde se do dalsi faze.
 */
void SplitShunter::onVehiclePassed() {
  if (debugShunter) {
    Serial.print(F("Vehicle passed:")); Serial.print(counter); Serial.print(' '); Serial.println(phase);
  }
  if (counter < splitAtVehicle) {
    setPhase(phase + 1);
    // nechame timer bezet, musi poodjet pripadne i 
    return;
  }
  if (debugShunter) {
    Serial.println(F("Vehicle reached, pause"));
  }
  splitterOccupied = false;
  scheduler.cancel(this, countingVehicle);
  locoImmediateStop();
  retractTime = 0;
  raiseAndMoveForward();
}

void SplitShunter::raiseAndMoveForward() {
  AccessoryCommand splitCmd;
  makeSplitterOn();
  setPhase(stopRaiseSplitter);
  scheduler.schedule(timeToRaiseSplitter, this, stopRaiseSplitter);
}

/**
 * Zacatek dalsiho vozu. Zrusi se predchozi timeout a nastavi se
 * novy.
 */
void SplitShunter::onVehicleStarted() {
  scheduler.cancel(this, countingVehicle);
  setPhase(countingVehicle);
  counter++;
  if (debugShunter) {
    Serial.print(F("Vehicle start:")); Serial.print(counter); Serial.print(' '); Serial.println(phase);
  }
  doVehicleStarted();
}

void SplitShunter::doVehicleStarted() {
  scheduler.schedule(timeForVehicle * 1000, this, countingVehicle);
}

void SplitShunter::checkSensorsUp() {
  boolean allUp = 
    xnet.isFeedbackReady(pointSensorId);
  if (allUp) {
    startLocoApproach();
    return;
  }
  if (--counter) {
    scheduler.schedule(100, this, sensorInit);
  } else {
    stop();
  }
}

void SplitShunter::startLocoApproach() {
  if (debugShunter) {
    Serial.print(F("Approach: t=")); Serial.print(timeToApproach); Serial.print(F(" point=")); Serial.print(pointSensorId); Serial.print(F(" loco="));  Serial.println(locoId);
  }
  setPhase(locoApproaching);

  LocomotiveSpeedAndDirection locoStart(locoId);
  locoStart.speed(baseSpeed).direction(locoDirection);
  // timeout pro vyhlaseni chyby, kdyz loko k senzoru nedojede

  if (debugShunter) {
    Serial.print(F("Approach timeout:")); Serial.println(timeToApproach * 1000);
  }
  scheduler.schedule(timeToApproach * 1000, this, locoApproaching);
  
  // bude se monitorovat senzor
  xnet.feedbackNotify().addListener(pointSensorId, *this);

  xnet.sendPacket(locoStart);
  xnet.sendPacket(locoStart);
  locoMoving = true;
}

class X : public ScheduledProcessor {
public:
  virtual void timeout(unsigned int data) override {
    userShunter.execute();      
  };
};
X x;

boolean SplitShunter::execute() {
  if (phase != idle) {
    Serial.println(F("Err: Shunt in progress"));
    return;
  }
  if (xnet.isFeedbackReady(pointSensorId)) {
    startLocoApproach();
    return;    
  }
  setPhase(sensorInit);
  // start waiting, in 100ms increments for sensors to be reported, at most 500ms
  scheduler.schedule(100, this, sensorInit);
  FeedbackStateRequest rq;
  rq.sensor(pointSensorId);
  xnet.sendPacket(rq);
}

void SplitShunter::locoImmediateStop() {
  /*
    LocomotiveSpeedAndDirection locoStop(locoId);
    locoStop.speed(-1);
    */
    LocomotiveEmergencyStop locoStop(locoId);
    xnet.sendPacket(locoStop);
    xnet.sendPacket(locoStop);
    locoMoving = false;
}

void SplitShunter::stop() {
  Serial.println(F("*** STOP"));
  xnet.feedbackNotify().removeListener(pointSensorId, *this);
  if (locoMoving) {
    Serial.println(F("Stopping loco"));
    locoImmediateStop();
  }
  if (splitterUp) {
    makeSplitterOff();
  }
  scheduler.cancel(this, splitterOff);
  switch (phase) {
    case locoApproaching:
      if (debugShunter) {
        Serial.println(F("Cancel approach timeout"));
      }
      scheduler.cancel(this, locoApproaching);
      break;  
  case countingJoin:
  case countingVehicle:
  if (debugShunter) {
      Serial.println(F("Cancel vehicle timeout"));
    }
    scheduler.cancel(this, phase);
    break;
  case splitRetract:
    if (debugShunter) {
      Serial.println(F("Cancel vehicle retract"));
    }
    scheduler.cancel(this, phase);
    break;
  }
  Serial.println("*** Shunter stopped");
  clear();
}


/////////////////////////////////////////////////////////////////////////////////

void bootShunter() {
  registerLineCommand("SSET", &commandSplitSet);
  registerLineCommand("SSTOP", &commandSplitStop);
  registerLineCommand("SPLIT", &commandSplitExec);  
  registerLineCommand("LRUN", &locoRun);
  registerLineCommand("LSTOP", &locoStop);
  
  userShunter.
    locomotive(6).
    direction(false).
    speed(3).
    splitter(12).
    pointSensor(8).
    splitAt(2);
}

void commandSplitSet() {
  int loco = nextNumber();
  int splitter = nextNumber();
  int vehicles = nextNumber();
  if (loco < 1) {
    Serial.println(F("Bad loco."));
    return;
  }
  if (splitter < 1) {
    Serial.println(F("Bad splitter."));
    return;
  }
  if (vehicles < 1) {
    Serial.println(F("Bad count."));
    return;
  }
}

void commandSplitStop() {
  userShunter.stop();
}

void commandSplitExec() {
  userShunter.execute();
//    scheduler.schedule(10000, &x, 0);
}

void locoStop() {
  int locoId = nextNumber();
  LocomotiveSpeedAndDirection locoStart(locoId);
  locoStart.speed(-1);
  xnet.sendPacket(locoStart);

}

void locoRun() {
  int locoId = nextNumber();
  int spd = nextNumber();
  int dir;
  switch (*inputPos) {
    case '-':
    case '0':
    case 'b': case 'B':
      dir = 1;
      break;
    default:
      dir = 0;
      break;
  }
  if (locoId < 1 || spd < 1 || spd > 28) {
    Serial.println("Bad input.");
    return;
  }
  LocomotiveSpeedAndDirection locoStart(locoId);
  locoStart.speed(spd).direction(dir);

  locoStart.printPacket();
}

