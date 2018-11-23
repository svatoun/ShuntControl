
#include "Notifications.h"

Notifier::Notifier(byte sz, ListItem* storage, ListenerType type) : listHead(storage), listSize(sz), typeId(type) {
  ListItem* ptr = listHead; 
  for (byte c = sz; c > 0; (ptr++)->clear(), c--) ;
}

boolean Notifier::addListener(unsigned int key, Listener& l) {
  if (debugListeners) {
    Serial.print(F("New listener: ")); Serial.print((unsigned int)&l, HEX); Serial.print('/'); Serial.print(key, HEX);
    Serial.print(F(" notifier: ")); Serial.println((unsigned int)this, HEX);
  }
  Listener* n = &l;
  ListItem* ptr = listHead; 
  ListItem* firstNull = NULL; 
  ListItem nItem(typeId, n, key);
  boolean slot = true;
  
  for (byte c = listSize; c > 0; ptr++, c--) {
    Listener* t = ptr->callback;
    if (t == n && ptr->select == key) {
      if (debugListeners) {
        Serial.print(listSize - c); Serial.println(F(": Already present"));
      }
      return false;
    } else if (t == NULL) {
      if (debugListeners) {
          Serial.print(listSize - c); Serial.println(F(": Empty")); 
      }
      if (slot) {
        firstNull = ptr;
        slot = false;
      }
    } else {
      if (debugListeners) {
        Serial.print(listSize - c);
        Serial.print(F(": Present: "));
        Serial.print((int)t, HEX); Serial.print('/'); Serial.println(ptr->select);
      }
    }
  }
  if (firstNull == NULL) {
    return false;
  }
  if (debugListeners) {
    Serial.print(firstNull - listHead);
    Serial.println(F(": Added"));
  }
  *firstNull = nItem;
  return true;
}

boolean Notifier::matches(unsigned int key, unsigned int select, unsigned int selectMask) {
  if ((key == selectAll) || (select == selectAll)) {
    return true;
  } else {
    return (key == (select & selectMask));
  }
}

void Notifier::removeListener(unsigned int key, Listener& l) {
  if (debugListeners) {
    Serial.print(F("Remove listener: ")); Serial.print((unsigned int)&l, HEX);
    Serial.print(F(" notifier: ")); Serial.print((unsigned int)this, HEX);
    Serial.print(F(" key: ")); Serial.println((unsigned int)key, HEX);
  }
  Listener* n = &l;
  ListItem* ptr = listHead; 
  for (byte c = listSize; c > 0; ptr++, c--) {
    if (ptr->typeId ==typeId && ptr->callback == n) {
      if (matches(key, ptr->select, selectAll)) {
        if (debugListeners) {
          Serial.print(listSize - c); Serial.print(F(": Found ")); 
          Serial.println(ptr->select, HEX);
        }
        ptr->clear();
      } else {
        if (debugListeners) {
          Serial.print(listSize - c); Serial.print(F(": Not match ")); 
          Serial.println(ptr->select, HEX);
        }
      }
    } else {
        if (debugListeners) {
          Serial.print(listSize - c); Serial.print(F(": Registered ")); 
          Serial.print((unsigned int)ptr->callback, HEX); Serial.print('/'); Serial.println(ptr->select, HEX);
        }
    }
  }
}

void Notifier::fire(int state, unsigned int select, unsigned int selectMask) {
  if (debugListeners) {
    Serial.print(F("Fire: ")); Serial.print((byte)typeId, HEX);
    Serial.print(':'); Serial.print(select, HEX); Serial.print('/'); Serial.print(selectMask, HEX);
    Serial.print(F(" = ")); Serial.println(state);
  }
  ListItem* ptr = listHead; 
  for (byte c = listSize; c > 0; ptr++, c--) {
    if (ptr->callback == NULL) {
      continue;
    }
    unsigned int s = ptr->select;
    if (matches(select, s, selectMask)) {
        if (debugListeners) {
          Serial.print(listSize - c); Serial.print(F(": Found ")); Serial.print((unsigned int)ptr->callback, HEX); Serial.print('/');
          Serial.println(ptr->select, HEX);
        }
        if (ptr->callback->objectChanged(typeId, select, state, this)) {
          if (debugListeners) {
            Serial.print(listSize - c); Serial.print(F(": Cleared ")); Serial.print((unsigned int)ptr->callback, HEX); Serial.print('/');
            Serial.println(ptr->select, HEX);
          }
          ptr->clear();
        }
    } else {
      if (debugListeners) {
        Serial.print(listSize - c); Serial.print(F(": Ignored ")); Serial.print((unsigned int)ptr->callback, HEX); Serial.print('/');
        Serial.println(s, HEX);
      }
    }
  }
}

