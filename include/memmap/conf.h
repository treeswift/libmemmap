#ifndef _MEMMAP_CONF_H_
#define _MEMMAP_CONF_H_

#include <windows.h>

/* Tuning the Windows implementation of sys/mman.h API */

#ifdef __cplusplus

#include <functional>
#include <string>

/* C++ interfaces */

namespace mem {
namespace map {

} // namespace map

namespace shm {

/**
 * Directory to host files backing interprocess shared memory.
 * It is advised to either assign the value once at program startup
 * or use the default value (%TEMP%, or %TMP% if %TEMP% not defined).
 * `SetShmDir` returns `true` on success and `false` on failure.
 * Forward slashes are converted to backslashes.
 * 
 * MOREINFO remove this configuration section and use the page file instead?
 *          Why do Windows-ported shm implementations use temp folders then?
 */
bool SetShmDir(const std::string& path);
std::string DefShmDir();
std::string TmpShmDir();
std::string ShmDir();

} // namespace shm
} // namespace mem

/* __BEGIN_DECLS */
extern "C" {
#endif

/**
 * As there is no obvious way to determine `fd` access mode from itself or the
 * corresponding file HANDLE, and there is at most one FileMapping object per
 * file handle (and, therefore, per file descriptor), the following modes have
 * been suggested to infer the "write" and "access" bits:
 *
 * *_eager => assume maximum access;
 * *_probe => try grabbing as much as possible;
 * *_asreq => request as much access as the specific mapping requests.
 *
 * This is, of course, a hackaround. A better solution would rather duplicate
 * and/or reopen file handles when an incompatible access mode is required.
 * However, in practice, this is mostly needed to support executable files
 * (that contain code, readonly data and modifiable "initial" data together),
 * and that case is covered by `set_mmap_apply_executable_image_sections()`.
 * 
 * Therefore, this API should be considered unstable and *_asreq assumed to
 * be the current hardcoded behavior; but we will monitor real-life use cases.
 */
enum fd_access_inference_policy
{
    fd_access_inference_policy__eager = 0,
    fd_access_inference_policy__probe,
    fd_access_inference_policy__asreq,
};

void set_exec_bit_inference_policy(enum fd_access_inference_policy policy);
void set_write_bit_inference_policy(enum fd_access_inference_policy policy);

/**
 * Various boolean flags determining strict vs. relaxed implementation policies.
 */

/**
 * strict => require that fd==-1 if flags contains MAP_ANONYMOUS
 * !strict => ignore fd if an anonymous (i.e. private) mapping is requested
 */
void set_mmap_strict_policy(int strict);

/**
 * parse_coff => ignore `prot` flags and map the file the way a dynamic linker would
 *              (but without actually linking it dynamically into the symbol space);
 * !parse_coff => apply `prot` flags verbatim (default POSIX API behavior).
 */
void set_mmap_apply_executable_image_sections(int parse_coff);

/**
 * Pass nonzero to decommit memory affected by MADV_DONTNEED:
 * decommit => subsequent accesses cause a segfault
 * !decommit => subsequent read accesses "see" undefined contents
 *              (affected memory pages may need to be repopulated)
 * The default policy is !decommit. If decommit is enabled, the next option applies:
 */
void set_madvise_dontneed_decommits(int decommit);

/**
 * When memory is offered back to the OS with MADV_DONTNEED, resoluteness indicates
 * how serious the application is about the offer. 0 means "you can take it if you
 * really want"; 3 is the yiddishe mama level. The default is 2.
 */
void set_madvise_offer_resoluteness(int res);

/**
 * strict => ENOMEM if `mincore` range exceeds memory available to applications;
 *           ENOMEM if `mincore` range contains logically unmapped memory;
 *           EINVAL if `mincore` range start address is not a page boundary.
 * !strict => simply return 0 for the affected pages, but no `errno` value.
 */
void set_mincore_strict_policy(int strict);

/**
 * Directory to host memory sharing files (see long comment above).
 * `set_shared_memory_dir` returns 0 on success and -1 on failure.
 *  Possible `errno` values include ENOENT, ENOTDIR and EACCES.
 */
int set_shared_memory_dir(const char* path);
const char* def_shared_memory_dir();
const char* tmp_shared_memory_dir();
const char* get_shared_memory_dir();

/**
 * Emergency evacuation. Once this call is issued (e.g. by a crash handler/memory dump routine),
 * it is assumed that the heap and all previously allocated data might be in inconsistent state.
 * All internal bookkeeping is thence stopped, as no graceful termination is expected to follow.
 * `mmap` is the primary API call expected to respect this mode. `mincore` switches to no-alloc,
 * more CPU-intensive algorithms. `munmap` should be left to bacteria and scavengers.
 */
void emergency_mode_assume_unreliable_heap();

/* __END_DECLS */
#ifdef __cplusplus
}
#endif


#endif /* _MEMMAP_CONF_H_ */