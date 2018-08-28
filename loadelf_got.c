#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <elf.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdio.h>
#include <dlfcn.h>

// gcc -fpic -pie -std=gnu99 -o loadelf loadelf.c -ldl

int loadelf_got(char *exec)
{
 
    #ifdef __x86_64__   
        typedef Elf64_Ehdr elfhdr;
        typedef Elf64_Phdr prohdr;
	typedef Elf64_Dyn  dynhdr;
        typedef Elf64_Rela relhdr;
	typedef Elf64_Sym  symhdr;
	typedef uint64_t uint_t;
	uint_t laddress = 0x00400000;
    #else
        typedef Elf32_Ehdr elfhdr;
        typedef Elf32_Phdr prohdr;
	typedef Elf32_Dyn  dynhdr;
        typedef Elf32_Rel relhdr; 	// x86 uses only Elf32_Rel relocation entries
	typedef Elf32_Sym  symhdr;
	typedef uint32_t uint_t;
	uint_t laddress = 0x08048000;
    #endif 


    elfhdr *ehdr;
    prohdr *phdr;
    dynhdr *dyn;
    relhdr *rel;
    symhdr *sym;
    uint_t plen;
    uint_t in;

    in =  open(exec, O_RDONLY);
    if (in < 0) return EXIT_FAILURE;

    ehdr = (elfhdr *)malloc(sizeof(elfhdr));
    if (ehdr == NULL) return EXIT_FAILURE;    
    if (read(in, ehdr, sizeof(elfhdr)) != sizeof(elfhdr)) return EXIT_FAILURE;

    if (strncmp(ehdr->e_ident, ELFMAG, SELFMAG)) return EXIT_FAILURE;
    if (ehdr->e_type != ET_EXEC) return EXIT_FAILURE;

    if (lseek(in, ehdr->e_phoff, SEEK_SET) < 0) return EXIT_FAILURE;
    phdr = (prohdr *)malloc(plen = sizeof(prohdr)*ehdr->e_phnum);
    if (phdr == NULL) return EXIT_FAILURE;
    if (read(in, phdr, plen) < plen) return EXIT_FAILURE;

    uint_t entry_index = 0;
    uint_t no_of_entries = NULL;
    uint_t *entries = NULL;

    uint_t *strtab = NULL;
    uint_t *pltgot = NULL;
    uint_t *symtab = NULL;
    uint_t *jmprel = NULL;
    uint_t relaent = 0;
    uint_t pltrelsz = 0;

    for (int i = 0, p = 0; i < ehdr->e_phnum; i++) {
	uint_t address = phdr[i].p_vaddr;
	uint_t size    = phdr[i].p_memsz;
	uint_t offset  = phdr[i].p_offset;
	if (address >= laddress && phdr[i].p_type == PT_LOAD && p < 2) {
	    uint_t align_address = address & ~(getpagesize()-1);
	    uint_t mmap_address  = NULL;

	    mmap_address = (uint_t)mmap((void *)align_address , address - align_address + size, 
		PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED, 0, 0);

	    if (mmap_address != align_address) return EXIT_FAILURE;
	    if (lseek(in, offset, SEEK_SET) < 0) return EXIT_FAILURE;
	    if (read(in, (void *)address, size) < size) return EXIT_FAILURE;
             
	    ++p; 
	}

	else if (phdr[i].p_type == PT_DYNAMIC) {
	   
	    no_of_entries = size/(sizeof(void *) * 2);
	    entries = calloc(no_of_entries, sizeof(void *));
	    if (entries == NULL) return EXIT_FAILURE;	   
 
	    for (dyn = (dynhdr *)address; dyn->d_tag != DT_NULL; ++dyn) {
		switch(dyn->d_tag) {
	    	    case DT_NEEDED:
			entries[entry_index] = dyn->d_un.d_ptr;
			entry_index++;
			break;
		    case DT_STRTAB:
			strtab = (uint_t *)dyn->d_un.d_ptr;
			break;
		    case DT_SYMTAB:
			symtab = (uint_t *)dyn->d_un.d_ptr;
			break;
		    case DT_PLTGOT:
			pltgot = (uint_t *)dyn->d_un.d_ptr;
			break;
		    case DT_PLTRELSZ:
			pltrelsz = (uint_t)dyn->d_un.d_ptr;
			break;
		    case DT_RELAENT:
		    case DT_RELENT:
			relaent = (uint_t)dyn->d_un.d_ptr;
			break;
		    case DT_JMPREL:
			jmprel = (uint_t *)dyn->d_un.d_ptr;
			break;
		}
	    }
        }
    }

    uint_t *dl_handle = calloc(entry_index, sizeof(void *));
    if (dl_handle == NULL) return EXIT_FAILURE;
    
    for (uint_t i = 0; i < entry_index; i++) {
	dl_handle[i] = (uint_t)dlopen((void *)strtab + entries[i], RTLD_NOW|RTLD_GLOBAL);
    }

    uint_t no_of_got = pltrelsz/relaent;
    sym = (symhdr *)symtab;
 
    uint_t index = 0;
    for (rel = (relhdr *)jmprel; index < no_of_got; rel++, index++) {
   	uint_t *target_got = (uint_t *)rel->r_offset;
   	uint_t sym_index =  rel->r_info;
	char *sym_name = NULL;
	#ifdef __x86_64__
	    sym_index = (sym_index >> 32);
	#else
	    sym_index = (sym_index >> 8);
	#endif
	sym_name = (char *)((void *)strtab + sym[sym_index].st_name);
	
	uint_t *sym_address = NULL;
	for(int i = 0; i < entry_index; i++) {
	    dlerror();
	    sym_address = (uint_t *)dlsym((void *)dl_handle[i], sym_name);
	    if (dlerror() == NULL) break;
	}

	*target_got = (uint_t)sym_address;
    }
    
    return EXIT_SUCCESS;
}

