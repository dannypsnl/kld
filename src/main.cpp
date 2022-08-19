#include "linker.h"
#include <boost/algorithm/string/predicate.hpp>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc == 1) {
    std::cout << "please provide arguments." << std::endl;
    return 0;
  }
  Linker linker;
  int i = 1;
  while (true) {
    string arg = argv[i];
    if (!boost::algorithm::ends_with(arg, ".o")) {
      linker.link(arg.c_str());
      break;
    }
    linker.add_elf(arg);
    i++;
  }
  return 0;
}
