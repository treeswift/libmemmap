#include "memmap/proc.h"

#include <windows.h>
#include <errno.h>

extern "C" {

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

} // extern "C"
