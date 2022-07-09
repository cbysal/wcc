#include "ASMWriter.h"

ASMWriter::ASMWriter(const string &asmFile, const vector<Symbol *> &consts,
                     const vector<Symbol *> &globalVars,
                     const vector<pair<Symbol *, vector<ASM *>>> &funcASMs) {
  this->isProcessed = false;
  this->asmFile = asmFile;
  this->consts = consts;
  this->globalVars = globalVars;
  this->funcASMs = funcASMs;
}

ASMWriter::~ASMWriter() {}

void ASMWriter::write() {
  FILE *file = fopen(asmFile.data(), "w");
  fprintf(file, "\t.arch armv7-a\n");
  fprintf(file, "\t.fpu vfpv4\n");
  fprintf(file, "\t.arm\n");
  fprintf(file, "\t.data\n");
  for (Symbol *symbol : consts) {
    int size = 4;
    for (int dimension : symbol->dimensions)
      size *= dimension;
    fprintf(file, "\t.global %s\n", symbol->name.data());
    fprintf(file, "\t.size %s, %d\n", symbol->name.data(), size);
    fprintf(file, "%s:\n", symbol->name.data());
    if (symbol->dimensions.empty())
      fprintf(file, "\t.word %d\n", symbol->iVal);
    else
      for (int i = 0; i < size / 4; i++)
        fprintf(file, "\t.word %d\n", symbol->iMap[i]);
  }
  for (Symbol *symbol : globalVars) {
    int size = 4;
    for (int dimension : symbol->dimensions)
      size *= dimension;
    fprintf(file, "\t.size %s, %d\n", symbol->name.data(), size);
    fprintf(file, "%s:\n", symbol->name.data());
    if (symbol->dimensions.empty())
      fprintf(file, "\t.word %d\n", symbol->iVal);
    else {
      if (symbol->iMap.empty())
        fprintf(file, "\t.space %d\n", size);
      else
        for (int i = 0; i < size / 4; i++)
          fprintf(file, "\t.word %d\n", symbol->iMap[i]);
    }
  }
  fprintf(file, "\t.text\n");
  for (pair<Symbol *, vector<ASM *>> &funcASM : funcASMs) {
    fprintf(file, "\t.global %s\n", funcASM.first->name.data());
    fprintf(file, "%s:\n", funcASM.first->name.data());
    for (unsigned i = 0; i < funcASM.second.size(); i++)
      fprintf(file, "%s\n", funcASM.second[i]->toString().data());
  }
  fprintf(file, "\t.section .note.GNU-stack,\"\",%%progbits\n");
  fclose(file);
}