#include "elf_file.h"
#include <stdio.h>
#include <string.h>

void Elf_file::get_data(char *buf, Elf32_Off offset, Elf32_Word size) {
  FILE *fp = fopen(elf_dir.c_str(), "rb");
  rewind(fp);
  fseek(fp, offset, 0);
  fread(buf, size, 1, fp);
  fclose(fp);
}

Elf_file::Elf_file() : elf_dir{}, shstrtab{}, strtab{} {}

void Elf_file::read_elf(string dir) {
  elf_dir = dir;
  FILE *fp = fopen(elf_dir.c_str(), "rb");
  rewind(fp);
  fread(&elf_file_header, sizeof(Elf32_Ehdr), 1, fp);

  if (elf_file_header.e_type == ET_EXEC) {
    fseek(fp, elf_file_header.e_phoff, 0);
    program_header_table.resize(elf_file_header.e_phnum);
    for (auto program_header : program_header_table) {
      fread(&program_header, elf_file_header.e_phentsize, 1, fp);
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
    Elf32_Shdr sec_header;
    fread(&sec_header, elf_file_header.e_shentsize, 1, fp);
    string name(shstr_tab_data + sec_header.sh_name);
    shdr_names.push_back(name);
    if (!name.empty()) {
      section_header_table[name] = sec_header;
    }
  }
  delete[] shstr_tab_data;

  Elf32_Shdr str_tab = section_header_table[".strtab"];
  char *str_tab_data = new char[str_tab.sh_size];
  fseek(fp, str_tab.sh_offset, 0);
  fread(str_tab_data, str_tab.sh_size, 1, fp);

  Elf32_Shdr sh_symbol_tab = section_header_table[".symtab"];
  fseek(fp, sh_symbol_tab.sh_offset, 0);
  int symNum = sh_symbol_tab.sh_size / 16;
  vector<Elf32_Sym> sym_list;
  for (auto i = 0; i < symNum; ++i) {
    Elf32_Sym sym;
    fread(&sym, 16, 1, fp);
    sym_list.push_back(sym);
    string name(str_tab_data + sym.st_name);
    if (!name.empty()) {
      symbol_table[name] = sym;
    }
  }
  for (auto const &[key, val] : section_header_table) {
    if (key.find(".rel") == 0) {
      Elf32_Shdr sh_rel_tab = section_header_table[key];
      fseek(fp, sh_rel_tab.sh_offset, 0);
      int relNum = sh_rel_tab.sh_size / 8;
      for (int j = 0; j < relNum; ++j) {
        Elf32_Rel rel;
        fread(&rel, 8, 1, fp);
        string name{str_tab_data + sym_list[ELF32_R_SYM(rel.r_info)].st_name};
        relocation_table.push_back(RelocationItem(key.substr(4), rel, name));
      }
    }
  }
  delete[] str_tab_data;

  fclose(fp);
}

int Elf_file::get_seg_index(string seg_name) {
  int index = 0;
  for (auto name : shdr_names) {
    if (name == seg_name)
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
  Elf32_Phdr ph{
      .p_type = type,
      .p_offset = off,
      .p_vaddr = vaddr,
      .p_paddr = vaddr,
      .p_filesz = filesz,
      .p_memsz = memsz,
      .p_flags = flags,
      .p_align = align,
  };
  program_header_table.push_back(ph);
}

void Elf_file::add_section_header(string sh_name, Elf32_Word sh_type,
                                  Elf32_Word sh_flags, Elf32_Addr sh_addr,
                                  Elf32_Off sh_offset, Elf32_Word sh_size,
                                  Elf32_Word sh_link, Elf32_Word sh_info,
                                  Elf32_Word sh_addralign,
                                  Elf32_Word sh_entsize) {
  Elf32_Shdr sh{
      .sh_name = 0,
      .sh_type = sh_type,
      .sh_flags = sh_flags,
      .sh_addr = sh_addr,
      .sh_offset = sh_offset,
      .sh_size = sh_size,
      .sh_link = sh_link,
      .sh_info = sh_info,
      .sh_addralign = sh_addralign,
      .sh_entsize = sh_entsize,
  };
  section_header_table[sh_name] = sh;
  shdr_names.push_back(sh_name);
}

void Elf_file::add_empty_symbol() {
  Elf32_Sym sym{
      .st_name = 0,
      .st_value = 0,
      .st_size = 0,
      .st_info = 0,
      .st_other = 0,
      .st_shndx = 0,
  };
  symbol_table[""] = sym;
  sym_names.push_back("");
}
void Elf_file::add_symbol(string st_name, Elf32_Sym &s) {
  Elf32_Sym sym{
      .st_name = 0,
      .st_value = s.st_value,
      .st_size = s.st_size,
      .st_info = s.st_info,
      .st_other = s.st_other,
      .st_shndx = s.st_shndx,
  };
  symbol_table[st_name] = sym;
  sym_names.push_back(st_name);
}

void Elf_file::write_elf(const char *dir, int flag) {
  switch (flag) {
  case 1: {
    FILE *fp = fopen(dir, "w+");
    fwrite(&elf_file_header, elf_file_header.e_ehsize, 1, fp);
    for (auto program_header : program_header_table) {
      fwrite(&program_header, elf_file_header.e_phentsize, 1, fp);
    }
    fclose(fp);
    break;
  }
  case 2: {
    FILE *fp = fopen(dir, "a+");
    fwrite(shstrtab.data(), shstrtab_size, 1, fp);
    for (string &sec_header_name : shdr_names) {
      Elf32_Shdr &sh = section_header_table[sec_header_name];
      fwrite(&sh, elf_file_header.e_shentsize, 1, fp);
    }
    for (string &name : sym_names) {
      Elf32_Sym sym = symbol_table[name];
      fwrite(&sym, sizeof(Elf32_Sym), 1, fp);
    }
    fwrite(strtab.data(), strtab_size, 1, fp);
    fclose(fp);
    break;
  }
  default:
    break;
  }
}
