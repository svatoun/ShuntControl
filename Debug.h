#ifndef __Debug_h__
#define __Debug_h__

// ladi ovladani
const boolean debugControl = true;

// vypisy pro XPressNet komunikaci jinych zarizeni: sniffer
const boolean debugXnetOtherDevices = false;

// vypisy o zmenach na S88
const boolean debugFeedbackChange = true;

// posluchaci udalosti (vyhybky, senzory)
const boolean debugListeners = false;

// ladici vypisy planovace udalosti
const boolean debugSchedule = false;

// ladici vypisy o accessory (feedback) state request/response
const boolean debugFeedbackInfo = false;

// "krokovaci" vypisy, ve vhodnych uzlovych bodech
boolean debugTrace = false;

// obecne ladici vypisy komunikace na XPressNet
const boolean debugXnetComm = false;
// vypisy jednotlivych CALL packetu
const boolean debugXnetCall = false;
// vypisy ze zpracovani feedback broadcast packetu
const boolean debugFeedbackBcastLow = false;
// ladeni kruhoveho odesilaciho bufferu
const boolean debugRingBuffer = false;
// prikaz zmeny rychlosti a smeru
const boolean debugLocoSpeed = false;
// odesilani prikazu do centraly
const boolean debugSendRequest = false;

// posunovaci algoritmus
const boolean debugShunter = false;

#endif

