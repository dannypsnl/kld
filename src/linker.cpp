#include "linker.h"
#include <stdio.h>
#include <string.h>

Block::Block(char *d, unsigned int off, unsigned int s)
    : data{d}, offset{off}, size{s} {}
Block::~Block() { delete[] data; }

SegList::~SegList() {
  owner_list.clear();
  for (auto i = 0; i < blocks.size(); ++i) {
    delete blocks[i];
  }
  blocks.clear();
}

void SegList::alloc_addr(string name, unsigned int &base, unsigned int &off) {
  begin = off;
  if (name != ".bss")
    base += (MEM_ALIGN - base % MEM_ALIGN) % MEM_ALIGN;
  int align = DISC_ALIGN;
  if (name == ".text")
    align = 16;
  off += (align - off % align) % align;
  base = base - base % MEM_ALIGN + off % MEM_ALIGN;

  base_addr = base;
  offset = off;
  size = 0;
  for (auto i = 0; i < owner_list.size(); ++i) {
    size += (DISC_ALIGN - size % DISC_ALIGN) % DISC_ALIGN;
    Elf32_Shdr *seg = owner_list[i]->shdr_tab[name];
    if (name != ".bss") {
      char *buf = new char[seg->sh_size];
      owner_list[i]->get_data(buf, seg->sh_offset, seg->sh_size);
      blocks.push_back(new Block(buf, size, seg->sh_size));
    }
    seg->sh_addr = base + size;
    size += seg->sh_size;
  }
  base += size;
  if (name != ".bss")
    off += size;
}

void SegList::reloc_addr(unsigned int relAddr, unsigned char type,
                         unsigned int symAddr) {
  unsigned int relOffset = relAddr - base_addr;
  for (auto i = 0; i < blocks.size(); ++i) {
    if (blocks[i]->offset <= relOffset &&
        blocks[i]->offset + blocks[i]->size > relOffset) {
      auto b = blocks[i];
      int *pAddr = (int *)(b->data + relOffset - b->offset);
      if (type == R_386_32) {
        *pAddr = symAddr;
      } else if (type == R_386_PC32) {
        *pAddr = symAddr - relAddr + *pAddr;
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
    seg_lists[seg_names[i]] = new SegList();
}

void Linker::add_elf(const char *dir) {
  Elf_file *elf = new Elf_file();
  elf->read_elf(dir);
  elfs.push_back(elf);
}

void Linker::collect_info() {
  for (auto i = 0; i < elfs.size(); ++i) {
    Elf_file *elf = elfs[i];
    for (auto i = 0; i < seg_names.size(); ++i)
      if (elf->shdr_tab.find(seg_names[i]) != elf->shdr_tab.end())
        seg_lists[seg_names[i]]->owner_list.push_back(elf);
    for (auto symIt = elf->sym_tab.begin(); symIt != elf->sym_tab.end();
         ++symIt) {
      {
        SymLink *symLink = new SymLink();
        symLink->name = symIt->first;
        if (symIt->second->st_shndx == STN_UNDEF) {
          symLink->recv = elf;
          symLink->prov = NULL;
          symbol_links.push_back(symLink);
        } else {
          symLink->prov = elf;
          symLink->recv = NULL;
          symbol_def.push_back(symLink);
        }
      }
    }
  }
}

bool Linker::symbol_is_valid() {
  bool flag = true;
  start_owner = NULL;
  for (auto i = 0; i < symbol_def.size(); ++i) {
    if (ELF32_ST_BIND(
            symbol_def[i]->prov->sym_tab[symbol_def[i]->name]->st_info) !=
        STB_GLOBAL) {
      continue;
    }
    if (symbol_def[i]->name == START) {
      start_owner = symbol_def[i]->prov;
    }
    for (auto j = i + 1; j < symbol_def.size(); ++j) {
      if (ELF32_ST_BIND(
              symbol_def[j]->prov->sym_tab[symbol_def[j]->name]->st_info) !=
          STB_GLOBAL) {
        continue;
      }
      if (symbol_def[i]->name == symbol_def[j]->name) {
        printf("symbol %s in %s and %s are redefined.\n",
               symbol_def[i]->name.c_str(), symbol_def[i]->prov->elf_dir,
               symbol_def[j]->prov->elf_dir);
        flag = false;
      }
    }
  }
  if (start_owner == NULL) {
    printf("linker cannot find entry %s.\n", START);
    flag = false;
  }
  for (auto i = 0; i < symbol_links.size(); ++i) {
    for (auto j = 0; j < symbol_def.size(); ++j) {
      if (ELF32_ST_BIND(
              symbol_def[j]->prov->sym_tab[symbol_def[j]->name]->st_info) !=
          STB_GLOBAL)
        continue;
      if (symbol_links[i]->name == symbol_def[j]->name

      ) {
        symbol_links[i]->prov = symbol_def[j]->prov;
        symbol_def[j]->recv = symbol_def[j]->prov;
      }
    }
    if (symbol_links[i]->prov == NULL) {
      unsigned char info =
          symbol_links[i]->recv->sym_tab[symbol_links[i]->name]->st_info;
      string type;
      if (ELF32_ST_TYPE(info) == STT_OBJECT) {
        type = "variable";
      } else if (ELF32_ST_TYPE(info) == STT_FUNC) {
        type = "function";
      } else {
        type = "symbol";
      }
      printf("in file %s type %s named %s is undefined.\n",
             symbol_links[i]->recv->elf_dir, type.c_str(),
             symbol_links[i]->name.c_str());
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
  for (auto i = 0; i < seg_names.size(); ++i) {
    seg_lists[seg_names[i]]->alloc_addr(seg_names[i], curAddr, curOff);
  }
}

void Linker::symbol_parser() {
  for (auto i = 0; i < symbol_def.size(); ++i) {
    Elf32_Sym *sym = symbol_def[i]->prov->sym_tab[symbol_def[i]->name];
    string segName = symbol_def[i]->prov->shdr_names[sym->st_shndx];
    sym->st_value =
        sym->st_value + symbol_def[i]->prov->shdr_tab[segName]->sh_addr;
  }
  for (auto i = 0; i < symbol_links.size(); ++i) {
    Elf32_Sym *provsym = symbol_links[i]->prov->sym_tab[symbol_links[i]->name];
    Elf32_Sym *recvsym = symbol_links[i]->recv->sym_tab[symbol_links[i]->name];
    recvsym->st_value = provsym->st_value;
  }
}

void Linker::relocate() {
  for (auto i = 0; i < elfs.size(); ++i) {
    vector<RelItem *> tab = elfs[i]->rel_tab;
    for (auto j = 0; j < tab.size(); ++j) {
      Elf32_Sym *sym = elfs[i]->sym_tab[tab[j]->rel_name];
      unsigned int symAddr = sym->st_value;
      unsigned int relAddr =
          elfs[i]->shdr_tab[tab[j]->seg_name]->sh_addr + tab[j]->rel->r_offset;

      seg_lists[tab[j]->seg_name]->reloc_addr(
          relAddr, ELF32_R_TYPE(tab[j]->rel->r_info), symAddr);
    }
  }
}

void Linker::assemble_executable() {
  int *p_id = (int *)exe.ehdr.e_ident;
  *p_id = 0x464c457f;
  p_id++;
  *p_id = 0x010101;
  p_id++;
  *p_id = 0;
  p_id++;
  *p_id = 0;
  exe.ehdr.e_type = ET_EXEC;
  exe.ehdr.e_machine = EM_386;
  exe.ehdr.e_version = EV_CURRENT;
  exe.ehdr.e_flags = 0;
  exe.ehdr.e_ehsize = 52;
  unsigned int curOff = 52 + 32 * seg_names.size();
  exe.add_section_header("", 0, 0, 0, 0, 0, 0, 0, 0, 0);
  int shstrtabSize = 26;
  for (auto i = 0; i < seg_names.size(); ++i) {
    string name = seg_names[i];
    shstrtabSize += name.length() + 1;

    Elf32_Word flags = PF_W | PF_R;
    Elf32_Word filesz = seg_lists[name]->size;
    if (name == ".text") {
      flags = PF_X | PF_R;
    }
    if (name == ".bss") {
      filesz = 0;
    }
    exe.add_program_header(PT_LOAD, seg_lists[name]->offset,
                           seg_lists[name]->base_addr, filesz,
                           seg_lists[name]->size, flags, MEM_ALIGN);
    curOff = seg_lists[name]->offset;

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
    exe.add_section_header(name, sh_type, sh_flags, seg_lists[name]->base_addr,
                           seg_lists[name]->offset, seg_lists[name]->size,
                           SHN_UNDEF, 0, sh_align, 0);
  }
  exe.ehdr.e_phoff = 52;
  exe.ehdr.e_phentsize = 32;
  exe.ehdr.e_phnum = seg_names.size();
  char *str = exe.shstrtab = new char[shstrtabSize];
  exe.shstrtab_size = shstrtabSize;
  int index = 0;
  map<string, int> shstrIndex{};
  shstrIndex[".shstrtab"] = index;
  strcpy(str + index, ".shstrtab");
  index += 10;
  shstrIndex[".symtab"] = index;
  strcpy(str + index, ".symtab");
  index += 8;
  shstrIndex[".strtab"] = index;
  strcpy(str + index, ".strtab");
  index += 8;
  shstrIndex[""] = index - 1;
  for (auto i = 0; i < seg_names.size(); ++i) {
    shstrIndex[seg_names[i]] = index;
    strcpy(str + index, seg_names[i].c_str());
    index += seg_names[i].length() + 1;
  }
  exe.add_section_header(".shstrtab", SHT_STRTAB, 0, 0, curOff, shstrtabSize,
                         SHN_UNDEF, 0, 1, 0);
  exe.ehdr.e_shstrndx = exe.get_seg_index(".shstrtab");
  curOff += shstrtabSize;
  exe.ehdr.e_shoff = curOff;
  exe.ehdr.e_shentsize = 40;
  exe.ehdr.e_shnum = 4 + seg_names.size();
  curOff += 40 * (4 + seg_names.size());
  exe.add_section_header(".symtab", SHT_SYMTAB, 0, 0, curOff,
                         (1 + symbol_def.size()) * 16, 0, 0, 1, 16);
  exe.shdr_tab[".symtab"]->sh_link = exe.get_seg_index(".symtab") + 1;
  int strtabSize = 0;
  exe.add_symbol("", NULL);
  for (auto i = 0; i < symbol_def.size(); ++i) {
    string name = symbol_def[i]->name;
    strtabSize += name.length() + 1;
    Elf32_Sym *sym = symbol_def[i]->prov->sym_tab[name];
    sym->st_shndx =
        exe.get_seg_index(symbol_def[i]->prov->shdr_names[sym->st_shndx]);
    exe.add_symbol(name, sym);
  }
  exe.ehdr.e_entry = exe.sym_tab[START]->st_value;
  curOff += (1 + symbol_def.size()) * 16;
  exe.add_section_header(".strtab", SHT_STRTAB, 0, 0, curOff, strtabSize,
                         SHN_UNDEF, 0, 1, 0);
  str = exe.strtab = new char[strtabSize];
  exe.strtab_size = strtabSize;
  index = 0;
  map<string, int> strIndex;
  strIndex[""] = strtabSize - 1;
  for (auto i = 0; i < symbol_def.size(); ++i) {
    strIndex[symbol_def[i]->name] = index;
    strcpy(str + index, symbol_def[i]->name.c_str());
    index += symbol_def[i]->name.length() + 1;
  }
  for (auto i = exe.sym_tab.begin(); i != exe.sym_tab.end(); ++i) {
    i->second->st_name = strIndex[i->first];
  }
  for (auto i = exe.shdr_tab.begin(); i != exe.shdr_tab.end(); ++i) {
    i->second->sh_name = shstrIndex[i->first];
  }
}

void Linker::export_elf(const char *dir) {
  exe.write_elf(dir, 1);
  FILE *fp = fopen(dir, "a+");
  char pad[1] = {0};
  for (auto i = 0; i < seg_names.size(); ++i) {
    SegList *sl = seg_lists[seg_names[i]];
    int padnum = sl->offset - sl->begin;
    while (padnum--)
      fwrite(pad, 1, 1, fp);
    if (seg_names[i] != ".bss") {
      Block *old = NULL;
      char instPad[1] = {(char)0x90};
      for (auto j = 0; j < sl->blocks.size(); ++j) {
        Block *b = sl->blocks[j];
        if (old != NULL) {
          padnum = b->offset - (old->offset + old->size);
          while (padnum--)
            fwrite(instPad, 1, 1, fp);
        }
        old = b;
        fwrite(b->data, b->size, 1, fp);
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

Linker::~Linker() {
  for (auto i = seg_lists.begin(); i != seg_lists.end(); ++i) {
    delete i->second;
  }
  seg_lists.clear();
  for (auto i = symbol_links.begin(); i != symbol_links.end(); ++i) {
    delete *i;
  }
  symbol_links.clear();
  for (auto i = symbol_def.begin(); i != symbol_def.end(); ++i) {
    delete *i;
  }
  symbol_def.clear();
  for (auto i = 0; i < elfs.size(); ++i) {
    delete elfs[i];
  }
  elfs.clear();
}
