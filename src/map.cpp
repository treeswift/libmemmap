#include "sys/mman.h"
#include "memmap/conf.h"

// implementation
#include <windows.h>
#include <errno.h>
#include <io.h>

namespace
{
using namespace mem;
using namespace map;

// Global data. No thread safety here at all. AT ALL. Period.
// All custom logic here must be confined to program startup.
HANDLE GetOSFHandle(int fd) { return (HANDLE)_get_osfhandle(fd); }
handle_from_posix_fd_func const kDefFd2HandleFunc = &GetOSFHandle;
Fd2HANDLE _curFd2HandleImpl = DefFd2Handle();

enum class ModusVivendi {
    NORMAL = 0x78723a15, // a whimsical constant value least likely to cause a false positive
    EMERGENCY = 0,
} _modus_vivendi = ModusVivendi::NORMAL;

} // anonymouys / nonreusable

// m[un]map(ANON): VirtualAlloc, VirtualFree
// m[un]map(file): MapViewOfFile(map), UnmapViewOfFile(view)

namespace mem {
namespace map {

void SetFd2Handle(Fd2HANDLE delegate) {
    _curFd2HandleImpl = delegate;
}

Fd2HANDLE DefFd2Handle() {
    return kDefFd2HandleFunc;
}

} // namespace map
} // namespace mem

extern "C" {

///////////////////
// configuration //
///////////////////

handle_from_posix_fd_func def_handle_from_posix_fd_func() {
    return kDefFd2HandleFunc;
}

int set_handle_from_posix_fd_func(handle_from_posix_fd_func func) {
    return func ? SetFd2Handle(func), 0 : (errno = EINVAL), -1;
}

int set_handle_from_posix_fd_hook(handle_from_posix_fd_hook hook, void * hint) {
    return hook ? SetFd2Handle([=](int fd){ return hook(fd, hint); }), 0 : (errno = EINVAL), -1;
}

bool TrustTheHeap() {
    return _modus_vivendi == ModusVivendi::NORMAL;
}

void emergency_mode_assume_unreliable_heap() {
    _modus_vivendi = ModusVivendi::EMERGENCY;
}

/////////////////////
// POSIX interface //
/////////////////////

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t off) {
    // there are two crucially different cases: file view and anonymous memory
    // HANDLE(file)->HANDLE(map): map=CreateFileMapping(file)
    // (fd == 0): VirtualAlloc
    return nullptr;
}

int munmap(void* addr, size_t length) {
    // *** IMPLEMENTATION NOTES ***
    // We want `munmap` to be a general purpose semantic equivalent of its POSIX counterpart.
    // Therefore we can only rely on a runtime examination of the page state.

    // NOTE: it is not possible to poke a hole in a FileView.
    // When an entire file view is closed, we UnmapViewOfFile(view);
    // when all mappings of a file are closed, we CloseHandle(map).
    // Side effects of a partial munmap() on a file view are replicated w/`msync`
    // We stop tracking status of "logically unmapped" regions in emergency mode.
    // For anonymous regions, we do VirtualFree().

    return -1;
}

int mprotect(void* addr, size_t length, int flags) {
    // mprotect: VirtualProtect
    return -1;
}

int msync(void* addr, size_t length, int flags) {
    // msync: FlushViewOfFile
    return -1;
}

/**
 * NOTE(MSDN): "offering and reclaiming virtual memory is similar to using the MEM_RESET[_UNDO]...
 * except that OfferVirtualMemory removes the memory from the process working set and restricts
 * access to the offered pages until they are reclaimed."
 * Therefore {Offer|Reclaim|Discard}VirtualMemory aren't strict equivalents of `madvise` (^^^).
 * Also, `MADV_FREE` has the "last-moment writes" semantic that isn't fully supported by Offer.
 */
int madvise(void* addr, size_t len, int advice) {
    // madvise: normally, VirtualAlloc: MADV_DONTNEED->MEM_RESET, MADV_WILLNEED->MEM_RESET_UNDO

    return -1;
}

int mlock (const void* addr, size_t length) {
    // VirtualLock
    return -1;
}

int munlock(const void* addr, size_t length) {
    // VirtualUnlock
    return -1;
}

int mlockall(int flags) {
    // *all: no such shortcuts on Windows. The process will have to iterate through
    //       its entire memory map and lock/unlock all committed pages it encounters.
    //       (see `mincore` below; its underlying loop can be reused.)

    return -1;
}

int munlockall() {
    // ditto (see `mlockall`)

    return -1;
}

/* MOREINFO move queries to src/map.cpp? */
int mincore(void* start, size_t length, unsigned char* status) {
    // VirtualQuery (iterative call of, once per homogeneous page range)
    return -1;
}

} // extern "C"
