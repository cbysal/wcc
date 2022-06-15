#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

bool flag;

void my_system(const char *cmd) {
  FILE *fp;
  char buf[1024];
  fp = popen(cmd, "r");
  ofstream ofs;
  ofs.open("out", ios::out);
  flag = false;
  while (fgets(buf, sizeof(buf), fp)) {
    ofs << buf;
    flag = true;
  }
  if (flag && buf[strlen(buf) - 1] != '\n')
    ofs << endl;
  ofs << ((unsigned)pclose(fp) >> 8) << endl;
  ofs.close();
}

void test(const string &syFile) {
  string file = syFile.substr(0, syFile.size() - 3);
  cout << "testing: " << file << endl;
  if (system(string("./compiler -S -o test.s " + file + ".sy").data()))
    exit(-1);
  if (system("arm-linux-gnueabihf-gcc test.s runtime/sylib.h runtime/sylib.c "
             "-o test"))
    exit(-1);
  if (filesystem::exists(filesystem::status(file + ".in")))
    my_system(
        string("qemu-arm -L /usr/arm-linux-gnueabihf/ test < " + file + ".in")
            .data());
  else
    my_system(string("qemu-arm -L /usr/arm-linux-gnueabihf/ test").data());
  if (system(string("diff out " + file + ".out").data()))
    exit(-1);
  cout << endl;
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
    test(argv[1]);
    return 0;
  }
  string dir1 = "test_case/functional";
  string dir2 = "test_case/performance";
  vector<string> files;
  for (const filesystem::directory_entry &entry :
       filesystem::directory_iterator(dir1))
    if (!entry.path().extension().compare(".sy"))
      files.push_back(entry.path());
  for (const filesystem::directory_entry &entry :
       filesystem::directory_iterator(dir2))
    if (!entry.path().extension().compare(".sy"))
      files.push_back(entry.path());
  sort(files.begin(), files.end());
  system("make");
  for (string file : files)
    test(file);
  return 0;
}