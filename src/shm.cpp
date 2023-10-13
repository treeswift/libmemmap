#include "sys/mman.h"
#include "memmap/conf.h"

#include <string>

namespace {

using namespace mem;
using namespace shm;


std::string _shm_dir;
}

namespace mem {
namespace shm {

bool SetShmDir(const std::string& path) {
    //
}

std::string DefShmDir() {
    //
}

std::string ShmDir() {
    //
}

} // namespace shm
} // namespace mem

extern "C" {

/**
 * Directory to host memory sharing files (see long comment above).
 * `set_shared_memory_dir` returns 0 on success and -1 on failure.
 *  Possible `errno` values include ENOENT, ENOTDIR and EACCES.
 */
int set_shared_memory_dir(const char* path);
const char* get_shared_memory_dir();

int shm_open(const char* filename, int open_flag, mode_t mode) {
    return -1;
}

int shm_unlink(const char* filename) {
    return -1;
}

} // extern "C"
