#ifndef __notifications_h__
#define __notifications_h__

typedef void (*NotificationFuncType)(int objectId, void* ptr);

enum class ListenerType : byte {
  empty,
  // ohlas obsazeni / detektory
  feedback,
  
  // pozice vyhybek
  turnouts,
  
  // prikazy lokomotivam
  locomotive,
  
  // ziskane informace o lokomotive
  locoDelayed,
  
  // ziskane info o stavu prislusenstvi
  turnoutDelayed,
  
  station
};


class Notifier;
class Listener {
public:
  virtual boolean objectChanged(ListenerType typeId, int select, int objectState, Notifier* source) = 0;
};

struct ListItem {
    Listener*     callback;
    ListenerType  typeId : 4;
    unsigned int  select : 12;
public:
    ListItem(ListenerType type, Listener* cb, unsigned int s) : typeId(typeId), callback(cb), select(s) {};
    ListItem() : typeId(ListenerType::empty), callback(NULL) {};
    void clear() { typeId = ListenerType::empty; callback = NULL; };
};

class Notifier {
  private:
    const ListenerType typeId;
    const byte  listSize;
    ListItem    *listHead;
  protected:
    virtual boolean matches(unsigned int key, unsigned int select, unsigned int selectMask);
  public:
    enum {
      selectAll = (1 << 12) - 1
    };
    Notifier(byte sz, ListItem* storage, ListenerType type);
    boolean addListener(unsigned int key, Listener& l);
    boolean addListener(Listener& l) { addListener(selectAll, l); }
    void removeListener(unsigned int key, Listener& l);
    boolean removedListener(Listener& l) { removeListener(selectAll, l); }
    void fire(int state, unsigned int select, unsigned int selectMask);
};

#endif // __notifications_h__

