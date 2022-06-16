#include <iostream>
#include <string>

using namespace std;

int main() {
  int count = 0;
  while (!cin.eof()) {
    char ch = cin.get();
    if (ch == EOF)
      break;
    cout << ch;
    count = (count + 1) % 20;
    if (!count)
      cout << endl;
  }
  return 0;
}