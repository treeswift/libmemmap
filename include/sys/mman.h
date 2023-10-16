/**
 * This file has no copyright assigned and is placed in the public domain.
 * This file is part of the libmemmap compatibility library:
 *   https://github.com/treeswift/libmemmap
 * No warranty is given; refer to the LICENSE file in the project root.
 */

#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include <stddef.h>
#include <sys/types.h> /* mode_t, off_t */

#ifndef _OFF_T_DEFINED
#include <stdint.h>
#ifdef _WIN64
typedef int64_t off_t
#else
typedef int32_t off_t
#endif
#endif

#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4
#define PROT_MASK   0x7

#define PROT_DATA (PROT_READ | PROT_WRITE)
#define PROT_CODE (PROT_READ | PROT_EXEC)
#define PROT_JITC (PROT_CODE | PROT_WRITE)
/* PROT_JITC is equivalent to PROT_MASK */
/* PROT_WRITE | PROT_EXEC is impractical */

#define MAP_SHARED  0x1 /* "sync"able to the medium; PAGE_* mode depends on flags */
#define MAP_PRIVATE 0x2 /* copy-on-write: PAGE_WRITECOPY or PAGE_EXECUTE_WRITECOPY */
#define MAP_SHARED_VALIDATE MAP_SHARED /* unsupported; aliasing for compatibility */

#define MAP_FIXED   0x10 /* specific address request, freeing overlapping mappings */
#define __MAP_NOREPLACE 0x800 /* specific address request if available, or EEXIST */
#define MAP_FIXED_NOREPLACE __MAP_NOREPLACE /* TODO (define logic)*/

#define MAP_ANON    0x1000 /* no backing file */
#define MAP_ANONYMOUS MAP_ANON

#define __MAP_NOFAULT 0x2000 /* allocate from live RAM; unsupported and unlikely */

#define MAP_STACK   0x4000 /* stack segment. per documentation, may be (and is) ignored. */
#define MAP_GROWSDOWN MAP_STACK

/* BSD extensions */
#define MAP_CONCEAL 0x8000 /* omit from core dumps (Linux: MADV_DONTDUMP) -- ignored atm */

/* Linux extensions */
#define MAP_POPULATE  0x8000 /* TODO "commit and touch" */
#define MAP_NONBLOCK 0x10000 /* ignored */
#define MAP_HUGETLB  0x40000 /* translates to MEM_LARGE_PAGES */
#define MAP_SYNC     0x80000 /* persistent write guarantee; unsupported */
#define MAP_UNINITIALIZED 0x4000000 /* don't zero out contents; ignored */

#define MAP_FLAGMASK ~0 /* all flag bits are valid, but some are reserved for future use */

#define MAP_FAILED	((void*) -1)

/**
 * `msync` flags. All of them are ignored, but strict mode makes sure that
 *  exactly one of MS_SYNC and MS_ASYNC is passed.
 */
#define MS_SYNC       0x1
#define MS_INVALIDATE 0x2
#define MS_ASYNC      0x4

#define MADV_DONTNEED 0x1
#define MADV_WILLNEED 0x2

#define MADV_DONTDUMP 0x10
#define MADV_DODUMP   0x11

#define MLOCK_ONFAULT 0x10

#define MCL_CURRENT 0x1
#define MCL_FUTURE  0x2 /* unsupported */
#define MCL_ONFAULT 0x4 /* unsupported */

/* __BEGIN_DECLS */
#ifdef __cplusplus
extern "C" {
#endif

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t off);
int munmap(void* addr,  size_t length);

int mprotect(void* addr, size_t length, int flags);
int msync(void* addr, size_t length, int flags);
int madvise(void* addr, size_t length, int advice);
#define posix_madvise madvise

int mlock(const void* addr, size_t length);
int mlock2(const void* addr, size_t length, int flags); /* Linux specific; our impl identical to `mlock` */
int munlock(const void* addr, size_t length);

/* Not atomic on Windows */
int mlockall(int flags);
/* Not atomic on Windows */
int munlockall();

int mincore(void* start, size_t length, unsigned char* status);

int shm_open(const char* filename, int open_flag, mode_t mode);
int shm_unlink(const char* filename);

/* __END_DECLS */
#ifdef __cplusplus
}
#endif

#endif /* _SYS_MMAN_N_ */