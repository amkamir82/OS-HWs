/*
* mm_alloc.c
        *
        * Stub implementations of the mm_* routines. Remove this comment and provide
        * a summary of your allocator's design here.
*/

#include "mm_alloc.h"
#include <memory.h>
#include <unistd.h>

s_block_ptr head = NULL;

s_block_ptr initial_heap(size_t s) {
    void *brk = sbrk(s + sizeof(struct s_block));
    if (brk == (void *) -1) {
        return NULL;
    } else {
        s_block_ptr block = (s_block_ptr) brk;
        block->size = s;
        block->next = NULL;
        block->prev = NULL;
        block->is_free = 0;
        block->ptr = brk + sizeof(struct s_block);
        memset(block->ptr, 0, block->size);
        head = block;
        return block->ptr;
    }
}
/* Your final implementation should comment out this macro. */

#define MM_USE_STUBS

void *mm_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    if (head == NULL) {
        return initial_heap(size);
    }

    s_block_ptr block = head;
    s_block_ptr last_block = NULL;

    while (block != NULL) {
        if (block->size >= size && block->is_free) {
            block->is_free = 0;
            split_block(block, size);
            return block->ptr;
        }
        last_block = block;
        block = block->next;
    }
    return extend_heap(last_block, size);

//#ifdef MM_USE_STUBS
//#else
//#endif
}


s_block_ptr extend_heap(s_block_ptr last, size_t s) {
    void *brk = sbrk(s + sizeof(struct s_block));
    if (brk == (void *) -1) {
        return NULL;
    } else {
        s_block_ptr block = (s_block_ptr) brk;
        block->size = s;
        block->next = NULL;
        block->prev = last;
        block->is_free = 0;
        block->ptr = brk + sizeof(struct s_block);
        memset(block->ptr, 0, block->size);
        last->next = block;
        return block->ptr;
    }
}

void split_block(s_block_ptr b, size_t s) {
    if (b) {
        if (s > 0) {
            if (b->size - s >= sizeof(struct s_block)) {
                s_block_ptr splitted_block = (s_block_ptr) (b->ptr + s);
                splitted_block->next = b->next;
                splitted_block->prev = b;
                splitted_block->size = b->size - s - sizeof(struct s_block);
                splitted_block->ptr = b->ptr + s + sizeof(struct s_block);
                b->next = splitted_block;
                b->size = s;
                if (splitted_block->next) {
                    (splitted_block->next)->prev = splitted_block;
                }
                mm_free(splitted_block->ptr);
                memset(b->ptr, 0, b->size);
            }
        }

    }
}

void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL && size == 0) {
        return mm_malloc(0);
    }
    if (size == 0 && ptr != NULL) {
        mm_free(ptr);
        return NULL;
    }
    if (ptr == NULL && size > 0) {
        return mm_malloc(size);
    }
    s_block_ptr block = get_block(ptr);
    size_t realloc_size = size;
    void *new_block_ptr = mm_malloc(realloc_size);
    s_block_ptr new_block = (s_block_ptr) new_block_ptr;
    new_block->ptr = new_block_ptr;
    new_block->size = realloc_size;
    memset(new_block->ptr, 0, new_block->size);
    memcpy(new_block_ptr, ptr, block->size < realloc_size ? block->size : realloc_size);
    mm_free(ptr);

    return new_block_ptr;

#ifdef MM_USE_STUBS
#else
#error Not implemented.
#endif
}


s_block_ptr fusion(s_block_ptr b) {
    if (b->next != NULL && b->next->is_free) {
        b->size = b->size + sizeof(struct s_block) + (b->next)->size;
        b->next = (b->next)->next;
        if (b->next != NULL) {
            (b->next)->prev = b;
        }
    }
    if (b->prev != NULL && b->prev->is_free) {
        (b->prev)->next = b->next;
        (b->prev)->size = (b->prev)->size + sizeof(struct s_block) + b->size;
        if (b->next != NULL) {
            (b->next)->prev = b->prev;
        }
        b = b->prev;
    }
    return b;
}

s_block_ptr get_block(void *p) {
    s_block_ptr temp = head;
    while (temp != NULL) {
        if (temp->ptr == p) {
            return temp;
        } else {
            temp = temp->next;
        }
    }
    return NULL;
}

void mm_free(void *ptr) {
    if (ptr == NULL) {
        return;
    } else {
        s_block_ptr block = get_block(ptr);
        block->is_free = 1;
        fusion(block);
    }
//#ifdef MM_USE_STUBS
//#else
//#error Not implemented.
//#endif
}