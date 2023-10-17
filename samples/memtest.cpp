#include <stdio.h>
#include <fcntl.h>
#include "sys/mman.h"
#include "memmap/conf.h"
#include "memmap/proc.h"
#include <assert.h>
#include <atomic>

constexpr const char* kTestFile = "test-memmap.dat";
constexpr std::size_t kBSz = 1024;
constexpr std::size_t kKbs = 140;

constexpr std::size_t kFileSize = kBSz * kKbs;
constexpr std::size_t kFileInto = kBSz;
constexpr std::size_t kFileSpan = kBSz * 5;
constexpr std::size_t kSyncSpan = kBSz;

constexpr std::size_t kCowField = kBSz * 135;

constexpr uint32_t kBackground = 0x07070707;
constexpr uint32_t kForeground = 0x23232323;

void write_and_read(void* addr) {
    ((volatile char*) addr)[3] = '#';
    ((volatile char*) addr)[2] = '#';
    ((volatile char*) addr)[1] = '#';
    ((volatile char*) addr)[0] = '#';
    assert(*(volatile uint32_t*)addr == kForeground);
}

void pen_stroke(void* mapping) {
    assert(mapping != MAP_FAILED);
    assert(*(volatile uint32_t*)mapping == kBackground);
    write_and_read(mapping);
    assert(*(volatile uint32_t*)mapping == kForeground);
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

void ValidateFileData(int fd) {
    int readvalue;
    lseek(fd, -4, SEEK_CUR);
    assert(read(fd, &readvalue, 4) == 4 && (readvalue == kBackground));
    lseek(fd, kFileInto, SEEK_SET);
    assert(read(fd, &readvalue, 4) == 4 && (readvalue == kForeground));
    lseek(fd, kCowField, SEEK_SET);
    assert(read(fd, &readvalue, 4) == 4 && (readvalue == kBackground));
}

long page_size;

void test_lockall() {
    mlockall(MCL_CURRENT);
    munlockall();
    printf("m[un]lockall() test completed, errno=%d\n", errno);
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
    printf("mmap(anon) p=%p errno=%d\n", my_page, errno);
    assert(my_page != MAP_FAILED);
    unsigned char in_core[2];
    assert(!mincore(my_page, page_size, in_core));
    assert(in_core[0]);
    write_and_read(my_page);

    int fd = CreateDataFile();
    void* my_file = mmap(nullptr, kFileSpan, PROT_DATA, MAP_SHARED, fd, kFileInto); // skip 1kb and map 5kb
    printf("mmap(data) p=%p errno=%d\n", my_file, errno);
    pen_stroke(my_file);
    const bool sync_ok = !msync(my_file, kSyncSpan, MS_SYNC);
    printf("msync(data) p=%p errno=%d\n", my_file, errno);
    fflush(stdout);
    assert(sync_ok);

    // we need a new file handle because the file view created from the old one is now implicitly MAP_SHARED
    int hc = open(kTestFile, O_CREAT | O_RDWR | O_BINARY);
    void* holycow = mmap(nullptr, kFileSpan, PROT_DATA, MAP_PRIVATE, hc, kCowField);
    printf("mmap(cows) p=%p errno=%d\n", my_file, errno);
    pen_stroke(holycow);
    const bool sync_hc = !msync(my_file, kSyncSpan, MS_SYNC);
    printf("msync(cows) p=%p errno=%d\n", my_file, errno);
    fflush(stdout);
    printf(sync_hc ? "holy cow, `msync`ing a private view is a no-op\n"
                       : "nope, `msync`ing a private view is not allowed\n");
    _commit(hc); // FlushFileBuffers()

    // just make sure it doesn't crash (we'll provide a WinRT fallback later)
    void* rnddown = (char*)holycow - (kCowField % page_size);
    madvise(rnddown, page_size, MADV_WILLNEED); // should be a no-op
    madvise(rnddown, page_size, MADV_DONTNEED); // offer to the kernel
    madvise(rnddown, page_size, MADV_WILLNEED); // take back

    ValidateFileData(fd);
    printf("Testing the other (cow) file...\n");
    fflush(stdout);
    ValidateFileData(hc);
    close(fd);
    close(hc);

    printf("mmap()/msync() test completed.\n");
}

int main(int, char **) {
    page_size = getpagesize();
    assert(page_size);

    printf("sys/mman.h API test panel.\n\n");
    fflush(stdout);

    test_mincore();
    test_lockall();
    test_mmap();

    unlink(kTestFile);

    return 0;
}
