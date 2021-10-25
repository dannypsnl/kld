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

  void allocAddr();
};

void Linker::allocAddr() {
  unsigned int curAddr = BASE_ADDR;
  unsigned int curOff = 52 + sizeof(segment) * segNames.size();
  for (int i = 0; i < segNames.size(); ++i) {
    segLists[segNames[i]]->allocAddr(segNames[i], curAddr, curOff);
  }
}

int main(int argc, char **argv) {
  elfio e{};
  cout << "hello world!" << endl;
  return 0;
}
