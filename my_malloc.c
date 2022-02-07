#include <pthread.h>
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
////// only put 'free' block in the list!
#include <limits.h>
#include <stdio.h>   //for printf
#include <unistd.h>  //for sbrk()

#include "my_malloc.h"

//Thread Safe malloc/free : locking version
void * ts_malloc_lock(size_t size) {
  pthread_mutex_lock(&lock);
  // printf("I'M HERE!\n");
  void * p = bf_malloc(size, 0);
  pthread_mutex_unlock(&lock);
  return p;
}
void ts_free_lock(void * ptr) {
  pthread_mutex_lock(&lock);
  bf_free(ptr);
  pthread_mutex_unlock(&lock);
}

void * ts_malloc_nolock(size_t size) {
  void * p = bf_malloc(size, 1);
  return p;
}
void * locked_sbrk(size_t required_size) {
  pthread_mutex_lock(&lock);
  void * p = sbrk(required_size);
  pthread_mutex_unlock(&lock);
  return p;
}

void ts_free_nolock(void * ptr) {
  bf_free(ptr);
}

__thread meta_info * head = NULL;  //This parameter should be global because there
                                   // is only one linked_list during the whole process
                                   // this should be initialized to NULL
void * start =
    NULL;  //this is the beginning of the whole story!used this to calculate the entire size of heap!

void print_mylist(void) {
  meta_info * cur = head;
  int i = 1;
  //  printf("The size of a meta_info is: %x\n", (unsigned int)sizeof(meta_info));
  while (cur) {
    printf("This number %d block starts at: %p\n", i, (void *)cur);
    printf("The free status is: %d\n", cur->is_free);
    printf("The size of this block is: %x\n\n", (unsigned int)cur->sz);
    cur = cur->next;
    i++;
  }
}

void bf_free(
    void *
        ptr) {  //we assume that all inputs are valid! (returned by ff_malloc previously)
  //  printf("FREE!!!!!!!!!!!!!!!\n");
  // printf("The address we are going to free is: %p\n", ptr);
  if (!ptr) {
    return;
  }  // pass NULL to ff_free is acceptable, we just do nothing
  //Frist we find the block ,set it to free
  meta_info * target = (meta_info *)ptr;
  target = target - 1;  //this points at the actual start address of that block
  target->is_free = 1;  // free it

  //now we need to insert this new node into our free-list
  if (head == NULL) {  //the first node in the list!! no need to 'insert' or 'merge'!
    head = target;
    target->pre = NULL;
    target->next = NULL;
    // print_mylist();
    // printf("\n\n\n");
    return;
  }

  //now there is at least one node in the free-list, lets find the proper position for target
  meta_info * cur = head;
  meta_info dumpnode;
  meta_info * previous = &dumpnode;  //we use this to trace the node before cur
  previous->next = head;
  while (cur && cur < target) {
    cur = cur->next;
    previous = previous->next;
  }

  if (cur == head) {  //we need to insert at front
    head->pre = target;
    target->next = head;
    head = target;
    target->pre = NULL;
  }
  else {  //there has to be at least one node before target and that is 'previous'
    //we insert target between previous and cur
    target->next = cur;
    if (cur) {
      cur->pre = target;
    }
    target->pre = previous;
    previous->next = target;
  }

  //********* newly freed space has been added into list!!!!

  //  Then we examine if this newly-freed block has a following freed block, in which case
  //     we combine them

  if (target->next && target->next == target->sz + (void *)target) {
    target->sz += target->next->sz;
    target->next = target->next->next;
    if (target->next) {
      target->next->pre = target;
    }
  }

  //  Then, we need to look at the node before target, we might also want to combine them
  meta_info * pre_node = target->pre;

  if (pre_node && target == pre_node->sz + (void *)pre_node) {
    pre_node->sz += target->sz;
    pre_node->next = target->next;
    if (pre_node->next) {
      pre_node->next->pre = pre_node;
    }
  }
  // printf("after a free for address %p:\n", ptr);
  // print_mylist();
  // printf("\n\n\n");
}

//Best Fit malloc/free
void * bf_malloc(size_t size, int should_lock_sbrk) {
  //  printf("***************************************\n");
  // void * data_break = sbrk(0);
  // printf("Before this alloc, the data break is %p\n", data_break);
  //first check if there is enough space for 'size'
  size_t size_required = sizeof(meta_info) + size;  //this is the real space needed!
  meta_info * cur = head;
  size_t min_space = INT_MAX;
  meta_info * best_fit = NULL;
  while (cur) {
    if (cur->sz == size_required) {
      best_fit = cur;
      break;  //it's impossible to find a better place!!!
    }
    if (cur->sz > size_required && cur->sz < min_space) {
      //found a better fit!
      best_fit = cur;
      min_space = cur->sz;
      //  printf("best fit changed to block %p of size %lu!\n", (void *)cur, min_space);
    }
    cur = cur->next;
  }
  cur = best_fit;  //give the value back to cur
  //now when we exit the searching process (while loop), we can tell
  //   if we can allocate spaca right away or we should ask for more spaces
  //   in heap segment
  //         return the pointer to the allocated space (pointer to the actual space)
  if (cur != NULL) {
    cur->is_free = 0;
    //split the block if it's bigger than required
    //if the remaining space is smaller than even the size of
    //a meta_info block, no need to split and keep track of it!
    if (cur->sz > size_required + sizeof(meta_info)) {
      meta_info * splited = (void *)((size_t)cur + size_required);
      splited->sz = cur->sz - size_required;
      splited->is_free = 1;
      splited->next = NULL;
      //insert a new node into the linked list, some pointers need to change
      cur->sz = size_required;
      if (cur == head) {
        head = splited;
        splited->pre = NULL;
      }
      else {  //cur has a pre node! and that node would be the pre node for split
        assert(cur->pre != NULL);
        cur->pre->next = splited;
        splited->pre = cur->pre;
      }
      if (cur->next) {  //cur has a next node! and that node would the next node for split
        cur->next->pre = splited;
        splited->next = cur->next;
      }
    }
    else {                //we simply remove the current block out of our free-list
      if (cur == head) {  //need to change head!
        head = cur->next;
      }
      else {  //cur!=head
              //now we know that there has to be at least one 'previous' node for cur
        assert(cur->pre != NULL);
        cur->pre->next = cur->next;
      }
      //if there is free block after cur, we modify its pre pointer
      if (cur->next) {
        cur->next->pre = cur->pre;
      }
    }
    cur->next = NULL;
    cur->pre = NULL;
    // printf("after a malloc for size %lu : \n", size);
    // print_mylist();
    // printf("\n\n\n");
    return cur +
           1;  //cur points at the entire block (meta+usable space),but our user only wants
    //the usable space. As cur is of pointer type, do this will jump pass sizeof(meta_info)
    //, which is exactly the start of usable space
  }

  //      if there were no enough space, use sbrk() to require for new spacec
  //         notice that we should ask more space than 'size' because we add meta info
  else {  //cur==NULL
    meta_info * new_space = NULL;
    if (should_lock_sbrk == 1) {
      new_space = locked_sbrk(size_required);
    }
    else {  //should_lock_sbrk == 0
      new_space = sbrk(size_required);
    }
    if (start == NULL) {
      start = new_space;  //this is the first block malloced!
    }
    // printf("sbrk gives us %p\n", (void *)new_space);
    // printf("but we need to give user the start of the usable space which is %p\n",
    //     (void *)(new_space + 1));
    new_space->is_free = 0;
    new_space->sz = size_required;
    // printf("after a malloc for size %lu : \n", size);
    // print_mylist();
    // printf("\n\n\n");
    return new_space + 1;  //this '+1' is of the same reason with the 'cur+1' above
  }
}

unsigned long get_data_segment_size() {
  return (unsigned long)sbrk(0) - (unsigned long)start;
}

unsigned long get_data_segment_free_space_size() {
  meta_info * cur = head;
  unsigned long ans = 0;
  while (cur) {
    assert(cur->is_free == 1);
    ans += cur->sz;
    cur = cur->next;
  }
  return ans;
}
