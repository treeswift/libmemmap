#ifndef _MEMMAP_PROC_H_
#define _MEMMAP_PROC_H_

/* process memory layout query */

#ifdef __cplusplus

/* C++ interfaces */
////////////////////

/* __BEGIN_DECLS */
extern "C" {
#endif

/* pure C interfaces */
///////////////////////

#ifndef _SC_PAGESIZE
#define _SC_PAGESIZE 0x9600
#endif

#ifndef _SC_PAGE_SIZE
#define _SC_PAGE_SIZE _SC_PAGESIZE
#endif

/**
 * Either a wrapper of runtime-provided `sysconf` or a replacement of it.
 * Provides the virtual memory allocation unit (page size).
 * NOTE: hardcoded PAGE_SIZE and PAGE_SHIFT are defined in <ddk/wdm.h>
 *      from the ReactOS DDK package as:
 * `#define PAGE_SHIFT  12`
 * `#define PAGE_SIZE   (1 << PAGE_SHIFT)`
 * The file itself, however, is severely broken as of the current MinGW.
 */
long memmap_sysconf(int name);

/* Ditto. */
int getpagesize();

/**
 * We could introduce constants representing allocation granilarity etc., but if
 * the caller knows what they mean, it can as well call GetSystemInfo() directly.
 */

/* __END_DECLS */
#ifdef __cplusplus
}
#endif

#endif /* _MEMMAP_PROC_H_ */