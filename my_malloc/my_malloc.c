#include "my_malloc.h"
#include <unistd.h>
#include <stdio.h>
#include <limits.h>

/***************************************************************** 
 * ECE650 Homework Assignment 2: Implementing Thread-Safe Malloc *
 * Developed by Connor Grehlinger (cmg88)                        *
 * January 31, 2018                                              *
 *****************************************************************/

/** NOTES: 
 * 
 * -Alignment macros not used in final implementation.
 *  
 */

/* For alignment purposes */
#define ALIGNMENT 8

/* Macro for finding the nearest multiple of 8 for alignment */
#define ALIGN(x) (((x) + (ALIGNMENT - 1)) & ~(ALIGNMENT-1))

/* Size of meta data struct (offset to payload in memory, 24 bytes) */
#define META_DATA_SIZE sizeof(block_node) 

/* Minimum size threshold that determines if a block is split. 
 * This parameter can be set based on the target workload,
 * but must be at least the size of the block_node struct to avoid error */
#define MIN_SIZE META_DATA_SIZE + 128


/* Thread local storage for free list 
 * Used in non-locking version of malloc/free */
__thread block_node * thread_head = NULL;
__thread block_node * thread_tail = NULL;
__thread size_t thread_list_size = 0;


/* Free list head */
block_node * free_list_head = NULL;

/* Free list tail */
block_node * free_list_tail = NULL;

/* Free list size */
unsigned long free_size = 0;


/* Synchronization primitives for locking malloc/free and sbrk calls:
 * 
 * -NOTE- According to man pages:
 * In cases where default mutex attributes are appropriate, the macro PTHREAD_MUTEX_INITIALIZER 
 * can be used to initialize mutexes that are statically allocated. The effect shall be equivalent
 * to dynamic initialization by a call to pthread_mutex_init() with parameter attr specified as 
 * NULL, except that no error checks are performed. 
 */
pthread_mutex_t sbrk_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;


/* Global variable for determining size of entire data segment */
unsigned long data_segment_size = 0;

/* First address returned by sbrk, used for error checking */
block_node * original_break;

/* Global variables used for debugging and performance metrics */
unsigned long long num_cos = 0;
unsigned long long num_splits = 0;
unsigned long long num_mallocs = 0;
unsigned long long sum_malloc_requests = 0;
unsigned long long num_reuse = 0;
unsigned long long num_frees = 0;


/* Print free blocks for debugging */
void print_free(){
  printf("************** Printing free blocks... ***************\n");
  block_node * current = free_list_head;
  printf("Current program break = %lu\n", sbrk(0)); 
  while (current){
    printf("----------block_start----------\n");
    if (current == free_list_head){
      printf("Free list head:\n");
    }
    if (current == free_list_tail){
      printf("Free list tail:\n");
    }
    printf("address of previous block = %lu\n", current->prev);
    printf("address of current block  = %lu, block size = %lu\n", current, current->size);
    printf("address of next block     = %lu\n", current->next);
    printf("address of block + size   = %lu\n", (char*)current + current->size);
    
    if (current == current->next){
      printf("Inf loop error, free_size = %lu\n", free_size);
      break;
    }
    current = current->next;
    printf("-----------block_end-----------\n");
  }
  printf("******************************************************\n");
}


/* Print free blocks for debugging (thread local storage version) */
void thread_print_free(){
  printf("************** Printing free blocks... ***************\n");
  block_node * current = thread_head;
  printf("Current program break = %lu\n", sbrk(0)); 
  while (current){
    printf("----------block_start----------\n");
    if (current == thread_head){
      printf("Free list head:\n");
    }
    if (current == thread_tail){
      printf("Free list tail:\n");
    }
    printf("address of previous block = %lu\n", current->prev);
    printf("address of current block  = %lu, block size = %lu\n", current, current->size);
    printf("address of next block     = %lu\n", current->next);
    printf("address of block + size   = %lu\n", (char*)current + current->size);
    
    if (current == current->next){
      printf("Inf loop error, thread_list_size = %lu\n", thread_list_size);
      break;
    }
    current = current->next;
    printf("-----------block_end-----------\n");
  }
  printf("******************************************************\n");
}


/* Adds to the free list in sorted order. 
 * Sorted insert is used to ensure that free blocks that form a contiguous 
 * segment in the heap are neighbors in the list and can be easily coalesced. */
void add_to_free_list(block_node * to_add){
  if (to_add == NULL){
    fprintf(stderr,"Error: adding a NULL block_node to the free list\n");
    return;
  }
  if (free_list_head == NULL){
    free_list_head = free_list_tail = to_add;
    free_list_head->next = NULL;
    free_list_head->prev = NULL;
  }
  else if (to_add < free_list_head){ // if added block has lower address than the list head
    to_add->next = free_list_head;   // add to front
    to_add->prev = NULL;
    to_add->next->prev = to_add;
    free_list_head = to_add;
  }
  else{
    block_node * current = free_list_head;
    while ((current->next) && (to_add > current->next)){ 
      // traverse list until <= next block or at end of free list 
      current = current->next;
    }
    to_add->next = current->next;
    to_add->prev = current;
    if (current->next){
      current->next->prev = to_add;
    }
    else{
      free_list_tail = to_add;
    }
    current->next = to_add;
  } 
  free_size++;
}


/* Adds to the free list in sorted order (thread local storage version).
 * Sorted insert is used to ensure that free blocks that form a contiguous 
 * segment in the heap are neighbors in the list and can be easily coalesced. */
void thread_add_to_free_list(block_node * to_add){
  if (to_add == NULL){
    fprintf(stderr,"Error: adding a NULL block_node to the free list\n");
    return;
  }
  if (thread_head == NULL){
    thread_head = thread_tail = to_add;
    thread_head->next = NULL;
    thread_head->prev = NULL;
  }
  else if (to_add < thread_head){ // if added block has lower address than the list head
    to_add->next = thread_head;   // add to front
    to_add->prev = NULL;
    to_add->next->prev = to_add;
    thread_head = to_add;
  }
  else{
    block_node * current = thread_head;
    while ((current->next) && (to_add > current->next)){ 
      // traverse list until <= next block or at end of free list 
      current = current->next;
    }
    to_add->next = current->next;
    to_add->prev = current;
    if (current->next){
      current->next->prev = to_add;
    }
    else{
      thread_tail = to_add;
    }
    current->next = to_add;
  } 
  thread_list_size++;
}



/* Splits a previously allocated block that has been selected for a malloc request.
 * The block should only be split if it can hold the minimum set size (MIN_SIZE). 
 * The size_needed parameter is the whole block size (for the malloc request AND block_node).
 * This function essentially creates a block within another block and
 * adds it to the list of free blocks.*/
void attempt_split(block_node * to_split, size_t size_needed){  
  if (to_split->size - size_needed >= MIN_SIZE){

    //num_splits++; // collect for performance analysis

    block_node * new_block = (block_node *) ((char *) to_split + size_needed);
    new_block->size = to_split->size - size_needed; // set new block's size to remaining size
    new_block->next = to_split->next;
    new_block->prev = to_split;
    if (to_split->next == NULL){
      free_list_tail = new_block;
    }
    else{
      to_split->next->prev = new_block;
    }
    to_split->size = size_needed; // update the size for the split block
    to_split->next = new_block;
    free_size++; // update free list size
  }
}  



/* Splits a previously allocated block that has been selected for a malloc request.
 * The block should only be split if it can hold the minimum set size (MIN_SIZE). 
 * The size_needed parameter is the whole block size (for the malloc request AND block_node).
 * This function essentially creates a block within another block and
 * adds it to the list of free blocks (thread local storage version) */
void thread_attempt_split(block_node * to_split, size_t size_needed){  
  if (to_split->size - size_needed >= MIN_SIZE){

    //num_splits++; // collect for performance analysis
    
    block_node * new_block = (block_node *) ((char *) to_split + size_needed);
    new_block->size = to_split->size - size_needed; // set new block's size to remaining size
    new_block->next = to_split->next;
    new_block->prev = to_split;
    if (to_split->next == NULL){
      thread_tail = new_block;
    }
    else{
      to_split->next->prev = new_block;
    }
    to_split->size = size_needed; // update the size for the split block
    to_split->next = new_block;
    thread_list_size++;
  }
}  



/* Coalesces either 2 or 3 adjacent free blocks together and 
 * updates the free list accordingly. */
void coalesce(block_node * free_block){
  if(free_size > 1){
    if (free_block->next){
      // next location in heap memory:
      block_node * next_location = (block_node *)((char*) free_block + free_block->size); 
      // if the next free block in mem is adjacent to block being free'd:
      if (next_location == free_block->next){ 

	//num_cos++; // collect for performance analysis

	free_block->size += free_block->next->size;
	remove_from_free_list(free_block->next);	
	if (free_size == 1){
	  return; // no blocks left to coalesce
	}
      }
    }
    if (free_block->prev){
      block_node * current_location = (block_node *)((char *) free_block->prev + free_block->prev->size); 
      if (current_location == free_block){
	
	//num_cos++; // collect for performance analysis

	free_block->prev->size += free_block->size;
	remove_from_free_list(free_block);
      }
    }
  }
}


/* Coalesces either 2 or 3 adjacent free blocks together and 
 * updates the free list accordingly (thread local storage version). */
void thread_coalesce(block_node * free_block){
  if(free_size > 1){
    if (free_block->next){
      block_node * next_location = (block_node *)((char*) free_block + free_block->size); 
      if (next_location == free_block->next){ 
	
	//num_cos++; // collect for performance analysis

	free_block->size += free_block->next->size;
	thread_remove_from_free_list(free_block->next);
	if (thread_list_size == 1){
	  return; // no blocks left to coalesce
	}
      }
    }
    if (free_block->prev){
      block_node * current_location = (block_node *)((char *) free_block->prev + free_block->prev->size); 
      if (current_location == free_block){
	
	//num_cos++; // collect for performance analysis

	free_block->prev->size += free_block->size;
	thread_remove_from_free_list(free_block);
      }
    }
  }
}


/* Removes from free list */
void remove_from_free_list(block_node * to_remove){
  if (free_list_head == NULL){
    fprintf(stderr,"Error: remove from free list called on empty list\n");
    return;
  }
  if (to_remove == NULL){
    fprintf(stderr, "Error: target block_node to remove from free list is NULL\n");
    return;
  }
  if (to_remove == free_list_head){
    free_size--;
    free_list_head = free_list_head->next;
    if (free_list_head){
      free_list_head->prev = NULL;
    }
    if (free_size == 1){
      free_list_tail = free_list_head;
      free_list_head->next = NULL;
    }   
  }
  else if (to_remove == free_list_tail){
    free_list_tail = free_list_tail->prev;
    if (free_list_tail){
      free_list_tail->next = NULL;
    }
    free_size--;
    if (free_size == 1){
      free_list_head = free_list_tail;
    }
  }
  else{
    to_remove->prev->next = to_remove->next;
    to_remove->next->prev = to_remove->prev;
    free_size--;
  }
  to_remove->next = NULL;
  to_remove->prev = NULL;
}


/* Removes from free list (thread local storage version) */
void thread_remove_from_free_list(block_node * to_remove){
  if (thread_head == NULL){
    fprintf(stderr,"Error: remove from free list called on empty list\n");
    return;
  }
  if (to_remove == NULL){
    return;
    fprintf(stderr, "Error: target block_node to remove from free list is NULL\n");
  }
  if (to_remove == thread_head){
    thread_list_size--;
    thread_head = thread_head->next;
    if (thread_head){
      thread_head->prev = NULL;
    }
    if (thread_list_size == 1){
      thread_tail = thread_head;
      thread_head->next = NULL;
    }   
  }
  else if (to_remove == thread_tail){
    thread_tail = thread_tail->prev;
    if (thread_tail){
      thread_tail->next = NULL;
    }
    thread_list_size--;
    if (thread_list_size == 1){
      thread_head = thread_tail;
    }
  }
  else{
    to_remove->prev->next = to_remove->next;
    to_remove->next->prev = to_remove->prev;
    thread_list_size--;
  }
  to_remove->next = NULL;
  to_remove->prev = NULL;
}


/* Extends the heap by size bytes and returns a block_node pointer to the old 
 * break location, which will be the address of the added block.
 * This function is used by both the locking and non-locking malloc. */   
block_node * grow_heap(size_t size){
  block_node * new_block = NULL;

  pthread_mutex_lock(&sbrk_mutex);
  if ((new_block = sbrk(size)) == (void *) -1){ // check if sbrk failed, return NULL if true
    pthread_mutex_unlock(&sbrk_mutex);
    fprintf(stderr, "Error: sbrk call with size %lu failed\n", size);	
    return NULL;
  }
  pthread_mutex_unlock(&sbrk_mutex);

  //data_segment_size += size; // keep track of data segment size
  
  new_block->size = size; // set size of the block   
  return new_block;
}


/* Search for free'd block to use, only search free list for performance. */
block_node * try_block_reuse_bf(size_t size){
  block_node * current_block = free_list_head; // get the head of the free list
  size_t smallest_diff = ULONG_MAX; 
  size_t current_diff;
  block_node * result = NULL;
  while (current_block){
    if (current_block->size >= size){ // if the current block can accomodate request
      current_diff = current_block->size - size;
      if (current_diff < smallest_diff){
	smallest_diff = current_diff; 
	result = current_block; // update best fitting block
      }
    }
    current_block = current_block->next; 
  }
  if (result){ // if block found, attempt to split it
    attempt_split(result, size);
  }
  return result;
}


/* Search for free'd block to use, only search free list for performance 
 * (thread local storage version). */
block_node * thread_try_block_reuse_bf(size_t size){
  block_node * current_block = thread_head; 
  size_t smallest_diff = ULONG_MAX; 
  size_t current_diff;
  block_node * result = NULL;
  while (current_block){ 
    if (current_block->size >= size){ 
      current_diff = current_block->size - size;
      if (current_diff < smallest_diff){
	smallest_diff = current_diff; 
	result = current_block;
      }
    }
    current_block = current_block->next;
  }
  if (result){
    thread_attempt_split(result, size);
  }
  return result;
}
  

/* Thread-safe malloc lock version. */
void * ts_malloc_lock(size_t size){
  size_t block_size = size + META_DATA_SIZE;
  block_node * target_block = NULL;

  if (original_break){ // if blocks have been allocated

    pthread_mutex_lock(&list_lock);// lock list for attempted search and removal
    
    //num_mallocs++;
    //sum_malloc_requests += block_size; // collect data for performance analysis

    target_block = try_block_reuse_bf(block_size);
    
    if (target_block){ // if block found for re-use 
      remove_from_free_list(target_block);       
      pthread_mutex_unlock(&(list_lock)); // unlock after free list modified 
    }
    else{ // extend the heap if no block found
      pthread_mutex_unlock(&(list_lock)); // unlock after failed search and removal
      if ((target_block = grow_heap(block_size)) == NULL){ // check grow_heap function
	return NULL;
      }
    }
  }
  else{ // first allocation
    if ((target_block = grow_heap(block_size)) == NULL){
      return NULL;
    }
    original_break = target_block; 
  }
  return (char*)target_block + META_DATA_SIZE;
}


/* Thread-safe free lock version. */
void ts_free_lock(void * ptr){
  if (ptr == NULL){ // freeing NULL does nothing 
    return;
  }
  // get address of meta data (block_node):
  block_node * to_free = (block_node *)((char *)ptr - META_DATA_SIZE);  
  
  pthread_mutex_lock(&(list_lock)); // lock list for insertion and coalesce attempt

  //num_frees++; // collect for performance analysis 
  add_to_free_list(to_free);
  coalesce(to_free);
 
  pthread_mutex_unlock(&(list_lock)); // unlock after insertion and attempted coalesce 
}



/* For debugging and data collection */
void print_avg(){
  printf("Average request size = %lu\n", sum_malloc_requests/num_mallocs);
}

void print_num_coalesces(){
  printf("Number of coalesces = %lu\n", num_cos);
}

void print_num_splits(){
  printf("Number of splits = %lu\n", num_splits);
}

void print_num_mallocs(){
  printf("Number of mallocs = %lu\n", num_mallocs);
}

void print_num_frees(){
  printf("Number of frees = %lu\n", num_frees);
}

void print_num_reuse(){
  printf("Number of block reuses = %lu\n", num_reuse);
}

 
/* For debugging */
/* Checks to ensure the address is one allocated by custom malloc */
char is_valid_address(block_node * to_check){	
  if (original_break){ // make sure malloc has at least been called once
    if ((original_break < to_check) && (to_check < (block_node*)sbrk(0))){ // range of thea heap
      return 1;
    }
  }
  return 0;
}
 

/* Thread-safe malloc no-lock version. */
void * ts_malloc_nolock(size_t size){
  size_t block_size = size + META_DATA_SIZE;

  //num_mallocs++;
  //sum_malloc_requests += block_size; // collect data for performance analysis

  block_node * target_block = NULL;
  if (original_break){ // if blocks have been allocated
    target_block = thread_try_block_reuse_bf(block_size);
    if (target_block){ // if block found for re-use 
      thread_remove_from_free_list(target_block); 
    }
    else{ // extend the heap if no block found 
      if ((target_block = grow_heap(block_size)) == NULL){ // check grow_heap function
	return NULL;
      }
    }
  }
  else{ // first allocation
    if ((target_block = grow_heap(block_size)) == NULL){
      return NULL;
    }
    original_break = target_block;
  }
  return (char*)target_block + META_DATA_SIZE;
}


/* Thread-safe free no-lock version (thread local storage). */
void ts_free_nolock(void * ptr){
  //num_frees++;  // collect for performance analysis
  if (ptr == NULL){ // freeing NULL does nothing 
    return;
  }
  block_node * to_free = (block_node *)((char *)ptr - META_DATA_SIZE);  
  thread_add_to_free_list(to_free);
  thread_coalesce(to_free);
}


unsigned long get_data_segment_size(){
  return data_segment_size;
}


unsigned long get_data_segment_free_space_size(){
  unsigned long free_space = 0;
  block_node * current = free_list_head;
  while (current){
    free_space += current->size;
    current = current->next;
  }
  return free_space;
}


unsigned long thread_get_data_segment_free_space_size(){
  unsigned long free_space = 0;
  block_node * current = thread_head;
  while (current){
    free_space += current->size;
    current = current->next;
  }
  return free_space;
}

