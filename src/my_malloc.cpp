#include <assert.h>
#include <my_malloc.h>

// A pointer to the head of the free list.
node_t *head = NULL;
node_t *tail = NULL;

// The heap function returns the head pointer to the free list. If the heap
// has not been allocated yet (head is NULL) it will use mmap to allocate
// a page of memory from the OS and initialize the first free node.
node_t *heap() {
  if (head == NULL) {
    // This allocates the heap and initializes the head node.
    head =
        (node_t *)mmap(NULL, HEAP_SIZE + sizeof(node_t), PROT_READ | PROT_WRITE,
                       MAP_ANON | MAP_PRIVATE, -1, 0);
    tail = (node_t *)((char *)head +
                      HEAP_SIZE);  // Set the tail to the end of the heap
    head->size =
        HEAP_SIZE - sizeof(node_t);  // THe size does not include the header
    head->next = tail;
    tail->size = 0;
    tail->next = NULL;
  }

  return head;
}

// Reallocates the heap.
void reset_heap() {
  if (head != NULL) {
    munmap(head, HEAP_SIZE);
    head = NULL;
    heap();
  }
}

// Returns a pointer to the head of the free list.
node_t *free_list() { return head; }

// Calculates the amount of free memory available in the heap.
size_t available_memory() {
  size_t n = 0;
  node_t *p = heap();
  while (p != tail) {
    n += p->size;
    p = p->next;
  }
  return n;
}

// Returns the number of nodes on the free list.
int number_of_free_nodes() {
  int count = 0;
  node_t *p = heap();
  while (p != tail) {
    count++;
    p = p->next;
  }
  return count;
}

// Prints the free list. Useful for debugging purposes.
void print_free_list() {
  node_t *p = heap();
  while (p != tail) {
    printf("Free(%zd)", p->size);
    p = p->next;
    if (p != NULL) {
      printf("->");
    }
  }
  printf("\n");
}

// Finds a node on the free list that has enough available memory to
// allocate to a calling program. This function uses the "first-fit"
// algorithm to locate a free node.
//
// PARAMETERS:
// size - the number of bytes requested to allocate
//
// RETURNS:
// found - the node found on the free list with enough memory to allocate
// previous - the previous node to the found node
//
void find_free(size_t size, node_t **found, node_t **previous) {
  node_t *iter_node = heap();  // Get the head of the free list, initialize the
                               // heap if necessary
  // TODO
  node_t *prev = NULL;
  while(iter_node != tail){
    if(iter_node->size >= size){
      *found = iter_node;
      *previous = prev;
      return;
    }
    prev = iter_node;
    iter_node = iter_node->next;
  }
  *found = NULL;
  *previous= prev;

  return;
}

// Splits a found free node to accommodate an allocation request.
//
// The job of this function is to take a given free_node found from
// `find_free` and split it according to the number of bytes to allocate.
// In doing so, it will adjust the size and next pointer of the `free_block`
// as well as the `previous` node to properly adjust the free list.
//
// PARAMETERS:
// size - the number of bytes requested to allocate
// previous - the previous node to the free block
// free_block - the node on the free list to allocate from
//
// RETURNS:
// allocated - an allocated block to be returned to the calling program
//
void split(size_t size, node_t **previous, node_t **free_block,
           header_t **allocated) {
  assert(*free_block != NULL);
  // TODO
  node_t *alloc_node = *free_block;
  size_t actualSize = size + sizeof(header_t);

  if (alloc_node->size >= actualSize + sizeof(node_t)){
    node_t *newNode = (node_t *)(((char *)alloc_node) + actualSize);
    newNode->size = alloc_node->size - actualSize;
    newNode->next = alloc_node->next;

    
    if(*previous == NULL){
      head = newNode;
    }
    else{
      (*previous)->next = newNode; 
    }
    
    (*free_block)->size = alloc_node->size - actualSize; 

    *free_block = newNode;
  } else {
    if(*previous == NULL){
      head = alloc_node->next;
    }
    else{
      (*previous)->next = alloc_node->next; 
    }   
  }

  *allocated = (header_t *)alloc_node; 
  (*allocated)->size = size; 
  (*allocated)->magic = MAGIC;

}

// Returns a pointer to a region of memory having at least the request `size`
// bytes.
//
// PARAMETERS:
// size - the number of bytes requested to allocate
//
// RETURNS:
// A void pointer to the region of allocated memory
//
void *my_malloc(size_t size) {
  // TODO
  node_t *previous = NULL;
  node_t *free_block = NULL;
  header_t *allocated = NULL;

  find_free(size, &free_block, &previous);

  if(free_block == NULL){
    return NULL;
  }

  split(size, &previous, &free_block, &allocated);

  return (void *)(((char *)allocated) + sizeof(header_t));; 
}

/*
 * Coalesces a free block with its neighbors if they are also free.
 *
 * The function assumes the free_list is sorted order. One way to do this is to
 * check the arrdress of the blocks, and insert the new block such that the
 * address of the new block is greater than the address of the previous block
 * and less than the address of the next block.
 *
 * @hints
 *   - find the location in the free_list where the new block should be
 inserted.
 *   - First check if the next free block id physically adjacent to the new
 block.
 *     If so, merge the two blocks.
 *   - Next check if the previous free block is physically adjacent to the new
 *     block. If so, merge the two blocks.
 *
 * @warning
 *  - Make sure to update the next pinters correctlly.
 *  - If the new block is the first block in the free list, make sure to update
 *    the head pointer to point to the new block.

 * PARAMETERS:
 * free_block - the block to coalesce
 *
 */
void coalesce(node_t *free_block) {
  node_t *next = head;
  node_t *prev = NULL;

  while(next && next < free_block){
    prev = next; 
    next = next->next;
  }

    free_block->next = next;

    if(prev){
      prev->next = free_block;
    } else {
      head = free_block;
    }

  

  //size_t block_size = free_block->size + sizeof(node_t);
  //size_t prev_size = prev->size + sizeof(node_t);


  if (next && 
    (char *)free_block + (free_block->size + sizeof(node_t)) == (char *)next){
      free_block->size += next->size + sizeof(node_t);
      free_block->next = next->next;

   }

   
  if (prev){ 
   if (((char *)prev) + (prev->size + sizeof(node_t)) == (char* )free_block){
    prev->size += free_block->size + sizeof(node_t);
    prev->next = free_block->next;
    }
 }

 return;
}

// Frees a given region of memory back to the free list.
//
// PARAMETERS:
// allocated - a pointer to a region of memory previously allocated by my_malloc
//
void my_free(void *allocated) {
  // TODO
  header_t *header = (header_t*)((char *)allocated - sizeof(header_t));
  assert(header->magic == MAGIC);
  node_t *free_node = (node_t *)header;
  free_node->size = header->size;
  coalesce(free_node);
}
