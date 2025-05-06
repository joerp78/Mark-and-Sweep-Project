#ifndef __GC_H
#define __GC_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <list>

using namespace std;
class Heap;

class GarbageCollector {
    public:
        /**
         * Metadata header stored with each allocated block.
         * - `size` is the size of the user's allocated space (excluding the header).
         * - `marked` indicates if the block was visited during the mark phase.
         */
        typedef struct allocation {
            size_t size;
            bool marked;
        } allocation;

        /**
         * Allocates memory on the given heap and registers the allocation.
         * @param size Number of bytes to allocate.
         * @param heap Pointer to the heap to allocate from.
         * @return Pointer to the allocated memory (or NULL on failure).
         */
        void *malloc(size_t size, Heap *heap);

        /**
         * Runs mark-and-sweep garbage collection.
         * Frees any unreachable objects from the heap.
         * @param heap The heap to operate on.
         */
        list<void*> ms_collect(Heap *heap);

        /**
         * Runs reference-counting garbage collection.
         * Frees objects whose reference count has dropped to zero.
         * @param heap The heap to operate on.
         */
        list<void*> rc_collect(Heap *heap);

        /**
         * Adds a pointer to the root set to simulate a live reference.
         * Increments the reference count of the object (if applicable).
         * @param ptr Pointer to the object to track.
         * @return 0 if successful, -1 if the pointer was not found.
         */
        void add_reference(void *ptr);

        /**
         * Adds a nested reference from one object to another, then increments
         * the referenced object's reference count. Does NOT add the reference
         * to the root set as this is not a root reference. Allows for cyclic
         * referencing.
         * @param src Pointer to the memory block that's being modified
         * @param dest Pointer to the memory block that's being referenced
         * @return 0 if successful, -1 on failure.
         */
        int add_nested_reference(void *src, void *dest);

        /**
         * Removes a pointer from the root set.
         * Decrements the reference count of the object (if applicable).
         * @param ptr Pointer to remove.
         * @return 0 if successful, -1 if the pointer was not found.
         */
        void delete_reference(void *ptr);

    protected:
        /**
         * Performs the mark phase by traversing the root set and marking reachable objects.
         */
        void mark();

        /**
         * Performs the sweep phase by freeing all unmarked objects in the allocations map.
         * @param heap The heap to free memory from.
         */
        list<void*> sweep(Heap *heap);

        /**
         * Recursively walks the contents of a memory block, marking any reachable pointers found.
         * @param ptr Pointer to the start of a block to walk.
         */
        void walk_block(void *ptr);

        /**
         * Used internally by both reference counting (`rc_collect`) and mark-and-sweep (`ms_collect`)
         * garbage collection algorithms to reclaim unreachable memory.
         *
         * @param ptr  Pointer to the block to be freed.
         * @param heap Pointer to the Heap structure managing memory allocation.
         */
        void GC_free(void* ptr, Heap* heap);

        /**
         * Maps allocated heap pointers to their metadata.
         * This is the internal structure used to manage all tracked heap allocations.
         * Only modified during malloc() and sweep().
         */
        typedef map<void*, allocation*> PointerMap;
        PointerMap allocations;   // Tracks all active heap allocations.
        
        /**
         * Simulated root references (acting like stack/global pointers).
         * Any pointer here is treated as a live root for the mark phase.
         * Can be modified using add_reference() and delete_reference().
         */
        multiset<void*> root_set;

        /**
         * Reference counts for each allocated object.
         * Used by the reference counting garbage collection algorithm.
         */
        map<void*, int> reference_count;

};

#endif
