#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <elf.h>
#include <sys/mman.h>
#include <stdint.h>

int loadelf(char *exec)
{
 
    #ifdef __x86_64__   
        typedef Elf64_Ehdr elfhdr;
        typedef Elf64_Phdr prohdr;
	typedef uint64_t uint_t;
	uint_t laddress = 0x00400000;
    #else
        typedef Elf32_Ehdr elfhdr;
        typedef Elf32_Phdr prohdr;
	typedef uint32_t uint_t;
	uint_t laddress = 0x08048000;
    #endif 


    elfhdr *ehdr;
    prohdr *phdr;
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

    for (int i = 0, p = 0; i < ehdr->e_phnum; i++) {
	if (phdr[i].p_vaddr >= laddress && phdr[i].p_type == PT_LOAD) {
	    uint_t address = phdr[i].p_vaddr;
	    uint_t size    = phdr[i].p_memsz;
	    uint_t offset  = phdr[i].p_offset;
	    uint_t align_address = address & ~(getpagesize()-1);
	    uint_t mmap_address  = NULL;

	    mmap_address = (uint_t)mmap((void *)align_address, address - align_address + size, 
			   PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED, 0, 0);

	    if (mmap_address != align_address) return EXIT_FAILURE;
	    if (lseek(in, offset, SEEK_SET) < 0) return EXIT_FAILURE;
	    if (read(in, (void *)address, size) < size) return EXIT_FAILURE;
             
	    if (p == 2) break;
	    ++p; 
	}
    }
    
    return EXIT_SUCCESS;
}
