#include <assert.h>  //For debug!
#include <stdlib.h>

//this struct stores the meta info of a block of space
//It's basically a linked list structure
//I use this because linked list can remove/insert node efficiently
struct meta_info {
  struct meta_info * pre;
  struct meta_info * next;
  size_t
      sz;  //marks how big the according block is (in bytes) including the meta_info at the front of the block
  int is_free;  // 1 for is free, 0 for occupied
};
typedef struct meta_info meta_info;
//Thread Safe malloc/free: locking version
void * ts_malloc_lock(size_t size);
void ts_free_lock(void * ptr);

//Thread Safe mallloc/free: non-locking version
void * ts_malloc_nolock(size_t size);
void ts_free_nolock(void * ptr);
void * locked_sbrk(size_t required_size);

//Best Fit malloc/free
void * bf_malloc(size_t size, int should_lock_sbrk);
void bf_free(void * ptr);

unsigned long get_data_segment_size();             //in bytes
unsigned long get_data_segment_free_space_size();  //in bytes
