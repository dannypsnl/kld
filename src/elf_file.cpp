#include "elf_file.h"
#include <stdio.h>
#include <string.h>

RelItem::RelItem(string sname, Elf32_Rel *r, string rname) {
  seg_name = sname;
  rel = r;
  rel_name = rname;
}

RelItem::~RelItem() { delete rel; }

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
  fread(&ehdr, sizeof(Elf32_Ehdr), 1, fp);

  if (ehdr.e_type == ET_EXEC) {
    fseek(fp, ehdr.e_phoff, 0);
    for (auto i = 0; i < ehdr.e_phnum; ++i) {
      Elf32_Phdr *phdr = new Elf32_Phdr();
      fread(phdr, ehdr.e_phentsize, 1, fp);
      phdr_tab.push_back(phdr);
    }
  }

  fseek(fp, ehdr.e_shoff + ehdr.e_shentsize * ehdr.e_shstrndx, 0);
  Elf32_Shdr shstrTab;
  fread(&shstrTab, ehdr.e_shentsize, 1, fp);
  char *shstrTabData = new char[shstrTab.sh_size];
  fseek(fp, shstrTab.sh_offset, 0);
  fread(shstrTabData, shstrTab.sh_size, 1, fp);

  fseek(fp, ehdr.e_shoff, 0);
  for (auto i = 0; i < ehdr.e_shnum; ++i) {
    Elf32_Shdr *shdr = new Elf32_Shdr();
    fread(shdr, ehdr.e_shentsize, 1, fp);
    string name(shstrTabData + shdr->sh_name);
    shdr_names.push_back(name);
    if (name.empty())
      delete shdr;
    else {
      shdr_tab[name] = shdr;
    }
  }
  delete[] shstrTabData;

  Elf32_Shdr *strTab = shdr_tab[".strtab"];
  char *strTabData = new char[strTab->sh_size];
  fseek(fp, strTab->sh_offset, 0);
  fread(strTabData, strTab->sh_size, 1, fp);

  Elf32_Shdr *sh_symTab = shdr_tab[".symtab"];
  fseek(fp, sh_symTab->sh_offset, 0);
  int symNum = sh_symTab->sh_size / 16;
  vector<Elf32_Sym *> symList;
  for (auto i = 0; i < symNum; ++i) {
    Elf32_Sym *sym = new Elf32_Sym();
    fread(sym, 16, 1, fp);
    symList.push_back(sym);
    string name(strTabData + sym->st_name);
    if (name.empty())
      delete sym;
    else {
      sym_tab[name] = sym;
    }
  }
  for (auto i = shdr_tab.begin(); i != shdr_tab.end(); ++i) {
    if (i->first.find(".rel") == 0) {
      Elf32_Shdr *sh_relTab = shdr_tab[i->first];
      fseek(fp, sh_relTab->sh_offset, 0);
      int relNum = sh_relTab->sh_size / 8;
      for (int j = 0; j < relNum; ++j) {
        Elf32_Rel *rel = new Elf32_Rel();
        fread(rel, 8, 1, fp);
        string name(strTabData + symList[ELF32_R_SYM(rel->r_info)]->st_name);
        rel_tab.push_back(new RelItem(i->first.substr(4), rel, name));
      }
    }
  }
  delete[] strTabData;

  fclose(fp);
}

int Elf_file::get_seg_index(string segName) {
  int index = 0;
  for (auto i = 0; i < shdr_names.size(); ++i) {
    if (shdr_names[i] == segName)
      break;
    ++index;
  }
  return index;
}

int Elf_file::get_symbol_index(string symName) {
  int index = 0;
  for (auto i = 0; i < sym_names.size(); ++i) {
    if (shdr_names[i] == symName)
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
  phdr_tab.push_back(ph);
}

void Elf_file::add_section_header(string sh_name, Elf32_Word sh_type,
                                  Elf32_Word sh_flags, Elf32_Addr sh_addr,
                                  Elf32_Off sh_offset, Elf32_Word sh_size,
                                  Elf32_Word sh_link, Elf32_Word sh_info,
                                  Elf32_Word sh_addralign,
                                  Elf32_Word sh_entsize) //添加一个段表项
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
  shdr_tab[sh_name] = sh;
  shdr_names.push_back(sh_name);
}

void Elf_file::add_symbol(string st_name, Elf32_Sym *s) {
  Elf32_Sym *sym = sym_tab[st_name] = new Elf32_Sym();
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
    fwrite(&ehdr, ehdr.e_ehsize, 1, fp);
    if (!phdr_tab.empty()) {
      for (auto i = 0; i < phdr_tab.size(); ++i)
        fwrite(phdr_tab[i], ehdr.e_phentsize, 1, fp);
    }
    fclose(fp);
  } else if (flag == 2) {
    FILE *fp = fopen(dir, "a+");
    fwrite(shstrtab, shstrtab_size, 1, fp);
    for (auto i = 0; i < shdr_names.size(); ++i) {
      Elf32_Shdr *sh = shdr_tab[shdr_names[i]];
      fwrite(sh, ehdr.e_shentsize, 1, fp);
    }
    for (auto i = 0; i < sym_names.size(); ++i) {
      Elf32_Sym *sym = sym_tab[sym_names[i]];
      fwrite(sym, sizeof(Elf32_Sym), 1, fp);
    }
    fwrite(strtab, strtab_size, 1, fp);
    fclose(fp);
  }
}

Elf_file::~Elf_file() {
  for (auto i = phdr_tab.begin(); i != phdr_tab.end(); ++i) {
    delete *i;
  }
  phdr_tab.clear();
  for (auto i = shdr_tab.begin(); i != shdr_tab.end(); ++i) {
    delete i->second;
  }
  shdr_tab.clear();
  shdr_names.clear();
  for (auto i = sym_tab.begin(); i != sym_tab.end(); ++i) {
    delete i->second;
  }
  sym_tab.clear();
  for (auto i = rel_tab.begin(); i != rel_tab.end(); ++i) {
    delete *i;
  }
  rel_tab.clear();
  if (shstrtab != NULL)
    delete[] shstrtab;
  if (strtab != NULL)
    delete[] strtab;
  if (elf_dir)
    delete elf_dir;
}
