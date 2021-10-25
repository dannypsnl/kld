#include "linker.h"
#include <stdio.h>
#include <string.h>

Block::Block(char *d, unsigned int off, unsigned int s) {
  data = d;
  offset = off;
  size = s;
}

Block::~Block() { delete[] data; }

SegList::~SegList() {
  ownerList.clear();
  for (int i = 0; i < blocks.size(); ++i) {
    delete blocks[i];
  }
  blocks.clear();
}

void SegList::allocAddr(string name, unsigned int &base, unsigned int &off) {
  begin = off;
  if (name != ".bss")
    base += (MEM_ALIGN - base % MEM_ALIGN) % MEM_ALIGN;
  int align = DISC_ALIGN;
  if (name == ".text")
    align = 16;
  off += (align - off % align) % align;
  base = base - base % MEM_ALIGN + off % MEM_ALIGN;

  baseAddr = base;
  offset = off;
  size = 0;
  for (int i = 0; i < ownerList.size(); ++i) {
    size += (DISC_ALIGN - size % DISC_ALIGN) % DISC_ALIGN;
    Elf32_Shdr *seg = ownerList[i]->shdrTab[name];
    if (name != ".bss") {
      char *buf = new char[seg->sh_size];
      ownerList[i]->getData(buf, seg->sh_offset, seg->sh_size);
      blocks.push_back(new Block(buf, size, seg->sh_size));
    }
    seg->sh_addr = base + size;
    size += seg->sh_size;
  }
  base += size;
  if (name != ".bss")
    off += size;
}

void SegList::relocAddr(unsigned int relAddr, unsigned char type,
                        unsigned int symAddr) {
  unsigned int relOffset = relAddr - baseAddr;
  Block *b = NULL;
  for (int i = 0; i < blocks.size(); ++i) {
    if (blocks[i]->offset <= relOffset &&
        blocks[i]->offset + blocks[i]->size > relOffset) {
      b = blocks[i];
      break;
    }
  }
  int *pAddr = (int *)(b->data + relOffset - b->offset);
  if (type == R_386_32) {
    *pAddr = symAddr;
  } else if (type == R_386_PC32) {
    *pAddr = symAddr - relAddr + *pAddr;
  }
}

Linker::Linker() {
  segNames.push_back(".text");
  segNames.push_back(".data");
  segNames.push_back(".bss");
  for (int i = 0; i < segNames.size(); ++i)
    segLists[segNames[i]] = new SegList();
}

void Linker::addElf(const char *dir) {
  Elf_file *elf = new Elf_file();
  elf->readElf(dir);
  elfs.push_back(elf);
}

void Linker::collectInfo() {
  for (int i = 0; i < elfs.size(); ++i) {
    Elf_file *elf = elfs[i];
    for (int i = 0; i < segNames.size(); ++i)
      if (elf->shdrTab.find(segNames[i]) != elf->shdrTab.end())
        segLists[segNames[i]]->ownerList.push_back(elf);
    for (map<string, Elf32_Sym *>::iterator symIt = elf->symTab.begin();
         symIt != elf->symTab.end(); ++symIt) {
      {
        SymLink *symLink = new SymLink();
        symLink->name = symIt->first;
        if (symIt->second->st_shndx == STN_UNDEF) {
          symLink->recv = elf;
          symLink->prov = NULL;
          symLinks.push_back(symLink);
        } else {
          symLink->prov = elf;
          symLink->recv = NULL;
          symDef.push_back(symLink);
        }
      }
    }
  }
}

bool Linker::symValid() {
  bool flag = true;
  startOwner = NULL;
  for (int i = 0; i < symDef.size(); ++i) {
    if (ELF32_ST_BIND(symDef[i]->prov->symTab[symDef[i]->name]->st_info) !=
        STB_GLOBAL)
      continue;
    if (symDef[i]->name == START)
      startOwner = symDef[i]->prov;
    for (int j = i + 1; j < symDef.size(); ++j) {
      if (ELF32_ST_BIND(symDef[j]->prov->symTab[symDef[j]->name]->st_info) !=
          STB_GLOBAL)
        continue;
      if (symDef[i]->name == symDef[j]->name) {
        printf("符号名%s在文件%s和文件%s中发生链接冲突。\n",
               symDef[i]->name.c_str(), symDef[i]->prov->elf_dir,
               symDef[j]->prov->elf_dir);
        flag = false;
      }
    }
  }
  if (startOwner == NULL) {
    printf("链接器找不到程序入口%s。\n", START);
    flag = false;
  }
  for (int i = 0; i < symLinks.size(); ++i) {
    for (int j = 0; j < symDef.size(); ++j) {
      if (ELF32_ST_BIND(symDef[j]->prov->symTab[symDef[j]->name]->st_info) !=
          STB_GLOBAL)
        continue;
      if (symLinks[i]->name == symDef[j]->name

      ) {
        symLinks[i]->prov = symDef[j]->prov;
        symDef[j]->recv = symDef[j]->prov;
      }
    }
    if (symLinks[i]->prov == NULL) {
      unsigned char info =
          symLinks[i]->recv->symTab[symLinks[i]->name]->st_info;
      string type;
      if (ELF32_ST_TYPE(info) == STT_OBJECT)
        type = "变量";
      else if (ELF32_ST_TYPE(info) == STT_FUNC)
        type = "函数";
      else
        type = "符号";
      printf("文件%s的%s名%s未定义。\n", symLinks[i]->recv->elf_dir,
             type.c_str(), symLinks[i]->name.c_str());
      if (flag)
        flag = false;
    }
  }
  return flag;
}

void Linker::allocAddr() {
  unsigned int curAddr = BASE_ADDR;
  unsigned int curOff = 52 + 32 * segNames.size();
  for (int i = 0; i < segNames.size();
       ++i) //按照类型分配地址，不紧邻.data与.bss段
  {
    segLists[segNames[i]]->allocAddr(segNames[i], curAddr, curOff);
  }
}

void Linker::symParser() {
  for (int i = 0; i < symDef.size(); ++i) {
    Elf32_Sym *sym = symDef[i]->prov->symTab[symDef[i]->name];
    string segName = symDef[i]->prov->shdrNames[sym->st_shndx];
    sym->st_value = sym->st_value + symDef[i]->prov->shdrTab[segName]->sh_addr;
  }
  for (int i = 0; i < symLinks.size(); ++i) {
    Elf32_Sym *provsym = symLinks[i]->prov->symTab[symLinks[i]->name];
    Elf32_Sym *recvsym = symLinks[i]->recv->symTab[symLinks[i]->name];
    recvsym->st_value = provsym->st_value;
  }
}

void Linker::relocate() {
  for (int i = 0; i < elfs.size(); ++i) {
    vector<RelItem *> tab = elfs[i]->relTab;
    for (int j = 0; j < tab.size(); ++j) {
      Elf32_Sym *sym = elfs[i]->symTab[tab[j]->relName];
      unsigned int symAddr = sym->st_value;
      unsigned int relAddr =
          elfs[i]->shdrTab[tab[j]->segName]->sh_addr + tab[j]->rel->r_offset;

      segLists[tab[j]->segName]->relocAddr(
          relAddr, ELF32_R_TYPE(tab[j]->rel->r_info), symAddr);
    }
  }
}

void Linker::assemExe() {
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
  unsigned int curOff = 52 + 32 * segNames.size();
  exe.addShdr("", 0, 0, 0, 0, 0, 0, 0, 0, 0);
  int shstrtabSize = 26;
  for (int i = 0; i < segNames.size(); ++i) {
    string name = segNames[i];
    shstrtabSize += name.length() + 1;

    Elf32_Word flags = PF_W | PF_R;
    Elf32_Word filesz = segLists[name]->size;
    if (name == ".text")
      flags = PF_X | PF_R;
    if (name == ".bss")
      filesz = 0;
    exe.addPhdr(PT_LOAD, segLists[name]->offset, segLists[name]->baseAddr,
                filesz, segLists[name]->size, flags, MEM_ALIGN);
    curOff = segLists[name]->offset;

    Elf32_Word sh_type = SHT_PROGBITS;
    Elf32_Word sh_flags = SHF_ALLOC | SHF_WRITE;
    Elf32_Word sh_align = 4;
    if (name == ".bss")
      sh_type = SHT_NOBITS;
    if (name == ".text") {
      sh_flags = SHF_ALLOC | SHF_EXECINSTR;
      sh_align = 16;
    }
    exe.addShdr(name, sh_type, sh_flags, segLists[name]->baseAddr,
                segLists[name]->offset, segLists[name]->size, SHN_UNDEF, 0,
                sh_align, 0);
  }
  exe.ehdr.e_phoff = 52;
  exe.ehdr.e_phentsize = 32;
  exe.ehdr.e_phnum = segNames.size();
  char *str = exe.shstrtab = new char[shstrtabSize];
  exe.shstrtabSize = shstrtabSize;
  int index = 0;
  map<string, int> shstrIndex;
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
  for (int i = 0; i < segNames.size(); ++i) {
    shstrIndex[segNames[i]] = index;
    strcpy(str + index, segNames[i].c_str());
    index += segNames[i].length() + 1;
  }
  exe.addShdr(".shstrtab", SHT_STRTAB, 0, 0, curOff, shstrtabSize, SHN_UNDEF, 0,
              1, 0);
  exe.ehdr.e_shstrndx = exe.getSegIndex(".shstrtab");
  curOff += shstrtabSize;
  exe.ehdr.e_shoff = curOff;
  exe.ehdr.e_shentsize = 40;
  exe.ehdr.e_shnum = 4 + segNames.size();
  curOff += 40 * (4 + segNames.size());
  exe.addShdr(".symtab", SHT_SYMTAB, 0, 0, curOff, (1 + symDef.size()) * 16, 0,
              0, 1, 16);
  exe.shdrTab[".symtab"]->sh_link = exe.getSegIndex(".symtab") + 1;
  int strtabSize = 0;
  exe.addSym("", NULL);
  for (int i = 0; i < symDef.size(); ++i) {
    string name = symDef[i]->name;
    strtabSize += name.length() + 1;
    Elf32_Sym *sym = symDef[i]->prov->symTab[name];
    sym->st_shndx = exe.getSegIndex(symDef[i]->prov->shdrNames[sym->st_shndx]);
    exe.addSym(name, sym);
  }
  exe.ehdr.e_entry = exe.symTab[START]->st_value;
  curOff += (1 + symDef.size()) * 16;
  exe.addShdr(".strtab", SHT_STRTAB, 0, 0, curOff, strtabSize, SHN_UNDEF, 0, 1,
              0);
  str = exe.strtab = new char[strtabSize];
  exe.strtabSize = strtabSize;
  index = 0;
  map<string, int> strIndex;
  strIndex[""] = strtabSize - 1;
  for (int i = 0; i < symDef.size(); ++i) {
    strIndex[symDef[i]->name] = index;
    strcpy(str + index, symDef[i]->name.c_str());
    index += symDef[i]->name.length() + 1;
  }
  for (map<string, Elf32_Sym *>::iterator i = exe.symTab.begin();
       i != exe.symTab.end(); ++i) {
    i->second->st_name = strIndex[i->first];
  }
  for (map<string, Elf32_Shdr *>::iterator i = exe.shdrTab.begin();
       i != exe.shdrTab.end(); ++i) {
    i->second->sh_name = shstrIndex[i->first];
  }
}

void Linker::exportElf(const char *dir) {
  exe.writeElf(dir, 1);
  FILE *fp = fopen(dir, "a+");
  char pad[1] = {0};
  for (int i = 0; i < segNames.size(); ++i) {
    SegList *sl = segLists[segNames[i]];
    int padnum = sl->offset - sl->begin;
    while (padnum--)
      fwrite(pad, 1, 1, fp);
    if (segNames[i] != ".bss") {
      Block *old = NULL;
      char instPad[1] = {(char)0x90};
      for (int j = 0; j < sl->blocks.size(); ++j) {
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
  exe.writeElf(dir, 2);
}

bool Linker::link(const char *dir) {
  collectInfo();
  if (!symValid())
    return false;
  allocAddr();
  symParser();
  relocate();
  assemExe();
  exportElf(dir);
  return true;
}

Linker::~Linker() {
  for (map<string, SegList *>::iterator i = segLists.begin();
       i != segLists.end(); ++i) {
    delete i->second;
  }
  segLists.clear();
  for (vector<SymLink *>::iterator i = symLinks.begin(); i != symLinks.end();
       ++i) {
    delete *i;
  }
  symLinks.clear();
  for (vector<SymLink *>::iterator i = symDef.begin(); i != symDef.end(); ++i) {
    delete *i;
  }
  symDef.clear();
  for (int i = 0; i < elfs.size(); ++i) {
    delete elfs[i];
  }
  elfs.clear();
}