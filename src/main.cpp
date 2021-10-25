#include "linker.h"
#include <boost/algorithm/string/predicate.hpp>
#include <stdio.h>

int main(int argc, char *argv[]) {
  if (argc == 1) {
    printf("please provide arguments.\n");
    return 0;
  }
  Linker linker;
  string desFileName;
  int i = 1;
  while (true) {
    string arg = argv[i];
    if (!boost::algorithm::ends_with(arg, ".o")) {
      desFileName = arg;
      break;
    }
    linker.add_elf(arg.c_str());
    i++;
  }
  linker.link(desFileName.c_str());
  return 0;
}
