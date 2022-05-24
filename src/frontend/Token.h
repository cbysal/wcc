#ifndef __TOKEN_H__
#define __TOKEN_H__

#include <string>
#include <unordered_map>

#include "Type.h"

using namespace std;

class Token {
public:
  unsigned line;
  unsigned column;
  Type type;
  int intValue;
  float floatValue;
  string stringValue;

  Token(int line, int column, Type type);
  Token(int line, int column, Type type, int value);
  Token(int line, int column, Type type, float value);
  Token(int line, int column, Type type, string value);
  ~Token();

  const string toString();
};

#endif