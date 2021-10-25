#pragma once

#include "elf_file.h"
#include <map>
#include <vector>

using namespace std;

struct Block {
  char *data;
  unsigned int offset;
  unsigned int size;
  Block(char *d, unsigned int off, unsigned int s);
  ~Block();
};
struct SegList {
  unsigned int baseAddr;
  unsigned int offset;
  unsigned int size;
  unsigned int begin;
  vector<Elf_file *> ownerList;
  vector<Block *> blocks;

  void allocAddr(string name, unsigned int &base, unsigned int &off);
  void relocAddr(unsigned int relAddr, unsigned char type,
                 unsigned int symAddr);
  ~SegList();
};

struct SymLink {
  string name;
  Elf_file *recv;
  Elf_file *prov;
};

#define START "_start"
#define BASE_ADDR 0x08040000
#define MEM_ALIGN 4096
#define DISC_ALIGN 4

class Linker {
  vector<string> segNames;
  Elf_file exe;
  Elf_file *startOwner;

public:
  vector<Elf_file *> elfs;
  map<string, SegList *> segLists;
  vector<SymLink *> symLinks;
  vector<SymLink *> symDef;

public:
  Linker();
  void add_elf(const char *dir);
  void collect_info();
  bool symbol_is_valid();
  void alloc_addr();
  void symbol_parser();
  void relocate();
  void assemble_executable();
  void export_elf(const char *dir);
  bool link(const char *dir);
  ~Linker();
};
