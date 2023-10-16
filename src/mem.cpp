#include "memmap/conf.h"
#include "memmap/iter.h"
#include "memmap/proc.h"

#include <windows.h>
#include <errno.h>
#include <assert.h>
#include <algorithm>

namespace {

bool _mincore_strict_policy = false;

mem::RangeVisitor CPPize(memmap_range_visitor purefunc_visitor) {
    return [=](const MEMORY_BASIC_INFORMATION& mbi, const MEMMAP_RANGE& range) {
        (*purefunc_visitor)(&mbi, &range);
    };
}

mem::RangePredicate CPPize(memmap_range_predicate purefunc_predicate) {
    return [=](const MEMORY_BASIC_INFORMATION& mbi) {
        return (*purefunc_predicate)(&mbi);
    };
}

} // anonymous

namespace mem
{

MEMMAP_RANGE Range(void* base, std::size_t size) {
    return MEMMAP_RANGE{base, (char*)base + size};
}

void Traverse(const MEMMAP_RANGE& range, RangeVisitor visitor, RangePredicate predicate) {
    MEMMAP_RANGE subrange = range;
    MEMORY_BASIC_INFORMATION mbi;
    while((uintptr_t)subrange.lower < (uintptr_t)range.upper) {
        VirtualQuery(subrange.lower, &mbi, sizeof(mbi));
        assert(mbi.BaseAddress == subrange.lower);
        void* homog_until = (void*)((uintptr_t)subrange.lower + mbi.RegionSize);

        if(predicate(mbi)) {
            subrange.upper = (void*)(std::min((uintptr_t)homog_until, (uintptr_t)range.upper));
            visitor(mbi, subrange);
        }

        subrange.lower = homog_until;
    }
}

void TraverseAllProcessMemory(RangeVisitor visitor, RangePredicate predicate) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    void* lower = si.lpMinimumApplicationAddress;
    void* upper = si.lpMaximumApplicationAddress;
    Traverse({lower, upper}, visitor, predicate);
}

} // namespace mem

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

int gethugepagesize() {
    return (int) GetLargePageMinimum();
}

int get_allocation_granularity() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwAllocationGranularity;
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

void memmap_traverse_addresses_from_to(void* lower, void* upper, memmap_range_visitor visitor, memmap_range_predicate predicate) {
    mem::Traverse({lower, upper}, CPPize(visitor), CPPize(predicate));
}

void memmap_traverse_addresses_from_for(void* base, size_t size, memmap_range_visitor visitor, memmap_range_predicate predicate) {
    mem::Traverse(mem::Range(base, size), CPPize(visitor), CPPize(predicate));
}

void memmap_traverse_committed_from_to(void* lower, void* upper, memmap_range_visitor visitor) {
    mem::Traverse({lower, upper}, CPPize(visitor));
}

void memmap_traverse_committed_from_for(void* base, size_t size, memmap_range_visitor visitor) {
    mem::Traverse(mem::Range(base, size), CPPize(visitor));
}

void memmap_traverse_all_process_memory(memmap_range_visitor visitor, memmap_range_predicate predicate) {
    mem::TraverseAllProcessMemory(CPPize(visitor), CPPize(predicate));
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
