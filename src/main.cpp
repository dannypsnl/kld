#include "elfio/elfio.hpp"
#include <iostream>
#include <map>

using namespace std;
using namespace ELFIO;

#define BASE_ADDR 0x08048000
#define MEM_ALIGN 4096
#define DISC_ALIGN 4
#define TEXT_ALIGN 16

struct Block {
  char *data;
  unsigned int offset;
  unsigned int size;

  Block(char *data, unsigned int off, unsigned int size);
};

Block::Block(char *data, unsigned int off, unsigned int size)
    : data{data}, offset{off}, size{size} {}

struct SegList {
  unsigned int baseAddr;
  unsigned int begin;
  unsigned int offset;
  unsigned int size;
  vector<elfio *> ownerList;
  vector<Block *> blocks;

  void allocAddr(string name, unsigned int &base, unsigned int &off);
  void relocAddr(unsigned int relAddr, unsigned char type,
                 unsigned int symAddr);
};

void SegList::allocAddr(string name, unsigned int &base, unsigned int &off) {
  begin = off;

  int align = DISC_ALIGN;
  if (name == ".text") {
    align = TEXT_ALIGN;
  }
  off += (align - off % align) % align;
  base += (MEM_ALIGN - base % MEM_ALIGN) % MEM_ALIGN + off % MEM_ALIGN;

  baseAddr = base;
  offset = off;
  size = 0;

  for (int i = 0; i < ownerList.size(); ++i) {
    auto seg = ownerList[i]->sections[name];
    int sh_align = seg->get_addr_align();
    size += (sh_align - size % sh_align) % sh_align;

    auto data = seg->get_data();
    blocks.push_back(new Block((char *)data, size, seg->get_size()));

    seg->set_address(base + size);
    size += seg->get_size();
  }

  base += size;
  off += size;
}

struct SymLink {
  string name;
  elfio *recv;
  elfio *prov;
};

struct Linker {
  vector<string> segNames;
  map<string, SegList *> segLists;
  vector<elfio *> elfs;
  vector<SymLink *> symLinks;
  vector<SymLink *> symDef;
  elfio *startOwner;

  void allocAddr();
  void collectInfo();
  bool symValid();
};

void Linker::allocAddr() {
  unsigned int curAddr = BASE_ADDR;
  unsigned int curOff = 52 + sizeof(segment) * segNames.size();
  for (int i = 0; i < segNames.size(); ++i) {
    segLists[segNames[i]]->allocAddr(segNames[i], curAddr, curOff);
  }
}

void Linker::collectInfo() {
  for (int i = 0; i < elfs.size(); ++i) {
    elfio *elf = elfs[i];
    for (int i = 0; i < segNames.size(); ++i) {
      if (elf->sections[segNames[i]] != *elf->sections.end()) {
        segLists[segNames[i]]->ownerList.push_back(elf);
      }
    }
    section *sec;
    symbol_section_accessor access{(const elfio &)elf, sec};
    for (int i = 0; i < access.get_symbols_num(); ++i) {
      string name;
      Elf64_Addr value;
      Elf_Xword size;
      unsigned char bind;
      unsigned char type;
      Elf_Half section_index;
      unsigned char other;
      access.get_symbol(i, name, value, size, bind, type, section_index, other);
      if (type == STB_GLOBAL) {
        SymLink *symLink = new SymLink();
        symLink->name = name;
        if (section_index == STN_UNDEF) {
          symLink->recv = elf;
          symLink->prov = nullptr;
          symLinks.push_back(symLink);
        } else {
          symLink->recv = nullptr;
          symLink->prov = elf;
          symDef.push_back(symLink);
        }
      }
    }
  }
}

#define START "@start"

bool Linker::symValid() {
  bool flag = true;
  startOwner = nullptr;
  for (int i = 0; i < symDef.size(); ++i) {
    if (symDef[i]->name == START) {
      startOwner = symDef[i]->prov;
    }
    for (int j = i + 1; j < symDef.size(); ++j) {
      if (symDef[i]->name == symDef[j]->name) {
        printf("symbol %s redefinition.\n", symDef[i]->name.c_str());
      }
    }
  }
  if (startOwner == nullptr) {
    printf("can not find entrypoint symbol %s.\n", START);
    flag = false;
  }
  for (int i = 0; i < symLinks.size(); ++i) {
    for (int j = 0; j < symDef.size(); ++j) {
      if (symLinks[i]->name == symDef[j]->name) {
        symLinks[i]->prov = symDef[j]->prov;
        break;
      }
    }
    if (symLinks[i]->prov == nullptr) {
      printf("undefined symbol %s.\n", symDef[i]->name.c_str());
      flag = false;
    }
  }
  return flag;
}

int main(int argc, char **argv) {
  elfio e{};
  cout << "hello world!" << endl;
  return 0;
}
