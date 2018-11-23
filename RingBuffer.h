#ifndef __ringbuffer_h__
# define __ringbuffer_h__

class RingBuffer {
private:
  /**
   * the storage itself
   */
  byte  const *buffer;

  /**
   * Size of the storage
   */
  const int   bufSize;

  /**
   * Index of the unallocated start
   */
  int   head;

  /**
   * Index of the allocated start
   */
  int   tail;

  /**
   * Number of allocated bytes
   */
  int   count;

  /**
   * Slack space at the end of the buffer
   */
  byte  slackAtEnd;

  void printBufStats();
public: 
    RingBuffer(byte* storage, int aSize) : 
    buffer(storage), bufSize(aSize),
    head(0), tail(0), slackAtEnd(0), count(0) {
  }
  /**
   * Allocates a continuous storage of the given size.
   * Returns NULL, if the allocation faols
   */
  byte *malloc(byte size);

  /**
   * Frees the allocated space. The size must be the same as 
   * when space was originally allocated.
   */
  void free();

  boolean available() { return count > 0; };

  int queued() { return count; };

  boolean hasSpace() { return count < bufSize; };

  const byte *current();

  int currentSize();
};




#endif // __ringbuffer_h__
