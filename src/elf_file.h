#pragma once

#include "elf.h"
#include "relocation_item.h"
#include <map>
#include <string>
#include <vector>

using namespace std;

class Elf_file {
public:
  Elf32_Ehdr elf_file_header;
  vector<Elf32_Phdr> program_header_table;
  map<string, Elf32_Shdr> section_header_table;
  vector<string> shdr_names;
  map<string, Elf32_Sym> symbol_table;
  vector<string> sym_names;
  vector<RelocationItem *> relocation_table;
  string elf_dir;
  char *shstrtab;
  unsigned int shstrtab_size;
  char *strtab;
  unsigned int strtab_size;

public:
  Elf_file();
  void read_elf(string dir);
  void get_data(char *buf, Elf32_Off offset, Elf32_Word size);
  int get_seg_index(string segName);
  int get_symbol_index(string symName);
  void add_program_header(Elf32_Word type, Elf32_Off off, Elf32_Addr vaddr,
                          Elf32_Word filesz, Elf32_Word memsz, Elf32_Word flags,
                          Elf32_Word align);
  void add_section_header(string sh_name, Elf32_Word sh_type,
                          Elf32_Word sh_flags, Elf32_Addr sh_addr,
                          Elf32_Off sh_offset, Elf32_Word sh_size,
                          Elf32_Word sh_link, Elf32_Word sh_info,
                          Elf32_Word sh_addralign, Elf32_Word sh_entsize);
  void add_empty_symbol();
  void add_symbol(string st_name, Elf32_Sym &sym);
  void write_elf(const char *dir, int flag);
  ~Elf_file();
};
