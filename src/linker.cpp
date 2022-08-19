#include "linker.h"
#include <iostream>
#include <stdio.h>

Block::Block(string d, unsigned int off, unsigned int s)
    : data{d}, offset{off}, size{s} {}

void SegList::alloc_addr(string name, unsigned int &base, unsigned int &off) {
  begin = off;
  if (name != ".bss") {
    base += (MEM_ALIGN - base % MEM_ALIGN) % MEM_ALIGN;
  }
  int align = DISC_ALIGN;
  if (name == ".text") {
    align = 16;
  }
  off += (align - off % align) % align;
  base = base - base % MEM_ALIGN + off % MEM_ALIGN;

  this->base_addr = base;
  this->offset = off;
  this->size = 0;
  for (auto &elf_file : owner_list) {
    size += (DISC_ALIGN - size % DISC_ALIGN) % DISC_ALIGN;
    Elf32_Shdr &seg = elf_file.section_header_table[name];
    if (name != ".bss") {
      char *buf = new char[seg.sh_size];
      elf_file.get_data(buf, seg.sh_offset, seg.sh_size);
      blocks.push_back(Block(buf, size, seg.sh_size));
    }
    seg.sh_addr = base + size;
    size += seg.sh_size;
  }
  base += size;
  if (name != ".bss")
    off += size;
}

void SegList::reloc_addr(unsigned int rel_addr, unsigned char type,
                         unsigned int sym_addr) {
  unsigned int relOffset = rel_addr - base_addr;
  for (auto i = 0; i < blocks.size(); ++i) {
    if (blocks[i].offset <= relOffset &&
        blocks[i].offset + blocks[i].size > relOffset) {
      Block &b = blocks[i];
      int *pAddr = (int *)(b.data.data() + relOffset - b.offset);
      if (type == R_386_32) {
        *pAddr = sym_addr;
      } else if (type == R_386_PC32) {
        *pAddr = sym_addr - rel_addr + *pAddr;
      }
      return;
    }
  }
}

Linker::Linker() {
  seg_names.push_back(".text");
  seg_names.push_back(".data");
  seg_names.push_back(".bss");
  for (auto i = 0; i < seg_names.size(); ++i)
    seg_lists[seg_names[i]] = SegList();
}

void Linker::add_elf(string dir) {
  Elf_file elf;
  elf.read_elf(dir);
  elf_files.push_back(elf);
}

void Linker::collect_info() {
  for (auto i = 0; i < elf_files.size(); ++i) {
    Elf_file &elf = elf_files[i];
    for (auto name : seg_names) {
      if (elf.section_header_table.count(name)) {
        seg_lists[name].owner_list.push_back(elf);
      }
    }

    for (auto const &[key, val] : elf.symbol_table) {
      SymLink symLink;
      symLink.name = key;
      if (val.st_shndx == STN_UNDEF) {
        symLink.recv = elf;
        symLink.prov = nullopt;
        symbol_links.push_back(symLink);
      } else {
        symLink.recv = nullopt;
        symLink.prov = elf;
        symbol_def.push_back(symLink);
      }
    }
  }
}

bool Linker::symbol_is_valid() {
  bool flag = true;
  optional<Elf_file> start_owner{nullopt};
  for (auto i = 0; i < symbol_def.size(); ++i) {
    if (ELF32_ST_BIND(
            symbol_def[i].prov->symbol_table[symbol_def[i].name].st_info) !=
        STB_GLOBAL) {
      continue;
    }
    if (symbol_def[i].name == START) {
      start_owner = symbol_def[i].prov;
    }
    for (auto j = i + 1; j < symbol_def.size(); ++j) {
      if (ELF32_ST_BIND(
              symbol_def[j].prov->symbol_table[symbol_def[j].name].st_info) !=
          STB_GLOBAL) {
        continue;
      }
      if (symbol_def[i].name == symbol_def[j].name) {
        cout << "symbol " << symbol_def[i].name << " in "
             << symbol_def[i].prov->elf_dir << " and "
             << symbol_def[j].prov->elf_dir << " are redefined." << endl;
        flag = false;
      }
    }
  }
  if (!start_owner) {
    printf("linker cannot find entry %s.\n", START);
    flag = false;
  }
  for (auto i = 0; i < symbol_links.size(); ++i) {
    for (auto j = 0; j < symbol_def.size(); ++j) {
      if (ELF32_ST_BIND(
              symbol_def[j].prov->symbol_table[symbol_def[j].name].st_info) !=
          STB_GLOBAL)
        continue;
      if (symbol_links[i].name == symbol_def[j].name) {
        symbol_links[i].prov = symbol_def[j].prov;
        symbol_def[j].recv = symbol_def[j].prov;
      }
    }
    if (!symbol_links[i].prov) {
      unsigned char info =
          symbol_links[i].recv->symbol_table[symbol_links[i].name].st_info;
      string type;
      switch (ELF32_ST_TYPE(info)) {
      case STT_OBJECT:
        type = "variable";
        break;
      case STT_FUNC:
        type = "function";
        break;
      default:
        type = "symbol";
        break;
      }
      cout << "in file " << symbol_links[i].recv->elf_dir << " type " << type
           << " named " << symbol_links[i].name << " is undefined." << endl;
      if (flag) {
        flag = false;
      }
    }
  }
  return flag;
}

void Linker::alloc_addr() {
  unsigned int curAddr = BASE_ADDR;
  unsigned int curOff = 52 + 32 * seg_names.size();
  for (auto name : seg_names) {
    seg_lists[name].alloc_addr(name, curAddr, curOff);
  }
}

void Linker::symbol_parser() {
  for (auto &sym_link : symbol_def) {
    Elf32_Sym &sym = sym_link.prov->symbol_table[sym_link.name];
    string seg_name = sym_link.prov->shdr_names[sym.st_shndx];
    sym.st_value =
        sym.st_value + sym_link.prov->section_header_table[seg_name].sh_addr;
  }
  for (auto sym_link : symbol_links) {
    Elf32_Sym &provsym = sym_link.prov->symbol_table[sym_link.name];
    Elf32_Sym &recvsym = sym_link.recv->symbol_table[sym_link.name];
    recvsym.st_value = provsym.st_value;
  }
}

void Linker::relocate() {
  for (Elf_file &elf_file : elf_files) {
    auto relocation_table = elf_file.relocation_table;
    for (auto j = 0; j < relocation_table.size(); ++j) {
      Elf32_Sym &sym = elf_file.symbol_table[relocation_table[j].rel_name];
      unsigned int symbol_addr = sym.st_value;
      unsigned int relocation_addr =
          elf_file.section_header_table[relocation_table[j].seg_name].sh_addr +
          relocation_table[j].relocation.r_offset;

      seg_lists[relocation_table[j].seg_name].reloc_addr(
          relocation_addr, ELF32_R_TYPE(relocation_table[j].relocation.r_info),
          symbol_addr);
    }
  }
}

void Linker::assemble_executable() {
  int *p_id = (int *)exe.elf_file_header.e_ident;
  *p_id = 0x464c457f;
  p_id++;
  *p_id = 0x010101;
  p_id++;
  *p_id = 0;
  p_id++;
  *p_id = 0;
  exe.elf_file_header.e_type = ET_EXEC;
  exe.elf_file_header.e_machine = EM_386;
  exe.elf_file_header.e_version = EV_CURRENT;
  exe.elf_file_header.e_flags = 0;
  exe.elf_file_header.e_ehsize = 52;
  unsigned int cur_off = 52 + 32 * seg_names.size();
  exe.add_section_header("", 0, 0, 0, 0, 0, 0, 0, 0, 0);
  int shstrtabSize = 26;
  for (auto i = 0; i < seg_names.size(); ++i) {
    string name = seg_names[i];
    shstrtabSize += name.length() + 1;

    Elf32_Word flags = PF_W | PF_R;
    Elf32_Word filesz = seg_lists[name].size;
    if (name == ".text") {
      flags = PF_X | PF_R;
    }
    if (name == ".bss") {
      filesz = 0;
    }
    exe.add_program_header(PT_LOAD, seg_lists[name].offset,
                           seg_lists[name].base_addr, filesz,
                           seg_lists[name].size, flags, MEM_ALIGN);
    cur_off = seg_lists[name].offset;

    Elf32_Word sh_type = SHT_PROGBITS;
    Elf32_Word sh_flags = SHF_ALLOC | SHF_WRITE;
    Elf32_Word sh_align = 4;
    if (name == ".bss") {
      sh_type = SHT_NOBITS;
    }
    if (name == ".text") {
      sh_flags = SHF_ALLOC | SHF_EXECINSTR;
      sh_align = 16;
    }
    exe.add_section_header(name, sh_type, sh_flags, seg_lists[name].base_addr,
                           seg_lists[name].offset, seg_lists[name].size,
                           SHN_UNDEF, 0, sh_align, 0);
  }
  exe.elf_file_header.e_phoff = 52;
  exe.elf_file_header.e_phentsize = 32;
  exe.elf_file_header.e_phnum = seg_names.size();
  char *str = new char[shstrtabSize];
  exe.shstrtab = str;
  exe.shstrtab_size = shstrtabSize;
  int index = 0;
  map<string, int> shstr_index{};
  shstr_index[".shstrtab"] = index;
  strcpy(str + index, ".shstrtab");
  index += 10;
  shstr_index[".symtab"] = index;
  strcpy(str + index, ".symtab");
  index += 8;
  shstr_index[".strtab"] = index;
  strcpy(str + index, ".strtab");
  index += 8;
  shstr_index[""] = index - 1;
  for (auto seg_name : seg_names) {
    shstr_index[seg_name] = index;
    strcpy(str + index, seg_name.c_str());
    index += seg_name.length() + 1;
  }
  exe.add_section_header(".shstrtab", SHT_STRTAB, 0, 0, cur_off, shstrtabSize,
                         SHN_UNDEF, 0, 1, 0);
  exe.elf_file_header.e_shstrndx = exe.get_seg_index(".shstrtab");
  cur_off += shstrtabSize;
  exe.elf_file_header.e_shoff = cur_off;
  exe.elf_file_header.e_shentsize = 40;
  exe.elf_file_header.e_shnum = 4 + seg_names.size();
  cur_off += 40 * (4 + seg_names.size());
  exe.add_section_header(".symtab", SHT_SYMTAB, 0, 0, cur_off,
                         (1 + symbol_def.size()) * 16, 0, 0, 1, 16);
  exe.section_header_table[".symtab"].sh_link =
      exe.get_seg_index(".symtab") + 1;
  int strtab_size = 0;
  exe.add_empty_symbol();
  for (auto &sym_link : symbol_def) {
    string name = sym_link.name;
    strtab_size += name.length() + 1;
    Elf32_Sym sym = sym_link.prov->symbol_table[name];
    sym.st_shndx = exe.get_seg_index(sym_link.prov->shdr_names[sym.st_shndx]);
    exe.add_symbol(name, sym);
  }
  exe.elf_file_header.e_entry = exe.symbol_table[START].st_value;
  cur_off += (1 + symbol_def.size()) * 16;
  exe.add_section_header(".strtab", SHT_STRTAB, 0, 0, cur_off, strtab_size,
                         SHN_UNDEF, 0, 1, 0);
  str = new char[strtab_size];
  exe.strtab = str;
  exe.strtab_size = strtab_size;
  index = 0;
  map<string, int> str_index;
  str_index[""] = strtab_size - 1;
  for (auto sym_link : symbol_def) {
    str_index[sym_link.name] = index;
    strcpy(str + index, sym_link.name.c_str());
    index += sym_link.name.length() + 1;
  }
  for (auto &[name, symbol] : exe.symbol_table) {
    symbol.st_name = str_index[name];
  }
  for (auto &[name, sec_header] : exe.section_header_table) {
    sec_header.sh_name = shstr_index[name];
  }
}

void Linker::export_elf(const char *dir) {
  exe.write_elf(dir, 1);
  FILE *fp = fopen(dir, "a+");
  char pad[1] = {0};
  for (auto i = 0; i < seg_names.size(); ++i) {
    SegList &sl = seg_lists[seg_names[i]];
    int padnum = sl.offset - sl.begin;
    while (padnum--)
      fwrite(pad, 1, 1, fp);
    if (seg_names[i] != ".bss") {
      optional<Block> old{nullopt};
      char instPad[1] = {(char)0x90};
      for (auto j = 0; j < sl.blocks.size(); ++j) {
        Block &b = sl.blocks[j];
        if (old) {
          padnum = b.offset - (old->offset + old->size);
          while (padnum--)
            fwrite(instPad, 1, 1, fp);
        }
        old = b;
        fwrite(b.data.data(), b.size, 1, fp);
      }
    }
  }
  fclose(fp);
  exe.write_elf(dir, 2);
}

bool Linker::link(const char *dir) {
  collect_info();
  if (!symbol_is_valid())
    return false;
  alloc_addr();
  symbol_parser();
  relocate();
  assemble_executable();
  export_elf(dir);
  return true;
}
