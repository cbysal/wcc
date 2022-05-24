#include <algorithm>
#include <filesystem>
#include <iostream>

#include "../src/frontend/LexicalParser.h"

using namespace std;

int item = -2;

int main(int argc, char *argv[]) {
  if (argc == 2)
    item = stoi(argv[1]);
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
    LexicalParser *parser = new LexicalParser(file);
    vector<Token *> tokens = parser->getTokens();
    if (item > -2)
      for (Token *token : tokens)
        cout << token->toString() << endl;
    delete parser;
    cout << endl;
  }
  return 0;
}