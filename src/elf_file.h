#pragma once

#include "elf.h"
#include <map>
#include <string>
#include <vector>

using namespace std;

struct RelItem {
  string segName;
  Elf32_Rel *rel;
  string relName;
  RelItem(string sname, Elf32_Rel *r, string rname);
  ~RelItem();
};

class Elf_file {
public:
  Elf32_Ehdr ehdr;
  vector<Elf32_Phdr *> phdr_tab;
  map<string, Elf32_Shdr *> shdr_tab;
  vector<string> shdr_names;
  map<string, Elf32_Sym *> sym_tab;
  vector<string> sym_names;
  vector<RelItem *> rel_tab;
  char *elf_dir;
  char *shstrtab;
  unsigned int shstrtabSize;
  char *strtab;
  unsigned int strtabSize;

public:
  Elf_file();
  void read_elf(const char *dir);
  void get_data(char *buf, Elf32_Off offset, Elf32_Word size);
  int getSegIndex(string segName);
  int getSymIndex(string symName);
  void add_program_header(Elf32_Word type, Elf32_Off off, Elf32_Addr vaddr,
                          Elf32_Word filesz, Elf32_Word memsz, Elf32_Word flags,
                          Elf32_Word align);
  void add_section_header(string sh_name, Elf32_Word sh_type,
                          Elf32_Word sh_flags, Elf32_Addr sh_addr,
                          Elf32_Off sh_offset, Elf32_Word sh_size,
                          Elf32_Word sh_link, Elf32_Word sh_info,
                          Elf32_Word sh_addralign, Elf32_Word sh_entsize);
  void add_symbol(string st_name, Elf32_Sym *);
  void write_elf(const char *dir, int flag);
  ~Elf_file();
};
