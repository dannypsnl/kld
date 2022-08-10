#pragma once

#include "elf.h"
#include <string>

using namespace std;

struct RelocationItem {
  string seg_name;
  Elf32_Rel *relocation;
  string rel_name;
  RelocationItem(string seg_name, Elf32_Rel *relocation, string rel_name);
  ~RelocationItem();
};
