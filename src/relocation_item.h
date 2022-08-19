#pragma once

#include "elf.h"
#include <string>

using namespace std;

struct RelocationItem {
  RelocationItem(string seg_name, Elf32_Rel &relocation, string rel_name)
      : seg_name{seg_name}, relocation{relocation}, rel_name{rel_name} {}

  string seg_name;
  Elf32_Rel relocation;
  string rel_name;
};
