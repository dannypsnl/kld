#include "elf_file.h"
#include <stdio.h>
#include <string.h>

RelocationItem::RelocationItem(string sname, Elf32_Rel *r, string rname) {
  seg_name = sname;
  relocation = r;
  rel_name = rname;
}

RelocationItem::~RelocationItem() { delete relocation; }

void Elf_file::get_data(char *buf, Elf32_Off offset, Elf32_Word size) {
  FILE *fp = fopen(elf_dir, "rb");
  rewind(fp);
  fseek(fp, offset, 0);
  fread(buf, size, 1, fp);
  fclose(fp);
}

Elf_file::Elf_file() {
  shstrtab = NULL;
  strtab = NULL;
  elf_dir = NULL;
}

void Elf_file::read_elf(const char *dir) {
  string d = dir;
  elf_dir = new char[d.length() + 1];
  strcpy(elf_dir, dir);
  FILE *fp = fopen(dir, "rb");
  rewind(fp);
  fread(&elf_file_header, sizeof(Elf32_Ehdr), 1, fp);

  if (elf_file_header.e_type == ET_EXEC) {
    fseek(fp, elf_file_header.e_phoff, 0);
    for (auto i = 0; i < elf_file_header.e_phnum; ++i) {
      Elf32_Phdr *phdr = new Elf32_Phdr();
      fread(phdr, elf_file_header.e_phentsize, 1, fp);
      program_header_table.push_back(phdr);
    }
  }

  fseek(fp,
        elf_file_header.e_shoff +
            elf_file_header.e_shentsize * elf_file_header.e_shstrndx,
        0);
  Elf32_Shdr shstr_tab;
  fread(&shstr_tab, elf_file_header.e_shentsize, 1, fp);
  char *shstr_tab_data = new char[shstr_tab.sh_size];
  fseek(fp, shstr_tab.sh_offset, 0);
  fread(shstr_tab_data, shstr_tab.sh_size, 1, fp);

  fseek(fp, elf_file_header.e_shoff, 0);
  for (auto i = 0; i < elf_file_header.e_shnum; ++i) {
    Elf32_Shdr *shdr = new Elf32_Shdr();
    fread(shdr, elf_file_header.e_shentsize, 1, fp);
    string name(shstr_tab_data + shdr->sh_name);
    shdr_names.push_back(name);
    if (name.empty())
      delete shdr;
    else {
      section_header_table[name] = shdr;
    }
  }
  delete[] shstr_tab_data;

  Elf32_Shdr *str_tab = section_header_table[".strtab"];
  char *str_tab_data = new char[str_tab->sh_size];
  fseek(fp, str_tab->sh_offset, 0);
  fread(str_tab_data, str_tab->sh_size, 1, fp);

  Elf32_Shdr *sh_symbol_tab = section_header_table[".symtab"];
  fseek(fp, sh_symbol_tab->sh_offset, 0);
  int symNum = sh_symbol_tab->sh_size / 16;
  vector<Elf32_Sym *> sym_list;
  for (auto i = 0; i < symNum; ++i) {
    Elf32_Sym *sym = new Elf32_Sym();
    fread(sym, 16, 1, fp);
    sym_list.push_back(sym);
    string name(str_tab_data + sym->st_name);
    if (name.empty())
      delete sym;
    else {
      symbol_table[name] = sym;
    }
  }
  for (auto i = section_header_table.begin(); i != section_header_table.end();
       ++i) {
    if (i->first.find(".rel") == 0) {
      Elf32_Shdr *sh_rel_tab = section_header_table[i->first];
      fseek(fp, sh_rel_tab->sh_offset, 0);
      int relNum = sh_rel_tab->sh_size / 8;
      for (int j = 0; j < relNum; ++j) {
        Elf32_Rel *rel = new Elf32_Rel();
        fread(rel, 8, 1, fp);
        string name(str_tab_data + sym_list[ELF32_R_SYM(rel->r_info)]->st_name);
        relocation_table.push_back(
            new RelocationItem(i->first.substr(4), rel, name));
      }
    }
  }
  delete[] str_tab_data;

  fclose(fp);
}

int Elf_file::get_seg_index(string seg_name) {
  int index = 0;
  for (auto i = 0; i < shdr_names.size(); ++i) {
    if (shdr_names[i] == seg_name)
      break;
    ++index;
  }
  return index;
}

int Elf_file::get_symbol_index(string symbol_name) {
  int index = 0;
  for (auto i = 0; i < sym_names.size(); ++i) {
    if (shdr_names[i] == symbol_name)
      break;
    ++index;
  }
  return index;
}

void Elf_file::add_program_header(Elf32_Word type, Elf32_Off off,
                                  Elf32_Addr vaddr, Elf32_Word filesz,
                                  Elf32_Word memsz, Elf32_Word flags,
                                  Elf32_Word align) {
  Elf32_Phdr *ph = new Elf32_Phdr();
  ph->p_type = type;
  ph->p_offset = off;
  ph->p_vaddr = ph->p_paddr = vaddr;
  ph->p_filesz = filesz;
  ph->p_memsz = memsz;
  ph->p_flags = flags;
  ph->p_align = align;
  program_header_table.push_back(ph);
}

void Elf_file::add_section_header(string sh_name, Elf32_Word sh_type,
                                  Elf32_Word sh_flags, Elf32_Addr sh_addr,
                                  Elf32_Off sh_offset, Elf32_Word sh_size,
                                  Elf32_Word sh_link, Elf32_Word sh_info,
                                  Elf32_Word sh_addralign,
                                  Elf32_Word sh_entsize) //?????????????????????
{
  Elf32_Shdr *sh = new Elf32_Shdr();
  sh->sh_name = 0;
  sh->sh_type = sh_type;
  sh->sh_flags = sh_flags;
  sh->sh_addr = sh_addr;
  sh->sh_offset = sh_offset;
  sh->sh_size = sh_size;
  sh->sh_link = sh_link;
  sh->sh_info = sh_info;
  sh->sh_addralign = sh_addralign;
  sh->sh_entsize = sh_entsize;
  section_header_table[sh_name] = sh;
  shdr_names.push_back(sh_name);
}

void Elf_file::add_symbol(string st_name, Elf32_Sym *s) {
  Elf32_Sym *sym = symbol_table[st_name] = new Elf32_Sym();
  if (st_name == "") {
    sym->st_name = 0;
    sym->st_value = 0;
    sym->st_size = 0;
    sym->st_info = 0;
    sym->st_other = 0;
    sym->st_shndx = 0;
  } else {
    sym->st_name = 0;
    sym->st_value = s->st_value;
    sym->st_size = s->st_size;
    sym->st_info = s->st_info;
    sym->st_other = s->st_other;
    sym->st_shndx = s->st_shndx;
  }
  sym_names.push_back(st_name);
}

void Elf_file::write_elf(const char *dir, int flag) {
  if (flag == 1) {
    FILE *fp = fopen(dir, "w+");
    fwrite(&elf_file_header, elf_file_header.e_ehsize, 1, fp);
    if (!program_header_table.empty()) {
      for (auto i = 0; i < program_header_table.size(); ++i)
        fwrite(program_header_table[i], elf_file_header.e_phentsize, 1, fp);
    }
    fclose(fp);
  } else if (flag == 2) {
    FILE *fp = fopen(dir, "a+");
    fwrite(shstrtab, shstrtab_size, 1, fp);
    for (auto i = 0; i < shdr_names.size(); ++i) {
      Elf32_Shdr *sh = section_header_table[shdr_names[i]];
      fwrite(sh, elf_file_header.e_shentsize, 1, fp);
    }
    for (auto i = 0; i < sym_names.size(); ++i) {
      Elf32_Sym *sym = symbol_table[sym_names[i]];
      fwrite(sym, sizeof(Elf32_Sym), 1, fp);
    }
    fwrite(strtab, strtab_size, 1, fp);
    fclose(fp);
  }
}

Elf_file::~Elf_file() {
  for (auto i = program_header_table.begin(); i != program_header_table.end();
       ++i) {
    delete *i;
  }
  program_header_table.clear();
  for (auto i = section_header_table.begin(); i != section_header_table.end();
       ++i) {
    delete i->second;
  }
  section_header_table.clear();
  shdr_names.clear();
  for (auto i = symbol_table.begin(); i != symbol_table.end(); ++i) {
    delete i->second;
  }
  symbol_table.clear();
  for (auto i = relocation_table.begin(); i != relocation_table.end(); ++i) {
    delete *i;
  }
  relocation_table.clear();
  if (shstrtab != NULL)
    delete[] shstrtab;
  if (strtab != NULL)
    delete[] strtab;
  if (elf_dir)
    delete elf_dir;
}
