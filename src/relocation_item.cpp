#include "relocation_item.h"

RelocationItem::RelocationItem(string seg_name, Elf32_Rel *relocation, string rel_name) {
  this->seg_name = seg_name;
  this->relocation = relocation;
  this->rel_name = rel_name;
}

RelocationItem::~RelocationItem() { delete this->relocation; }
