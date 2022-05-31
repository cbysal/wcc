#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>

#include "frontend/IRParser.h"
#include "frontend/LexicalParser.h"
#include "frontend/SyntaxParser.h"

using namespace std;

char mode = 'A';

bool debug = false;
bool printSource = false;
bool printFormat = false;
bool printSymbols = false;
bool printIR = false;

int item = -1;

int main(int argc, char *argv[]) {
  if (argc != 3)
    return -1;
  int len = strlen(argv[1]);
  for (int i = 0; i < len; i++) {
    switch (argv[1][i]) {
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
      printSymbols = true;
    default:
      break;
    }
  }
  item = stoi(argv[2]);
  int id = 0;
  string dir = "test_case/functional";
  vector<string> files;
  for (const filesystem::directory_entry &entry :
       filesystem::directory_iterator(dir))
    files.push_back(entry.path());
  sort(files.begin(), files.end());
  for (const string &file : files) {
    if (file.length() < 3 || file.find(".sy") != file.length() - 3)
      continue;
    if (item >= 0 && id++ != item)
      continue;
    cout << file << endl;
    LexicalParser *lexicalParser = new LexicalParser(file);
    vector<Token *> tokens = lexicalParser->getTokens();
    SyntaxParser *syntaxparser = new SyntaxParser(tokens);
    vector<Symbol *> symbols = syntaxparser->getSymbolTable();
    AST *root = syntaxparser->getAST();
    IRParser *irParser = new IRParser(root, symbols);
    if (printSource) {
      cout << "----------------------------------------------------------------"
              "----------------"
           << endl;
      cout << lexicalParser->getContent() << endl;
    }
    if (printFormat) {
      cout << "----------------------------------------------------------------"
              "----------------"
           << endl;
      root->print(0);
    }
    if (printSymbols) {
      cout << "----------------------------------------------------------------"
              "----------------"
           << endl;
      for (Symbol *symbol : symbols)
        cout << symbol->toString() << endl;
    }
    if (printIR) {
      cout << "----------------------------------------------------------------"
              "----------------"
           << endl;
      irParser->printIRs();
    }
    delete irParser;
    delete syntaxparser;
    delete lexicalParser;
    cout << endl;
  }
  return 0;
}