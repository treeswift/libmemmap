#include <stdio.h>
#include <fcntl.h>
#include "sys/mman.h"
#include "memmap/conf.h"
#include "memmap/proc.h"
#include <assert.h>
#include <atomic>

constexpr const char* kTestFile = "test-memmap.dat";
constexpr std::size_t kBSz = 1024;
constexpr std::size_t kKbs = 7;

constexpr std::size_t kFileSize = kBSz * kKbs;
constexpr std::size_t kFileInto = kBSz;
constexpr std::size_t kFileSpan = kBSz * 5;
constexpr std::size_t kSyncSpan = kBSz;

void write_and_read(void* addr) {
    ((volatile char*) addr)[0] = 0x23;
    ((volatile char*) addr)[1] = 0x23;
    ((volatile char*) addr)[2] = 0x23;
    ((volatile char*) addr)[3] = 0x23;
    assert(*(volatile uint32_t*)addr == 0x23232323);
}

int CreateDataFile() {
    unsigned char bytebuf[kBSz];
    int fd = open(kTestFile, O_CREAT | O_RDWR | O_BINARY);
    memset(bytebuf, '\7', kBSz);
    for(std::size_t i = 0; i < kKbs; ++i) {
        assert(write(fd, bytebuf, kBSz) == kBSz); // enough space
    }
    _commit(fd); // equivalent of flush or fsync
    return fd;
}

long page_size;

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
    printf("mmap(anon) p=%p errno=%d\n", my_page, errno);
    assert(my_page != MAP_FAILED);
    unsigned char in_core[2];
    assert(!mincore(my_page, page_size, in_core));
    assert(in_core[0]);
    write_and_read(my_page);

    int fd = CreateDataFile();
    void* my_file = mmap(nullptr, kFileSpan, PROT_DATA, MAP_SHARED, fd, kFileInto); // skip 1kb and map 5kb
    printf("mmap(data) p=%p errno=%d\n", my_file, errno);
    assert(my_page != MAP_FAILED);
    assert(*(volatile uint32_t*)my_file == 0x7070707);
    write_and_read(my_file);
    assert(*(volatile uint32_t*)my_file == 0x23232323);
    bool sync_ok = !msync(my_file, kSyncSpan, MS_SYNC);
    printf("msync(data) p=%p errno=%d\n", my_file, errno);
    fflush(stdout);
    assert(sync_ok);
    // _commit(fd); // FlushFileBuffers()
    int readvalue;
    lseek(fd, -4, SEEK_CUR);
    assert(read(fd, &readvalue, 4) == 4 && (readvalue == 0x7070707));
    lseek(fd, kFileInto, SEEK_SET);
    assert(read(fd, &readvalue, 4) == 4 && (readvalue == 0x23232323));
    close(fd);

    printf("mmap()/msync() test completed.\n");
}

int main(int, char **) {
    page_size = getpagesize();
    assert(page_size);

    printf("sys/mman.h API test panel.\n\n");
    fflush(stdout);

    test_mincore();
    test_mmap();

    return 0;
}
