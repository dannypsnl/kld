#pragma once

#include "elf_file.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace std;

/// Block
struct Block {
  char *data;
  unsigned int offset;
  unsigned int size;
  Block(char *d, unsigned int off, unsigned int s);
  ~Block();
};
/// SegList
struct SegList {
  unsigned int base_addr;
  unsigned int offset;
  unsigned int size;
  unsigned int begin;
  vector<Elf_file> owner_list;
  vector<Block> blocks;

  void alloc_addr(string name, unsigned int &base, unsigned int &off);
  void reloc_addr(unsigned int relAddr, unsigned char type,
                  unsigned int symAddr);
};

/// SymLink
struct SymLink {
  string name;
  optional<Elf_file> recv;
  optional<Elf_file> prov;
};

#define START "_start"
#define BASE_ADDR 0x08040000
#define MEM_ALIGN 4096
#define DISC_ALIGN 4

/// Linker maintains major functionality for linking
class Linker {
  vector<string> seg_names;
  Elf_file exe;

public:
  vector<Elf_file> elf_files;
  map<string, SegList *> seg_lists;
  vector<SymLink> symbol_links;
  vector<SymLink> symbol_def;

public:
  Linker();
  /// add new elf file
  void add_elf(string dir);
  /// collect information from owned elf files
  void collect_info();
  /// check symbol is valid
  bool symbol_is_valid();
  /// allocate address
  void alloc_addr();
  void symbol_parser();
  /// relocate address
  void relocate();
  /// assemble executable
  void assemble_executable();
  /// export elf to a file
  void export_elf(const char *dir);
  bool link(const char *dir);
  ~Linker();
};
