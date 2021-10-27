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
  unsigned int base_addr;
  unsigned int offset;
  unsigned int size;
  unsigned int begin;
  vector<Elf_file *> owner_list;
  vector<Block *> blocks;

  void alloc_addr(string name, unsigned int &base, unsigned int &off);
  void reloc_addr(unsigned int relAddr, unsigned char type,
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
  vector<string> seg_names;
  Elf_file exe;
  Elf_file *start_owner;

public:
  vector<Elf_file *> elfs;
  map<string, SegList *> seg_lists;
  vector<SymLink *> symbol_links;
  vector<SymLink *> symbol_def;

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
