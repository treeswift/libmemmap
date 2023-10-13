#include "sys/mman.h"
#include "memmap/proc.h"

#include <windows.h>

#include <stdio.h>

// VirtualQueryEx(HANDLE, ...) -> memory info of another process

/* We assume that the address space is 32-bit, which is true for our Windows RT target. */

//

int main(int argc, char **argv) {
#ifdef PAGE_SIZE
    printf("Page size (static):\t0x%x (%ld) bytes\n", PAGE_SIZE, PAGE_SIZE);
#endif

    long page_size = memmap_sysconf(_SC_PAGESIZE);
    printf("Page size (dynamic):\t0x%x (%ld) bytes\n", page_size, page_size);
    return 0;
}
