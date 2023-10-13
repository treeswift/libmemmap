#include "sys/mman.h"
#include "memmap/proc.h"

#include <windows.h>

#include <stdio.h>

// VirtualQueryEx(HANDLE, ...) -> memory info of another process

/* We assume that the address space is 32-bit, which is true for our Windows RT target. */

//

int main(int argc, char **argv) {

    //

    printf("This is one half.\n");
    return 0;
}
