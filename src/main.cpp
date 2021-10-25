#include "linker.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
  Linker linker;
  string desFileName;
  int i = 1;
  while (true) {
    string arg = argv[i];
    if (arg.rfind(".o") != arg.length() - 2) {
      desFileName = arg;
      break;
    }
    linker.addElf(arg.c_str());
    i++;
  }
  linker.link(desFileName.c_str());
  return 0;
}
