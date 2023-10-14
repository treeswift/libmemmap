#include <stdio.h>
#include "sys/mman.h"
#include "memmap/conf.h"
#include "memmap/proc.h"
#include <assert.h>

int main(int argc, char **) {
    printf("sys/mman.h API test panel.\n\n");
    fflush(stdout);

    long page_size = getpagesize();

    // set_mincore_strict_policy(false); // default anyway

    // at least two pages (as the starting address might not be a page boundary)
    unsigned char in_core[2];
    // address guaranteed to be resident in memory
    void* addr = &argc;
    
    int retval = mincore(addr, page_size, in_core);
    assert(!errno);
    assert(!retval);
    assert(*in_core);

    set_mincore_strict_policy(true);

    *in_core = !*in_core;
    retval = mincore(addr, page_size, in_core);

    assert((errno == EINVAL) == !!((uintptr_t)addr) % page_size);
    assert(retval);
    assert(!*in_core);

    *in_core = !*in_core;

    // zero address, guaranteed to be unavailable
    addr = nullptr;
    retval = mincore(addr, page_size, in_core);
    assert(!*in_core);

    printf("mincore() test completed.\n");

    return 0;
}
