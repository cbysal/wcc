#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <unistd.h>

#include "backend/ASMParser.h"
#include "frontend/IRParser.h"
#include "frontend/LexicalParser.h"
#include "frontend/SyntaxParser.h"

using namespace std;

char mode = 'A';

bool debug = false;
bool printSource = false;
bool printTokens = false;
bool printFormat = false;
bool printSymbols = false;
bool printIR = false;
bool compileOrNot = false;

int item = -1;

int main(int argc, char *argv[]) {
  clock_t beginTime = clock();
  if (argc < 3)
    return -1;
  if (argc > 3) {
    string source = argv[4];
    string target = argv[3];
    LexicalParser *lexicalParser = new LexicalParser(source);
    pair<unsigned, unsigned> lineno = lexicalParser->getLineno();
    vector<Token *> tokens = lexicalParser->getTokens();
    SyntaxParser *syntaxparser = new SyntaxParser(tokens);
    vector<Symbol *> symbols = syntaxparser->getSymbolTable();
    AST *root = syntaxparser->getAST();
    IRParser *irParser = new IRParser(root, symbols);
    vector<Symbol *> consts = irParser->getConsts();
    vector<Symbol *> globalVars = irParser->getGlobalVars();
    unordered_map<Symbol *, vector<Symbol *>> localVars =
        irParser->getLocalVars();
    vector<pair<Symbol *, vector<IR *>>> funcs = irParser->getFuncs();
    ASMParser *asmParser =
        new ASMParser(target, lineno, funcs, consts, globalVars, localVars);
    asmParser->writeASMFile();
    delete asmParser;
    delete irParser;
    delete syntaxparser;
    delete lexicalParser;
  } else {
    int len = strlen(argv[1]);
    for (int i = 0; i < len; i++) {
      switch (argv[1][i]) {
      case 'c':
        compileOrNot = true;
        break;
      case 'd':
        debug = true;
        break;
      case 'f':
        printFormat = true;
        break;
      case 'i':
        printIR = true;
        break;
      case 's':
        printSource = true;
        break;
      case 't':
        printTokens = true;
        break;
      case 'y':
        printSymbols = true;
        break;
      default:
        break;
      }
    }
    item = stoi(argv[2]);
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
      IRParser *irParser = new IRParser(root, symbols);
      vector<Symbol *> consts = irParser->getConsts();
      vector<Symbol *> globalVars = irParser->getGlobalVars();
      unordered_map<Symbol *, vector<Symbol *>> localVars =
          irParser->getLocalVars();
      vector<pair<Symbol *, vector<IR *>>> funcs = irParser->getFuncs();
      ASMParser *asmParser =
          new ASMParser("test.s", lineno, funcs, consts, globalVars, localVars);
      asmParser->writeASMFile();
      if (printSource) {
        cout << "--------------------------------------------------------------"
                "--"
                "----------------"
             << endl;
        cout << lexicalParser->getContent() << endl;
      }
      if (printTokens) {
        cout << "--------------------------------------------------------------"
                "--"
                "----------------"
             << endl;
        for (Token *token : tokens)
          cout << token->toString() << endl;
      }
      if (printFormat) {
        cout << "--------------------------------------------------------------"
                "--"
                "----------------"
             << endl;
        root->print(0);
      }
      if (printSymbols) {
        cout << "--------------------------------------------------------------"
                "--"
                "----------------"
             << endl;
        for (Symbol *symbol : symbols)
          cout << symbol->toString() << endl;
      }
      if (printIR) {
        cout << "--------------------------------------------------------------"
                "--"
                "----------------"
             << endl;
        irParser->printIRs();
      }
      delete asmParser;
      delete irParser;
      delete syntaxparser;
      delete lexicalParser;
      cout << endl;
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