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
      append(new char[2] { c, '\0' });
    }

    // Appends the null terminated string to the buffer.
    void append(const char* pStr) {
      int sLen = strlen(pStr);      
      int copyLen = min(mLen - 1 - mOffset, sLen);

      // Note the mLen - 1 above, allow room for the null terminator
      strncpy(mpBuffer + mOffset, pStr, copyLen);
      mOffset += copyLen;      
    }

    char* c_str() {
      return mpBuffer;
    }

    int length() {
      return strlen(mpBuffer);
    }

  private:
    char* mpBuffer;
    int mLen;
    int mOffset;
};

#endif
