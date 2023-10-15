#include "sys/mman.h"
#include "memmap/proc.h"

#include <windows.h>

#include <string>
#include <stdio.h>
#include <assert.h>

// VirtualQueryEx(HANDLE, ...) -> memory info of another process

/* We assume that the address space is 32-bit, which is true for our Windows RT target. */

// TODO range arithmetic
// TODO range iterator


/**
 * Print memory usage flags for a page or a range of pages.
 * Intel Software Guard Extensions (SGX) are not supported.
*/
std::string VisualizeProtection(DWORD prot) {
    std::string vis;
    vis.resize(3, ' ');
    std::sprintf(&vis[0], "%03lx", prot);
    vis.append(" (");
    vis.push_back(prot & 0x400 ? 'b' : '-');
    vis.push_back(prot & 0x200 ? 'n' : '-');
    vis.push_back(prot & 0x100 ? 'g' : '-');
    DWORD xflags = (prot & 0xf0) >> 4;
    switch(xflags) {
        case 1: vis += "--x"; break;
        case 2: vis += "r-x"; break;
        case 4: vis += "rwx"; break;
        case 8: vis += "rcx"; break;
        default:
            DWORD dflags = prot & 0xf; 
            switch(dflags) {
                case 0: vis += "   "; break;
                case 1: vis += "---"; break;
                case 2: vis += "r--"; break;
                case 4: vis += "rw-"; break;
                case 8: vis += "rc-"; break;
                default: vis.append("???");
            }
    }
    vis.append(")");
    return vis;
}

int main(int, char **) {
#ifdef PAGE_SIZE
    printf("Page size (static):\t%10ld bytes (0x%lx)\n", PAGE_SIZE, PAGE_SIZE);
#endif

    long page_size = memmap_sysconf(_SC_PAGESIZE);
    printf("Page size (dynamic):\t%10ld bytes (0x%lx)\n", page_size, page_size);
    long huge_page = gethugepagesize();
    printf("Large ('huge') page:\t%10ld bytes (0x%lx)\n", huge_page, huge_page);
    long allocgran = get_allocation_granularity();
    printf("Alloc-n granularity:\t%10ld bytes (0x%lx)\n", allocgran, allocgran);

    printf("\n baseaddr-uptoaddr   length req (bngrwx) -> now (bngrwx) type res\n");
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    // there is also dwAllocationGranularity (may be greater than dwPageSize)

    void* lower = si.lpMinimumApplicationAddress;
    void* upper = si.lpMaximumApplicationAddress;
    while((uintptr_t)lower < (uintptr_t)upper) {
        MEMORY_BASIC_INFORMATION mbi;
        VirtualQuery(lower, &mbi, sizeof(mbi));
        assert(mbi.BaseAddress == lower);
        void* alloc = mbi.AllocationBase;
        size_t size = mbi.RegionSize;
        DWORD genre = mbi.Type; // MEM_IMAGE, MEM_MAPPED or MEM_PRIVATE
        DWORD state = mbi.State; // MEM_COMMIT, MEM_RESERVE or MEM_FREE
        DWORD asreq = mbi.AllocationProtect; // flags within 0x7ff
        DWORD asnow = mbi.Protect;
        // PartitionId == ?
        (void) alloc;

        std::string vis_asreq;
        std::string vis_asnow = VisualizeProtection(asnow);
        if(asnow == asreq) {
            vis_asreq.resize(16, ' ');
        } else {
            vis_asreq = VisualizeProtection(asreq);
            vis_asreq += " -> ";
        }

        const char* vis_genre =
            (MEM_IMAGE == genre)
                ? "exec" :
            (MEM_MAPPED == genre)
                ? "file" :
            (MEM_PRIVATE == genre)
                ? "data" :
            "----";
        const char* vis_state =
            (MEM_COMMIT == state)
                ? "COM" :
            (MEM_RESERVE == state)
                ? "res" :
            (MEM_FREE == state)
                ? "---" :
            "???";

        lower = (void*)((uintptr_t)lower + size); // step next
        printf(" %p-%p %8x %s%s %s %s\n", mbi.BaseAddress, lower, size, vis_asreq.c_str(), vis_asnow.c_str(), vis_genre, vis_state);
    }

    return 0;
}
