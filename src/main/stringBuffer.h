#ifndef __STRINGBUFFER_H__
#define __STRINGBUFFER_H__

#include <stdio.h>
#include <math.h>

// It's like StringBuilder for poor people!
// Note, the length specified is always effectively n-1 to account for the terminator char (added automatically)

class StringBuffer {
  public:
    StringBuffer(int length)
      : mLen(length) {
      mpBuffer = new char[length];      
      clear();
    }

    void clear() {
      memset(mpBuffer, '\0', mLen);
      mOffset = 0;
    }

    // Appends a non-terminated single char to the buffer
    void append(const char c) {
      mpBuffer[mOffset] = c;
      mOffset++;
    }

    // Appends the null terminated string to the buffer.
    // No bounds check done here, don't mess this up.
    void append(const char *pStr) {
      int sLen = strlen(pStr);
      strncpy(mpBuffer + mOffset, pStr, sLen);
      mOffset += sLen;
    }

    char* c_str() {
      return mpBuffer;
    }

    int length() {
      return strlen(mpBuffer);
    }

  private:
    char *mpBuffer;
    int mLen;
    int mOffset;
};

#endif
