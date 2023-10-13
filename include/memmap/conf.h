#ifndef _MEMMAP_PROC_H_
#define _MEMMAP_PROC_H_

#include <windows.h>

/* Tuning the Windows implementation of sys/mman.h API */

#ifdef __cplusplus

#include <functional>
#include <string>

/* C++ interfaces */

namespace mem {
namespace map {

// normally it's HANDLE _get_osfhandle(int), assuming fd is a standard Windows CRT open().
// however, alternative compatibility libraries may have their own file descriptor tables.
// pure C equivalent (with function pointers): see [set_]handle_from_posix_fd_* API below.

using Fd2HANDLE = std::function<HANDLE(int)>;
void SetFd2Handle(Fd2HANDLE delegate);
Fd2HANDLE DefFd2Handle();

} // namespace map

namespace shm {

/**
 * Directory to host files backing interprocess shared memory.
 * It is advised to either assign the value once at program startup
 * or use the default value (%TEMP%, or %TMP% if %TEMP% not defined).
 * `SetShmDir` returns `true` on success and `false` on failure.
 * Forward slashes are converted to backslashes.
 */
bool SetShmDir(const std::string& path);
std::string DefShmDir();
std::string ShmDir();

} // namespace shm
} // namespace mem

/* __BEGIN_DECLS */
extern "C" {
#endif

/* pure C interfaces */

/* stateless (or "externally stateful") converter */
typedef HANDLE(*handle_from_posix_fd_func)(int fd);
int set_handle_from_posix_fd_func(handle_from_posix_fd_func func);

/* default converter. returns pointer to wrapped _get_osfhandle */
handle_from_posix_fd_func def_handle_from_posix_fd_func();

/* explicitly stateful converter. hint is an opaque table or library pointer */
typedef HANDLE(*handle_from_posix_fd_hook)(int fd, void* hint);
int set_handle_from_posix_fd_hook(handle_from_posix_fd_hook hook, void * hint);

/**
 * As there is no obvious way to determine `fd` access mode from itself or the
 * corresponding file HANDLE, and there is at most one FileMapping object 
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
 * Directory to host memory sharing files (see long comment above).
 * `set_shared_memory_dir` returns 0 on success and -1 on failure.
 *  Possible `errno` values include ENOENT, ENOTDIR and EACCES.
 */
int set_shared_memory_dir(const char* path);
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


#endif /* _MEMMAP_PROC_H_ */