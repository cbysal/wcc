#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <unistd.h>

#include "backend/ASMOptimizer.h"
#include "backend/ASMParser.h"
#include "backend/ASMWriter.h"
#include "frontend/ASTOptimizer.h"
#include "frontend/IROptimizer.h"
#include "frontend/IRParser.h"
#include "frontend/LexicalParser.h"
#include "frontend/SyntaxParser.h"

using namespace std;

int main(int argc, char *argv[]) {
  clock_t beginTime = clock();
  if (argc > 3) {
    string source = argv[4];
    string target = argv[3];
    LexicalParser *lexicalParser = new LexicalParser(source);
    pair<unsigned, unsigned> lineno = lexicalParser->getLineno();
    vector<Token *> tokens = lexicalParser->getTokens();
    SyntaxParser *syntaxparser = new SyntaxParser(tokens);
    vector<Symbol *> symbols = syntaxparser->getSymbolTable();
    AST *root = syntaxparser->getAST();
    ASTOptimizer *astOptimizer = new ASTOptimizer(root, symbols);
    root = astOptimizer->getAST();
    IRParser *irParser = new IRParser(root, symbols);
    vector<Symbol *> consts = irParser->getConsts();
    vector<Symbol *> globalVars = irParser->getGlobalVars();
    unordered_map<Symbol *, vector<Symbol *>> localVars =
        irParser->getLocalVars();
    vector<pair<Symbol *, vector<IR *>>> funcs = irParser->getFuncs();
    unsigned tempId = irParser->getTempId();
    IROptimizer *irOptimizer =
        new IROptimizer(symbols, consts, globalVars, localVars, funcs, tempId);
    funcs = irOptimizer->getFuncs();
    ASMParser *asmParser =
        new ASMParser(lineno, funcs, consts, globalVars, localVars);
    vector<pair<Symbol *, vector<ASM *>>> funcASMs = asmParser->getFuncASMs();
    ASMOptimizer *asmOptimizer = new ASMOptimizer(funcASMs);
    funcASMs = asmOptimizer->getFuncASMs();
    ASMWriter *asmWriter = new ASMWriter(target, consts, globalVars, funcASMs);
    asmWriter->write();
    delete asmWriter;
    delete asmOptimizer;
    delete asmParser;
    delete irOptimizer;
    delete irParser;
    delete syntaxparser;
    delete lexicalParser;
  } else if (argc == 3) {
    LexicalParser *lexicalParser = new LexicalParser(argv[2]);
    pair<unsigned, unsigned> lineno = lexicalParser->getLineno();
    vector<Token *> tokens = lexicalParser->getTokens();
    SyntaxParser *syntaxparser = new SyntaxParser(tokens);
    vector<Symbol *> symbols = syntaxparser->getSymbolTable();
    AST *root = syntaxparser->getAST();
    ASTOptimizer *astOptimizer = new ASTOptimizer(root, symbols);
    root = astOptimizer->getAST();
    IRParser *irParser = new IRParser(root, symbols);
    vector<Symbol *> consts = irParser->getConsts();
    vector<Symbol *> globalVars = irParser->getGlobalVars();
    unordered_map<Symbol *, vector<Symbol *>> localVars =
        irParser->getLocalVars();
    vector<pair<Symbol *, vector<IR *>>> funcs = irParser->getFuncs();
    unsigned tempId = irParser->getTempId();
    IROptimizer *irOptimizer =
        new IROptimizer(symbols, consts, globalVars, localVars, funcs, tempId);
    funcs = irOptimizer->getFuncs();
    ASMParser *asmParser =
        new ASMParser(lineno, funcs, consts, globalVars, localVars);
    vector<pair<Symbol *, vector<ASM *>>> funcASMs = asmParser->getFuncASMs();
    ASMOptimizer *asmOptimizer = new ASMOptimizer(funcASMs);
    funcASMs = asmOptimizer->getFuncASMs();
    ASMWriter *asmWriter =
        new ASMWriter("test.s", consts, globalVars, funcASMs);
    asmWriter->write();
    for (Symbol *symbol : symbols)
      cout << symbol->toString() << endl;
    cout << "----------------------------------------------------------------"
            "----------------"
         << endl;
    irOptimizer->printIRs();
    delete asmWriter;
    delete asmOptimizer;
    delete asmParser;
    delete irOptimizer;
    delete irParser;
    delete syntaxparser;
    delete lexicalParser;
  } else {
    int item = stoi(argv[1]);
    int id = 0;
    string dir1 = "test_case/functional";
    string dir2 = "test_case/performance";
    vector<string> files;
    for (const filesystem::directory_entry &entry :
         filesystem::directory_iterator(dir1))
      files.push_back(entry.path());
    for (const filesystem::directory_entry &entry :
         filesystem::directory_iterator(dir2))
      files.push_back(entry.path());
    sort(files.begin(), files.end());
    for (const string &file : files) {
      if (file.length() < 3 || file.find(".sy") != file.length() - 3)
        continue;
      if (item >= 0 && id++ != item)
        continue;
      cout << file << endl;
      LexicalParser *lexicalParser = new LexicalParser(file);
      pair<unsigned, unsigned> lineno = lexicalParser->getLineno();
      vector<Token *> tokens = lexicalParser->getTokens();
      SyntaxParser *syntaxparser = new SyntaxParser(tokens);
      vector<Symbol *> symbols = syntaxparser->getSymbolTable();
      AST *root = syntaxparser->getAST();
      ASTOptimizer *astOptimizer = new ASTOptimizer(root, symbols);
      root = astOptimizer->getAST();
      IRParser *irParser = new IRParser(root, symbols);
      vector<Symbol *> consts = irParser->getConsts();
      vector<Symbol *> globalVars = irParser->getGlobalVars();
      unordered_map<Symbol *, vector<Symbol *>> localVars =
          irParser->getLocalVars();
      vector<pair<Symbol *, vector<IR *>>> funcs = irParser->getFuncs();
      unsigned tempId = irParser->getTempId();
      IROptimizer *irOptimizer = new IROptimizer(symbols, consts, globalVars,
                                                 localVars, funcs, tempId);
      funcs = irOptimizer->getFuncs();
      ASMParser *asmParser =
          new ASMParser(lineno, funcs, consts, globalVars, localVars);
      vector<pair<Symbol *, vector<ASM *>>> funcASMs = asmParser->getFuncASMs();
      ASMOptimizer *asmOptimizer = new ASMOptimizer(funcASMs);
      funcASMs = asmOptimizer->getFuncASMs();
      ASMWriter *asmWriter =
          new ASMWriter("test.s", consts, globalVars, funcASMs);
      asmWriter->write();
      for (Symbol *symbol : symbols)
        cout << symbol->toString() << endl;
      cout << "----------------------------------------------------------------"
              "----------------"
           << endl;
      irOptimizer->printIRs();
      cout << endl;
      delete asmWriter;
      delete asmOptimizer;
      delete asmParser;
      delete irOptimizer;
      delete irParser;
      delete syntaxparser;
      delete lexicalParser;
    }
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