#include <algorithm>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <unordered_set>
#include <vector>

#include "GlobalData.h"
#include "backend/ASMOptimizer.h"
#include "backend/ASMParser.h"
#include "backend/ASMWriter.h"
#include "frontend/ASTOptimizer.h"
#include "frontend/IROptimizer.h"
#include "frontend/IRParser.h"
#include "frontend/LexicalParser.h"
#include "frontend/SSAOptimizer.h"
#include "frontend/SyntaxParser.h"

using namespace std;

LexicalParser lexicalParser;
SyntaxParser syntaxParser;
ASTOptimizer astOptimizer;
IRParser irParser;
SSAOptimizer ssaOptimizer;
IROptimizer irOptimizer;
ASMParser asmParser;
ASMOptimizer asmOptimizer;
ASMWriter asmWriter;

int main(int argc, char *argv[]) {
  clock_t beginTime = clock();
  initGlobalData();
  if (argc > 3) {
    string source = argv[4];
    string target = argv[3];
    lexicalParser.setInput(source);
    lexicalParser.parse();
    syntaxParser.parseRoot();
    astOptimizer.optimize();
    irParser.parseRoot();
    ssaOptimizer.process();
    irOptimizer.optimize();
    asmParser.parse();
    asmOptimizer.optimize();
    asmWriter.setOutput(target);
    asmWriter.write();
  } else {
    lexicalParser.setInput(argv[1]);
    lexicalParser.parse();
    syntaxParser.parseRoot();
    astOptimizer.optimize();
    irParser.parseRoot();
    ssaOptimizer.process();
    irOptimizer.optimize();
    asmParser.parse();
    asmOptimizer.optimize();
    asmWriter.setOutput("test.s");
    asmWriter.write();
    cout << "----------------------------------------------------------------"
            "----------------"
         << endl;
    for (Symbol *cst : consts)
      cout << cst->toString() << endl;
    for (Symbol *cst : globalVars)
      cout << cst->toString() << endl;
    for (pair<Symbol *, vector<IR *>> func : funcIRs) {
      cout << func.first->name << endl;
      for (IR *ir : func.second)
        cout << ir->toString() << endl;
    }
    cout << endl;
  }
  clock_t endTime = clock();
  cerr << "Time: " << (endTime - beginTime) / 1000000.0 << "s" << endl;
  pid_t pid = getpid();
  cerr << "Mem: ";
  system(string("cat /proc/" + to_string(pid) +
                "/status | grep VmHWM | awk '{print $2 $3}' > /dev/fd/2")
             .data());
  return 0;
}