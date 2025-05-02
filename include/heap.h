#ifndef __HEAP_H
#define __HEAP_H
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <iostream>
#include <string>
#include <map>
#include <gc.h>

using namespace std;
#define HEAP_SIZE 4096 // 4KB

class Heap {
    public:
        // Represents a free memory block in the heap
        typedef struct __node_t {
            size_t size;       // Size of the free block
            struct __node_t *next; // Pointer to the next free block
        } node_t;
    
        node_t *head; // Pointer to the start of the free list
        node_t *tail; // Pointer to the end sentinel of the heap
    
        // Constructor
        Heap() {
            head = NULL;
            tail = NULL;
        }
    
        /**
         * Initializes the heap if it hasn't been started yet and returns the head of the free list.
         * @return Pointer to the head node of the free list.
         */
        node_t *start();
    
        /**
         * Resets the heap by unmapping and reinitializing it.
         */
        void reset();
    
        /**
         * Returns the total amount of free memory currently available in the heap.
         * @return Size in bytes of available memory.
         */
        size_t available_memory();
    
        /**
         * Prints a visual representation of the free list, showing block sizes.
         */
        void print_free_list();
    
        /**
         * Allocates a block of memory from the heap.
         * @param size Number of bytes to allocate.
         * @return Pointer to the usable memory block or NULL if allocation fails.
         */
        void *my_malloc(size_t size);
    
        /**
         * Frees a previously allocated block and returns it to the free list.
         * @param allocated Pointer to the memory block to be freed.
         */
        void my_free(void *allocated);
    
        /**
         * Finds a free block large enough to hold `size` bytes.
         * @param size The size needed.
         * @param found Output: Pointer to the found block.
         * @param prev Output: Pointer to the previous block (used for list manipulation).
         */
        void find_free(size_t size, node_t **found, node_t **prev);
    
        /**
         * Splits a free block into an allocated portion and a smaller free block.
         * @param size Requested size of memory.
         * @param prev Pointer to the previous node in the free list.
         * @param free_block Pointer to the block to be split.
         * @param allocated Output: Pointer to the newly allocated block metadata.
         */
        void split(size_t size, node_t **prev, node_t **free_block,
                   GarbageCollector::allocation **allocated);
    
        /**
         * Inserts a freed block back into the free list and merges it with adjacent free blocks.
         * @param free_block Pointer to the block being freed.
         */
        void coalesce(node_t *free_block);
};

#endif