#ifndef __Shunter_h__
#define __Shunter_h__

#include "Scheduler.h"
#include "Notifications.h"

struct SplitShunterConfig {
  enum {
      noId = -1
  };
  /**
   * ID lokomotivy, kterou se ma posunovat
   */
  int   locoId;

  /**
   * Smer pohybu lokomotivy pri posunu; false dopredu, true dozadu
   */
  boolean locoDirection;
  
  /**
   * Cislo rozpojovace
   */
  int   splitterId;

  /**
   * Poradove cislo vozidla ZA kterym se ma rozvesit. 
   * 1 znamena lokomotivu, rozpojeni hned za ni.
   */
  byte  splitAtVehicle;

  SplitShunterConfig() : locoId(noId), splitterId(noId), locoDirection(false), splitAtVehicle(0) {}
};

/**
 * Ridi posunovaci "operaci"
 */
class SplitShunter : public ScheduledProcessor, public Listener {
private:
  /**
   * ID lokomotivy, kterou se ma posunovat
   */
  int   locoId;

  /**
   * Smer pohybu lokomotivy pri posunu; false dopredu, true dozadu
   */
  boolean locoDirection;

  /**
   * Zakladni rychlost posunu
   */
  byte  baseSpeed;

  /**
   * Maximalni cas pro pocatecni priblizeni lokomotivy k rozpojovaci
   */
  byte  timeToApproach;

  /**
   * Pocet sekund pro prejezd jednoho voziku
   */
  byte  timeForVehicle;

  /**
   * Cislo rozpojovace
   */
  int   splitterId;

  /**
   * Sensor, ktery indikuje mezeru nad rozpojovacem
   */
  int  pointSensorId;

  /**
   * Senzor na koleji s rozpojovacem; 
   */
  int  splitterTrackId;

  /**
   * Poradove cislo vozidla ZA kterym se ma rozvesit. 
   * 1 znamena lokomotivu, rozpojeni hned za ni.
   */
  byte  splitAtVehicle;

  /**
   * Cislo aktualniho 'vozitka'. Pri prvni detekci dostane cislo 1.
   */
  byte  counter;

  /**
   * Pocatecni cas, od ktereho se meri doba posunu "inkriminovaneho" vagonu
   */
  unsigned long baseTime;

  /**
   * Doba, za kterou prejede vagon, co mel byt odpojeny
   */
  unsigned int  vehicleMeasuredTime;

  /**
   * Doba ktera se od "casu na vagon" odecte; ta se postupne zvysuje
   */
  unsigned int  retractTime;
  
  byte retryCount;

  /**
   * Stavove priznaky, pro jednodussi uklid.
   */
  boolean locoMoving : 1;
  boolean splitterUp : 1;

  /**
   * Indikator, ze je rozpojovac "cisty" a je nad nim jen sprahlo. Jakmile se
   * nad nim objevi vozitko, nastavi se. Priznak se shazuje pri odpocitani vozu
   */
  boolean splitterOccupied : 1;

  void locoImmediateStop();

  void checkSensorsUp();
  
  boolean onLocoApproached(boolean state);
  void doVehicleStarted();
  void onVehiclePassed();
  void onVehicleStarted();
  void startLocoApproach();

  void makeSplitterOff();
  void onSplitSuccessful();
  void onSplitterRaised();

  void sensorMoveForward();
  void sensorMoveBack();
  void onTrainRetracted();
  void makeSplitterOn();
  void raiseAndMoveForward();

public:
  /**
   * Faze rozpojeni. 
   * locoApproaching = Priblizovani lokomotivy, dokud sensor nevyhlasi pritomnost prekazky
   * countingVehicle, countingJoin
   * 
   * locoApproaching -> [1]countingVehicle -> countingJoin -> [1]
   * countingJoin -> stopRaiseSplitter -> splitMoveForward -> {end}
   * splitForwardError -> splitRetract -> splitMoveForward
   */
  enum Phase : uint8_t {
      idle,             // necinny                                          0
      sensorInit,       // pocatecni scan senzoru
      locoApproaching,  // lokomotiva prijizdi k rozprahovaci
      countingVehicle,  // zapocitane vozidlo
      countingJoin,     // zapocitana mezera / sprahlo
      stopRaiseSplitter,     // zastavime soupravu, zvedame rozpojovac      5
      splitMoveForwardSmall,   // posun kupredu, maly (mezera mezi vagony)
      splitMoveForward,        // posun kupredu                             7
      splitForwardError,  // posun se nezdaril
      splitRetract,       // suneme zpatky k rozpojovaci

      splitterOff   //                                                      10
  };

private:
  Phase phase;

  void setPhase(Phase p);
public:
  SplitShunter() : baseSpeed(4), timeToApproach(10), timeForVehicle(10) {
    clear();
  };

  SplitShunter& locomotive(int id) { locoId = id; return *this; };
  SplitShunter& direction(boolean b) { locoDirection = b; return *this; };
  SplitShunter& splitter(int id) { splitterId= id; return *this; };
  SplitShunter& splitAt(byte index) { splitAtVehicle = index; return *this; };
  SplitShunter& pointSensor(int s) { pointSensorId = s; return *this; };
  SplitShunter& speed(int s) { baseSpeed = s; return *this; };

  void clear();
  /**
   * True, pokud je posunovac volny
   */
  boolean available() {
    return locoId == SplitShunterConfig::noId;
  }

  virtual boolean objectChanged(ListenerType typeId, int select, int objectState, Notifier* source) override;
  virtual void  timeout(unsigned int data) override;

  boolean execute();

  void stop();
};

/*
inline SplitShunter::Phase& operator ++(SplitShunter::Phase& phase) {
  phase = SplitShunter::Phase(phase + 1);
  return phase;
}
*/

#endif
