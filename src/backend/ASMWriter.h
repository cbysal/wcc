#ifndef __ASM_WRITER_H__
#define __ASM_WRITER_H__

#include <string>
#include <utility>
#include <vector>

#include "../frontend/Symbol.h"
#include "ASM.h"

class ASMWriter {
private:
  std::string asmFile;

public:
  void setOutput(const std::string &);
  void write();
};

#endif