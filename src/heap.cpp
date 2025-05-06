#include <stdlib.h>
#include <sys/mman.h>
#include <iostream>
#include <string>
#include <heap.h>
#include <gc.h>
#include <assert.h>

using namespace std;
using Allocation = GarbageCollector::allocation;

Heap::node_t *head = NULL;
Heap::node_t *tail = NULL;

/**
 * Initializes the heap if it has not been created yet.
 * Uses `mmap` to allocate a contiguous block of memory and sets up the free list.
 *
 * @return Pointer to the head node of the free list.
 */
Heap::node_t* Heap::start() {
    if (head == NULL) {
        head = (node_t *)mmap(NULL, HEAP_SIZE + sizeof(node_t),
                              PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        tail = (node_t *)((char *)head + HEAP_SIZE);
        head->size = HEAP_SIZE - sizeof(node_t);
        head->next = tail;
        tail->size = 0;
        tail->next = NULL;
    }
    return head;
}

/**
 * Resets the heap by unmapping the current region and reinitializing it.
 */
void Heap::reset() {
    if (head != NULL) {
        munmap(head, HEAP_SIZE + sizeof(node_t));
        head = NULL;
        Heap::start();
    }
}

/**
 * Calculates and returns the total amount of available (free) memory in the heap.
 *
 * @return The number of free bytes in the heap.
 */
size_t Heap::available_memory() {
    size_t n = 0;
    node_t *p = Heap::start();
    while (p != tail) {
        n += p->size;
        p = p->next;
    }
    return n;
}

/**
 * Finds the first free block large enough to satisfy the requested size.
 *
 * @param size The number of bytes needed.
 * @param found Output parameter that will point to the suitable free block.
 * @param prev Output parameter that will point to the block preceding `found`.
 */
void Heap::find_free(size_t size, node_t **found, node_t **prev) {
    node_t *curr = Heap::start();
    *found = NULL;
    *prev = NULL;

    while (curr != tail) {
        if (curr->size >= size) {
            *found = curr;
            return;
        } else {
            *prev = curr;
            curr = curr->next;
        }
    }
}

/**
 * Splits a free block into an allocated block and a smaller free block.
 *
 * @param size The size of the memory to allocate (excluding metadata).
 * @param prev Pointer to the previous node in the free list.
 * @param free_block Pointer to the free block to be split.
 * @param allocated Output parameter pointing to the newly allocated block (with metadata).
 */
void Heap::split(size_t size, node_t **prev, node_t **free_block,
                    GarbageCollector::allocation **allocated) {
    assert(*free_block != NULL);

    node_t *temp = *free_block;
    size_t actual_size = size + sizeof(Allocation);
    size_t original_size = temp->size;

    *free_block = (node_t *)((char *)temp + actual_size);
    (*free_block)->size = original_size - actual_size;
    (*free_block)->next = temp->next;

    if (*prev == NULL) {
        head = *free_block;
    } else {
        (*prev)->next = *free_block;
    }

    *allocated = (Allocation *)temp;
    (*allocated)->size = size;
    (*allocated)->marked = false;
}

/**
 * Merges a freed block back into the free list and coalesces adjacent free blocks.
 *
 * @param free_block Pointer to the block being freed.
 */
void Heap::coalesce(node_t *free_block) {
    node_t *next = head;
    node_t *prev = NULL;

    while (next && next < free_block) {
        prev = next;
        next = next->next;
    }

    free_block->next = next;

    if (prev) {
        prev->next = free_block;
    } else {
        head = free_block;
    }

    if (next &&
        (char *)free_block + free_block->size + sizeof(node_t) == (char *)next) {
        free_block->size += next->size + sizeof(node_t);
        free_block->next = next->next;
    }

    if (prev &&
        (char *)prev + prev->size + sizeof(node_t) == (char *)free_block) {
        prev->size += free_block->size + sizeof(node_t);
        prev->next = free_block->next;
    }
}

/**
 * Allocates memory from the heap, splitting a free block if possible.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated memory block, or NULL if no suitable block is found.
 */
void *Heap::my_malloc(size_t size) {
    node_t *previous = NULL;
    node_t *free_block = NULL;
    Allocation *allocated = NULL;

    Heap::find_free(size, &free_block, &previous);

    if (free_block == NULL) {
        return NULL;
    }

    Heap::split(size, &previous, &free_block, &allocated);
    return (void *)((char *)allocated + sizeof(Allocation));
}

/**
 * Frees a previously allocated block and coalesces it into the free list.
 *
 * @param allocated Pointer to the memory block to free (as returned by my_malloc).
 */
void Heap::my_free(void *allocated) {
    Allocation *header = (Allocation *)((char *)allocated - sizeof(Allocation));
    node_t *free_node = (node_t *)header;
    free_node->size = header->size;
    Heap::coalesce(free_node);
}

/**
 * Prints the current free list, showing the sizes of free blocks.
 */
void Heap::print_free_list() {
    node_t *p = Heap::start();
    while (p != tail) {
        printf("Free(%zd)", p->size);
        p = p->next;
        if (p != NULL) {
            printf("->");
        }
    }
    printf("\n");
}
