#include <assert.h>
#include <gc.h>
#include <heap.h>
#include <iostream>

/**
 * Allocates memory from the heap and registers it with the garbage collector.
 *
 * @param size The number of bytes to allocate.
 * @param heap Pointer to the heap object used for allocation.
 * @return A pointer to the allocated memory block, or NULL if allocation fails.
 */
void* GarbageCollector::malloc(size_t size, Heap *heap) {
    void *ptr = heap->my_malloc(size);

    if (ptr) {
        allocations[ptr] = (allocation *)((char*)ptr - sizeof(allocation));
        add_reference(ptr);
    } else {
        return NULL;
    }

    return ptr;
}

/**
 * Recursively marks reachable memory blocks by scanning for pointers within the given block.
 *
 * @param ptr Pointer to the memory block to scan.
 */
void GarbageCollector::walk_block(void* ptr) {
    if (!ptr) return;

    allocation *alloc = (allocation *)(((char *)ptr) - sizeof(allocation));
    size_t size = alloc->size;

    if (alloc->marked) {
        return;
    }
    alloc->marked = true;

    uintptr_t* scan = reinterpret_cast<uintptr_t*>(ptr);
    uintptr_t* end = reinterpret_cast<uintptr_t*>(reinterpret_cast<char*>(ptr) + size);

    while (scan < end) {
        void* maybe_ptr = reinterpret_cast<void*>(*scan);
        if (allocations.find(maybe_ptr) != allocations.end()) {
            allocation* found_block = allocations[maybe_ptr];
            if (!found_block->marked) {
                walk_block(maybe_ptr);
            }
        }
        ++scan;
    }
}

/**
 * Initiates the mark phase of the garbage collection process.
 * Marks all reachable memory blocks starting from the root set.
 */
void GarbageCollector::mark() {

    // Clear all markings
    for (auto alloc = allocations.begin(); alloc != allocations.end(); alloc++) {
        alloc->second->marked = false;
    }

    // Traverse the root set to identify reachable objects
    for (void* root : root_set) {
        auto alloc = allocations.find(root);
        if (alloc != allocations.end()) {
            if (!alloc->second->marked) {
                walk_block(alloc->first); // Traverse the block's memory to identify additional references
            }
        }
    }
}

/**
 * Initiates the sweep phase of the garbage collection process.
 * Frees all memory blocks not marked as reachable.
 *
 * @param heap Pointer to the heap object used for deallocation.
 */
list<void*> GarbageCollector::sweep(Heap *heap) {
    list<void*> deleted;
    for (auto block = allocations.begin(); block != allocations.end(); ) {
        if (!block->second->marked) {
            void* dead = block->first;
            deleted.push_back(block->first);
            GC_free(dead, heap);
            block = allocations.begin();
        } else {
            ++block;
        }
    }

    // If nothing is left in allocations, reset heap structure
    if (allocations.empty()) {
        heap->reset();
    }

    return deleted;
}

/**
 * Adds a reference to the root set and increments the object's
 * reference count.
 * 
 * @param ptr Pointer to add to the root set.
 */
void GarbageCollector::add_reference(void *ptr) {
    //cout << "Adding reference: " << ptr << " to root_set" << endl;
    root_set.insert(ptr);
    reference_count[ptr] += 1;
}

/**
 * Adds a nested reference from one object to another, then increments
 * the referenced object's reference count. Does NOT add the reference
 * to the root set as this is not a root reference. Allows for cyclic
 * referencing.
 * 
 * @param src Pointer to the memory block that's being modified
 * @param dest Pointer to the memory block that's being referenced
 * @return 0 if successful, -1 on failure.
 */
int GarbageCollector::add_nested_reference(void *src, void *dest) {
    allocation *alloc = (allocation *)((char*)src - sizeof(allocation));
    if (alloc->size >= sizeof(void *)) {
        ((void **)src)[0] = dest;
        reference_count[dest]++;
    } else {
        return -1;
    }
    return 0;
}

/**
 * Deletes a reference from the root set.
 * 
 * @param ptr Pointer that is being deleted.
 */
void GarbageCollector::delete_reference(void *ptr) {
    //cout << "Deleting reference: " << ptr << " from root_set" << endl;
    /*
    int erased = root_set.erase(ptr);

    // Check that a reference was actually deleted
    if (erased > 0) {
        reference_count[ptr]--;
    }
    */
   auto it = root_set.find(ptr);
   if (it == root_set.end()) return;
   root_set.erase(it);

   auto rc_it = reference_count.find(ptr);
   if (rc_it != reference_count.end()) {
       rc_it->second--;
       if (rc_it->second < 0)
           rc_it->second = 0;
   }
}

/**
 * Executes the mark and sweep garbage collection algorithm.
 * 
 * @param heap Pointer to the heap to be garbage collected.
 */
list<void*> GarbageCollector::ms_collect(Heap *heap) {
    mark();
    return sweep(heap);
}

/**
 * Executes the reference counting garbage collection algorithm.
 * 
 * @param heap Pointer to the heap to be garbage collected.
 */
list<void*> GarbageCollector::rc_collect(Heap *heap) {
    list<void*> deleted;
    for (auto block = reference_count.begin(); block != reference_count.end(); ) {
        if (block->second <= 0) {
            void* dead = block->first;
            deleted.push_back(block->first);
            GC_free(dead, heap);
            block = reference_count.begin();
        } else {
            ++block;
        }
    }
    return deleted;
}

/**
 * Frees a block from the heap, removing its reference in the allocation and reference count lists
 * 
 * @param ptr pointer to a block to free and be removed from the allocation and reference_count list 
 * @param heap Pointer to the heap to be garbage collected.
 */

void GarbageCollector::GC_free(void * ptr, Heap* heap){
    heap->my_free(ptr);
    allocations.erase(ptr);
    reference_count.erase(ptr);
    root_set.erase(ptr);
}