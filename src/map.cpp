#include "sys/mman.h"
#include "memmap/conf.h"
#include "memmap/proc.h"

#include "dbg.h" // tracing

// implementation
#include <windows.h>
// #include <werapi.h> // breaks when included, so we `extern` the only symbol we need
#include <errno.h>
#include <io.h> // _get_osfhandle
#include <assert.h>

namespace
{
using namespace mem;
using namespace map;

constexpr DWORD kProtectionTranslationLUT[] = {
    // bit order (top down): xwr
    PAGE_NOACCESS,
    PAGE_READONLY,
    PAGE_READWRITE, // write access implies read access
    PAGE_READWRITE, // promotes to PAGE_WRITECOPY for COW
    PAGE_EXECUTE,
    PAGE_EXECUTE_READ,
    PAGE_EXECUTE_READWRITE, // ditto
    PAGE_EXECUTE_READWRITE, // ditto, to PAGE_EXECUTE_WRITECOPY
};

// Global data. No thread safety here at all. AT ALL. Period.
// All custom logic here must be confined to program startup.
HANDLE GetOSFHandle(int fd) { return (HANDLE)_get_osfhandle(fd); }
handle_from_posix_fd_func const kDefFd2HandleFunc = &GetOSFHandle;
Fd2HANDLE _curFd2HandleImpl = DefFd2Handle();

const long _page_size = getpagesize();

enum class ModusVivendi {
    NORMAL = 0x78723a15, // a whimsical constant value least likely to cause a false positive
    EMERGENCY = 0,
} _modus_vivendi = ModusVivendi::NORMAL;

enum fd_access_inference_policy _exec_policy = fd_access_inference_policy__probe;
enum fd_access_inference_policy _write_policy = fd_access_inference_policy__probe;

bool _mmap_strict_policy = false;
bool _mmap_apply_executable_image_sections = false;

constexpr int kOfferPriorityNormal = VmOfferPriorityNormal; // 4
constexpr int kOfferPriorityRange = VmOfferPriorityNormal - VmOfferPriorityVeryLow; // 4-1
OFFER_PRIORITY _offer_prio = VmOfferPriorityLow;

uintptr_t RoundDown(void* &addr, size_t &length) {
    uintptr_t base_addr = (uintptr_t) addr;
    uintptr_t remainder = base_addr % _page_size;
    base_addr -= remainder;
    length += remainder;
    addr = (void*) base_addr;
    return remainder;
}

int RoundDownFailFast(void* &addr, size_t &length) {
    return (RoundDown(addr, length) && _mmap_strict_policy)
        ? (errno = EINVAL, -1) : 0;
}

int RoundUpFailFast(size_t &length) {
    const auto oldlen = length;
    length += _page_size - 1;
    length -= length % _page_size;
    return (_mmap_strict_policy && (oldlen != length))
        ? (errno = EINVAL, -1) : 0;
}

} // anonymouys / nonreusable

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

void set_exec_bit_inference_policy(enum fd_access_inference_policy policy) {
    _exec_policy = policy;
}

void set_write_bit_inference_policy(enum fd_access_inference_policy policy) {
    _write_policy = policy;
}

void set_mmap_strict_policy(int strict) {
    _mmap_strict_policy = strict;
}

void set_mmap_apply_executable_image_sections(int parse_coff) {
    _mmap_apply_executable_image_sections = parse_coff;
}

void set_madvise_offer_resoluteness(int res) {
    if(res < 0) res = 0;
    if(res > kOfferPriorityRange) res = kOfferPriorityRange;
    _offer_prio = (OFFER_PRIORITY)(kOfferPriorityNormal - res);
}

////////////////////////
// Exclude from dumps //
////////////////////////

extern HRESULT WerRegisterExcludedMemoryBlock(const void *address, DWORD size)
    __attribute((weak));

extern HRESULT WerRegisterMemoryBlock(const void *address, DWORD size);

static bool WerExcludeMemoryBlock(const void *address, DWORD size) {
    return &WerRegisterExcludedMemoryBlock 
        && (WerRegisterExcludedMemoryBlock(address, size) == S_OK);
}

/////////////////////
// POSIX interface //
/////////////////////

// TODO(dbg): introduce some variadic macro instead of 

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t off) {
    // *** IMPLEMENTATION NOTES (code generation) ***
    // MSDN: "To execute dynamically generated code, use VirtualAlloc to allocate
    //      memory and the VirtualProtect function to grant PAGE_EXECUTE access."
    // ibid: "call to FlushInstructionCache once the code has been set in place."
    // [GCC: void __builtin___clear_cache(char* begin, char* end)]

    _MEMMAP_LOG("Protection: %x", prot);
    if(_mmap_strict_policy && (prot & ~PROT_MASK)) {
        return errno = EINVAL, MAP_FAILED; // unknown protection flags
    } else {
        prot &= PROT_MASK;
    }
    DWORD protection = kProtectionTranslationLUT[prot];
    DWORD vm_request = MEM_RESERVE | MEM_COMMIT;
    const bool large_pages = flags & MAP_HUGETLB;
    const long page_size = large_pages \
         ? vm_request |= MEM_LARGE_PAGES, gethugepagesize() \
         : _page_size;
    // NOTE: https://learn.microsoft.com/en-us/windows/win32/memory/large-page-support
    //       (particularly, see the part about AdjustTokenPrivileges)

    if(((off * _mmap_strict_policy) | (uintptr_t)addr) % page_size) {
        return errno = EINVAL, MAP_FAILED; // enforce page boundary
    }
    // length will be rounded up later -- there is a file view padding to incorporate

    const bool is_file_backed = !(flags & MAP_ANONYMOUS);
    if(is_file_backed) {
        _MEMMAP_LOG("Flags: %x", flags);
        if(!(flags & MAP_SHARED) == !(flags & MAP_PRIVATE)) {
            return errno = EINVAL, MAP_FAILED;
        }
        const bool copy_on_write = (flags & MAP_PRIVATE) && (prot & PROT_WRITE);
        if(copy_on_write) protection <<= 1; // *_READWRITE becomes *_WRITECOPY

        // ... handtracking here (unless emergency mode is on)

        HANDLE hfile = _curFd2HandleImpl(fd);
        // The handle CAN be INVALID_HANDLE_VALUE. In this case, Windows creates
        // a mapping backed by the system page file. However, this behavior is
        // not expected in POSIX API and we simply report an error and quit.
        if(hfile == INVALID_HANDLE_VALUE) {
            _MEMMAP_LOG("invalid hfile GetLastError()=%lx", GetLastError());
            return errno = EBADF, MAP_FAILED;
        }
        SECURITY_ATTRIBUTES sa;
        sa.lpSecurityDescriptor = nullptr;
        sa.bInheritHandle = true; // TODO review handle inheritance throughout the API
        DWORD file_prot = protection;
        // if(large_pages) fileprot |= SEC_LARGE_PAGES; // only for the swap file
        if((prot & PROT_EXEC) && _mmap_apply_executable_image_sections) {
            // SEC_IMAGE is not a prerequisite for mapping an executable file.
            // Instead it tells the OS that memory protection values must follow
            // the binary ("objdump -h") layout of the file being mapped.
            // There is no equivalent flag in POSIX API; we enable it by a policy.
            file_prot |= SEC_IMAGE;
        }
        _MEMMAP_LOG("CreateFileMappingW(%p, %cinh, 0x%lx, whole file, no name)",
                     hfile, sa.bInheritHandle?'+':'-', file_prot);
        HANDLE h_map = CreateFileMappingW(hfile, &sa, file_prot, 0, 0, nullptr /*name*/);
        // the name won't be null for shm_open

        if(h_map == INVALID_HANDLE_VALUE) {
            /* FIXME parse GetLastError() and translate to relevant BSD/Linux errno! */
            _MEMMAP_LOG("invalid h_map GetLastError()=%lx", GetLastError());
            return errno = EACCES, MAP_FAILED;
        }

        const bool existing_mapping = GetLastError() == ERROR_ALREADY_EXISTS; // atomic

        off_t allocgran = get_allocation_granularity();
        off_t fvpadding = (off % allocgran);
        off_t fv_offset = off - fvpadding;
        off_t fv_length = length + fvpadding;

        // NOTE: https://stackoverflow.com/questions/55018806/copy-on-write-file-mapping-on-windows
        // also: in MinGW FILE_MAP_ALL_ACCESS includes FILE_MAP_EXECUTE, which is contrary to MSDN.
        const bool share_changes = (flags & MAP_SHARED) && (prot & PROT_WRITE);
        DWORD fv_access = copy_on_write ? FILE_MAP_COPY :
                          share_changes ? FILE_MAP_WRITE|FILE_MAP_READ : FILE_MAP_READ;
        if(prot & PROT_EXEC) {
            fv_access |= FILE_MAP_EXECUTE;
        }
        _MEMMAP_LOG("MapViewOfFile(%p, %lx, 0:%lx, %lx)", h_map, fv_access, (DWORD)fv_offset, (DWORD)fv_length);
        addr = MapViewOfFile(h_map, fv_access, 0, fv_offset, fv_length);
        if(!addr) {
            _MEMMAP_LOG("invalid mview GetLastError()=%lx", GetLastError());
            return errno = ENOMEM, MAP_FAILED; /* FIXME GetLastError() etc. */
        }

        addr = (void*)((uintptr_t)addr + fvpadding);

        // TODO further decorate for synchronization, execution etc. RESPECTING PAGE BOUNDARIES
        // TODO add handtracking for further `munmap` and `madvise` purposes

    } else {
        if(_mmap_strict_policy && fd != -1) {
            return errno = EINVAL, MAP_FAILED;
        }

        // length is not checked, but silently rounded up:
        length += page_size - 1; length -= length % page_size;

        _MEMMAP_LOG("VirtualAlloc(%p, %lx, %lx, %lx)", addr, (DWORD)length, vm_request, protection);
        addr = VirtualAlloc(addr, length, vm_request, protection);
        if(!addr) {
            errno = (GetLastError() == ERROR_INVALID_ADDRESS) ? EINVAL : ENOMEM;
            return MAP_FAILED;
        }
    }

    assert(addr); // we quit earlier in all error cases

    // now adorn the newlywed memory in special modes independent of file mapping

    if(flags & MAP_CONCEAL) {
        WerExcludeMemoryBlock(addr, length);
    }

    return addr;
}

int munmap(void* addr, size_t length) {
    // *** IMPLEMENTATION NOTES (deallocation) ***
    // We want `munmap` to be a general purpose semantic equivalent of its POSIX counterpart.
    // This means that we want it to unmap not only pages that have been allocated with `mmap`
    /// (which we know about) but also pages that haven't (which we don't).
    // Therefore we can, generally, only rely on a runtime examination of the page state.

    // NOTE: it is not possible to poke a hole in a FileView.
    // When an entire file view is closed, we UnmapViewOfFile(view);
    // when all mappings of a file are closed, we CloseHandle(map).
    // MSDN: "These functions can be called in any order."
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

    if(_mmap_strict_policy) {
        if(!(flags & MS_SYNC) == !(flags & MS_ASYNC)) {
            return errno = EINVAL, -1;
        }
    }

    if(RoundDownFailFast(addr, length)) return -1;

    _MEMMAP_LOG("FlushViewOfFile(%p, %lx)", addr, (DWORD)length);
    return FlushViewOfFile(addr, length) ? 0 : (errno = ENOMEM, -1);
}

/**
 * NOTE(MSDN): "offering and reclaiming virtual memory is similar to using the MEM_RESET[_UNDO]...
 * except that OfferVirtualMemory removes the memory from the process working set and restricts
 * access to the offered pages until they are reclaimed."
 * Therefore {Offer|Reclaim|Discard}VirtualMemory aren't strict equivalents of `madvise` (^^^).
 * Also, `MADV_FREE` has the "last-moment writes" semantic that isn't fully supported by Offer.
 * HOWEVER: using VirtualAlloc requires knowing the protection flags. We can run a VirtualQuery
 * but it would be fragile and non-atomic. Therefore, {Offer|Reclaim}VirtualMemory().
 */
int madvise(void* addr, size_t length, int advice) {
    if(RoundDownFailFast(addr, length) || RoundUpFailFast(length)) return -1;

    switch(advice){
        case MADV_DONTNEED:
            return OfferVirtualMemory(addr, length, _offer_prio) == ERROR_SUCCESS ? 0 : (errno = ENOMEM, -1);
        case MADV_WILLNEED:
            return ReclaimVirtualMemory(addr, length) == ERROR_SUCCESS ? 0 : (errno = ENOMEM, -1);
        case MADV_DONTDUMP:
            return WerExcludeMemoryBlock(addr, length) ? 0 : (errno = EAGAIN, -1);
        case MADV_DODUMP:
            return (WerRegisterMemoryBlock(addr, length) == S_OK) ? 0: (errno = EAGAIN, -1);
        default:
            return _mmap_strict_policy ? (errno = EINVAL, -1) : 0;
    }
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

// `mincore` is defined in mem.cpp

} // extern "C"
