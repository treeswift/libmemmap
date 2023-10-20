project('libmemmap', 'cpp',
    version : '0.0.1',
    default_options : ['warning_level=3'],
  )

# add_global_arguments('-D_WIN32_WINNT=_WIN32_WINNT_WIN8', language: 'cpp') # Win8+
# add_global_arguments('-D_WIN32_WINNT=0x0603', language: 'cpp') # Win10+


memmap = shared_library('memmap',
    files(
      'src/mem.cpp',
      'src/map.cpp',
      'src/shm.cpp',
    ),
    include_directories: ['include'],
    link_args: ['-lkernel32', '-lfatctl'],
    install: true,
  )

mmtest = executable('test-memdmp', 
    files('samples/memdump.cpp'),
    include_directories: ['include'],
    link_with: [memmap],
    install: true,
  )

mmtest = executable('test-memmap', 
    files('samples/memtest.cpp'),
    include_directories: ['include'],
    cpp_args: ['-fasync-exceptions'], # '-fseh-exceptions', 
    link_with: [memmap],
    install: true,
  )

install_headers(
    files('include/sys/mman.h'),
    subdir: 'sys',
)

install_headers(
    files('include/memmap/conf.h'),
    files('include/memmap/proc.h'),
    files('include/memmap/iter.h'),
    subdir: 'memmap',
)
