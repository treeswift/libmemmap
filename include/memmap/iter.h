#ifndef _MEMMAP_ITER_H_
#define _MEMMAP_ITER_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>
#include <functional>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct MEMMAP_RANGE {
    void* lower;
    void* upper;
} MEMMAP_RANGE;

#define MEMMAP_RANGE_SIZE(range) ((uintptr_t)range.upper - (uintptr_t)range.lower)

typedef void(*memmap_range_visitor)(const MEMORY_BASIC_INFORMATION*, const MEMMAP_RANGE*);

typedef bool(*memmap_range_predicate)(const MEMORY_BASIC_INFORMATION*);

void memmap_traverse_addresses_from_to(void* lower, void* upper, memmap_range_visitor visitor, memmap_range_predicate predicate);

void memmap_traverse_addresses_from_for(void* base, size_t size, memmap_range_visitor visitor, memmap_range_predicate predicate);

void memmap_traverse_committed_from_to(void* lower, void* upper, memmap_range_visitor visitor);

void memmap_traverse_committed_from_for(void* base, size_t size, memmap_range_visitor visitor);

void memmap_traverse_all_process_memory(memmap_range_visitor visitor, memmap_range_predicate predicate);

#ifdef __cplusplus
} // extern "C"

namespace mem
{

using RangeVisitor = std::function<void(const MEMORY_BASIC_INFORMATION&, const MEMMAP_RANGE&)>;

using RangePredicate = std::function<bool(const MEMORY_BASIC_INFORMATION&)>;

MEMMAP_RANGE Range(void* base, std::size_t size);

inline bool Addressable(const MEMORY_BASIC_INFORMATION&) { return true; }

inline bool Reserved(const MEMORY_BASIC_INFORMATION& mbi) { return MEM_FREE != mbi.State; }

inline bool Committed(const MEMORY_BASIC_INFORMATION& mbi) { return MEM_COMMIT == mbi.State; }

void Traverse(const MEMMAP_RANGE& range, RangeVisitor visitor, RangePredicate predicate = &Committed);

void TraverseAllProcessMemory(RangeVisitor visitor, RangePredicate predicate = &Committed);

} // namespace mem

#endif // __cplusplus


#endif /* _MEMMAP_ITER_H_ */