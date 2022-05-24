#include <algorithm>
#include <filesystem>
#include <iostream>

#include "../src/frontend/GrammarParser.h"
#include "../src/frontend/LexicalParser.h"

using namespace std;

int item = -2;
char mode = 'A';

int main(int argc, char *argv[]) {
  if (argc == 3) {
    mode = argv[1][0];
    item = stoi(argv[2]);
  }
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
    GrammarParser *grammarParser = new GrammarParser(tokens);
    vector<Symbol *> symbols = grammarParser->getSymbolTable();
    AST *root = grammarParser->getAST();
    switch (mode) {
    case 'S':
      for (Symbol *symbol : symbols)
        cout << symbol->toString() << endl;
      break;
    case 'T':
      root->print(0);
      break;
    default:
      cout << lexicalParser->getContent() << endl;
      cout << "----------------------------------------------------------------"
              "----------------"
           << endl;
      root->print(0);
      cout << "----------------------------------------------------------------"
              "----------------"
           << endl;
      for (Symbol *symbol : symbols)
        cout << symbol->toString() << endl;
      break;
    }
    delete grammarParser;
    delete lexicalParser;
    cout << endl;
  }
  return 0;
}