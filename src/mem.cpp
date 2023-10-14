#include "memmap/conf.h"
#include "memmap/proc.h"

#include <windows.h>
#include <errno.h>
#include <algorithm>

namespace {

bool _mincore_strict_policy = false;

} // anonymous

extern "C" {

void set_mincore_strict_policy(int strict) {
    _mincore_strict_policy = strict;
}

extern long sysconf(int name) __attribute((weak));

int getpagesize() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
}

long memmap_sysconf(int name) {
    if((name == _SC_PAGESIZE) || (name == _SC_PAGE_SIZE)) {
        return getpagesize();
    } else if(&sysconf) {
        return sysconf(name);
    }
    errno = EINVAL;
    return -1;
}

int mincore(void* start, size_t length, unsigned char* status) {
    int retval = 0;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    long page_size = si.dwPageSize;

    uintptr_t start_address = (uintptr_t) start;
    uintptr_t last_address = start_address + length;
    uintptr_t remainder = (start_address % page_size);
    if(remainder) {
        if(_mincore_strict_policy) {
            return errno = EINVAL, -1;
        } else {
            start = (void*)(start_address -= remainder);
        }
    }

    if(_mincore_strict_policy && (last_address > (uintptr_t)si.lpMaximumApplicationAddress)) {
        errno = ENOMEM;
        retval = -1;
    }
    // rounding up to page_size
    last_address += page_size - 1;
    last_address -= last_address & page_size;

    // VirtualQuery (iterative call of, once per homogeneous page range)
    while(start_address < last_address) {
        MEMORY_BASIC_INFORMATION mbi;
        VirtualQuery(start, &mbi, sizeof(mbi));
        const bool is_resident = mbi.State == MEM_COMMIT;
        const bool is_unallocd = mbi.State == MEM_FREE;
        if(_mincore_strict_policy && is_unallocd) {
            errno = ENOMEM;
            retval = -1;
        }
        uintptr_t upperb = std::min(last_address, start_address + (uintptr_t)mbi.RegionSize);
        while(start_address < upperb) {
            *status++ = is_resident;
            start_address += page_size;
        }
        start = (void*)start_address;
    }

    return retval;
}

} // extern "C"
