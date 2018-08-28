#include "loadelf.h"
#include <stdlib.h>

// gcc -fpic -pie -std=gnu99 -o demo demo.c loadelf.c 

int main(int argc, char **argv)
{

    if (argc < 2) exit(EXIT_FAILURE);
    if (loadelf(argv[1]) == EXIT_FAILURE) exit(EXIT_FAILURE);

    int (*functionPtr)(int,int) = 0x0000000000400EA0;
    (*functionPtr)(0xFFFF, 0x8000);

    return 0;
}
