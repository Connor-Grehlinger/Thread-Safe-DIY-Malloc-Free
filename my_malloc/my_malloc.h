#include <stdlib.h>
#include <pthread.h>
// Meta data for alloaced blocks

typedef struct block_node_t{

  size_t size;
  struct block_node_t * next;
  struct block_node_t * prev;

} block_node;



// All malloc functions implemented with best-fit policy 

// Locking malloc/free

void * ts_malloc_lock(size_t size);

void ts_free_lock(void * ptr);



// Non-locking malloc/free

void * ts_malloc_nolock(size_t size);

void ts_free_nolock(void * ptr);



// Performance (fragmentation) functions 

unsigned long get_data_segment_size();

unsigned long get_data_segment_free_space_size();

unsigned long thread_get_data_segment_free_space_size();


// Helper functions:

// Adds to list of free blocks 
void add_to_free_list(block_node * to_add);

// Tries to split a block with size greater than the needed size
void attempt_split(block_node * to_split, size_t size_needed);

// Tries to re-use free'd blocks instead of growing heap
block_node * try_block_reuse_bf(size_t size);

// Removes a previously allocated block from the free list (it has been re-used)
void remove_from_free_list(block_node * to_remove);

// Extends the process heap segment
block_node * grow_heap(size_t size);

// Coalesces free'd blocks if they exist around free_block
void coalesce(block_node * free_block);


// Adds to list of free blocks 
void thread_add_to_free_list(block_node * to_add);

// Tries to split a block with size greater than the needed size
void thread_attempt_split(block_node * to_split, size_t size_needed);

// Tries to re-use free'd blocks instead of growing heap
block_node * thread_try_block_reuse_bf(size_t size);

// Removes a previously allocated block from the free list (it has been re-used)
void thread_remove_from_free_list(block_node * to_remove);

// Coalesces free'd blocks if they exist around free_block
void thread_coalesce(block_node * free_block);

