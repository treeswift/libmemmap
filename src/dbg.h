#ifndef _MEMMAP_SRC_DBG_H_
#define _MEMMAP_SRC_DBG_H_

#ifdef _MEMMAP_DEBUG
#define _MEMMAP_LOG(format, ...) { fprintf(_MEMMAP_DEBUG, format, ##__VA_ARGS__); fflush(_MEMMAP_DEBUG); }
#else
#define _MEMMAP_LOG(...)
#endif

#endif /* _MEMMAP_SRC_DBG_H_ */