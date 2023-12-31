#include "sys/mman.h"
#include "memmap/proc.h"
#include "memmap/iter.h"

#include <windows.h>
#include <psapi.h> /* GetMappedFileName */

#include <string>
#include <functional>
#include <stdio.h>

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

    printf("\n baseaddr-uptoaddr   length req (bngrwx) -> now (bngrwx) type res name\n");
    char map_name[MAX_PATH + 1];
    constexpr bool kTrimPath = true; // change to false to show full WinNT path

    mem::TraverseAllProcessMemory([&](const MEMORY_BASIC_INFORMATION& mbi, const MEMMAP_RANGE& range) {
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

        const char* vis_state =
            (MEM_COMMIT == state)
                ? "COM" :
            (MEM_RESERVE == state)
                ? "res" :
            (MEM_FREE == state)
                ? "---" :
            "???";

        // 'A' because locales s*ck. Speak whatever language you like, but system files should be named in ASCII.
        std::size_t str_length = GetMappedFileNameA(GetCurrentProcess(), range.lower, map_name, MAX_PATH);
        map_name[str_length] = '\0';
        char* rchr = strrchr(map_name, '\\');
        char* filename = (kTrimPath && rchr) ? rchr+1 : map_name;

        const char* vis_genre =
            (MEM_IMAGE == genre)
                ? "exec" :
            (MEM_MAPPED == genre)
                ? (*filename ? "file" : "swap") :
            (MEM_PRIVATE == genre)
                ? "data" :
            "----";

        printf(" %p-%p %8x %s%s %s %s %s\n", range.lower, range.upper, MEMMAP_RANGE_SIZE(range),
                vis_asreq.c_str(), vis_asnow.c_str(), vis_genre, vis_state, filename);
    }, &mem::Reserved); // show all "logical" application memory ranges, not only resident memory

    return 0;
}
