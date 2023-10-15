#include <stdio.h>
#include "sys/mman.h"
#include "memmap/conf.h"
#include "memmap/proc.h"
#include <assert.h>
#include <atomic>

long page_size;

void write_and_read(void* addr) {
    ((volatile char*) addr)[0] = 0x5;
    ((volatile char*) addr)[1] = 0x5;
    ((volatile char*) addr)[2] = 0x5;
    ((volatile char*) addr)[3] = 0x5;
    assert(*(volatile uint32_t*)addr == 0x5050505);
}

void test_mincore() {
    // set_mincore_strict_policy(false); // default anyway
    errno = 0;

    // at least two pages (as the starting address might not be a page boundary)
    unsigned char in_core[2];
    // address guaranteed to be resident in memory
    void* addr = &page_size;
    
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
}

void test_mmap() {
    errno = 0;
    void* my_page = mmap(nullptr, page_size, PROT_DATA, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    printf("mmap(anon) errno=%d\n", errno);
    assert(my_page != MAP_FAILED);
    unsigned char in_core[2];
    assert(!mincore(my_page, page_size, in_core));
    assert(in_core[0]);
    write_and_read(my_page);

}

int main(int argc, char **) {
    page_size = getpagesize();
    assert(page_size);

    printf("sys/mman.h API test panel.\n\n");
    fflush(stdout);

    test_mincore();
    test_mmap();

    return 0;
}
