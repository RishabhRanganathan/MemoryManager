#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "memory.h"


// Define global variables
static metadata *lst_head = NULL;
static void *start_of_heap = NULL;

// Define heap checker function
int heap_checker(int lineno) {
    // Get the current top of the heap
    void *top_of_heap = sbrk(0);
    void *heap_curr = start_of_heap;

    // Count the number of free blocks in the heap
    int num_free_blocks = 0;
    while (heap_curr < top_of_heap) {
        metadata *block = (metadata *)heap_curr;
        if (block->free) num_free_blocks++;
        heap_curr = (void *)(block->next);
    }

    // Count the number of free nodes in the linked list
    metadata *list_curr = lst_head;
    int num_free_nodes = 0;
    while (list_curr) {
        if (list_curr->free) num_free_nodes++;
        list_curr = list_curr->next;
    }

    // Check if the counts match
    if (num_free_blocks != num_free_nodes) {
        fprintf(stderr, "HEAP CHECK: number of free nodes in list does not match heap at alloc.c:%d\n", lineno);
        return 0;
    }

    return 1;
}

// Allocate memory with the specified size
void *malloc(size_t size) {
    return allocate_memory(size);
}

// Allocate cleared memory with the specified number and size
void *calloc(size_t num, size_t size) {
    return allocate_cleared_memory(num, size);
}

// Reallocate memory for the given pointer with the specified size
void *realloc(void *ptr, size_t size) {
    return reallocate_memory(ptr, size);
}

// Free the memory allocated for the given pointer
void free(void *ptr) {
    free_allocated_memory(ptr);
}

// Allocate cleared memory with the specified number and size
void *allocate_cleared_memory(size_t num, size_t size) {
    size_t total = num * size;
    void *result = allocate_memory(total);
    if (!result) return NULL;
    
    char *ptr = (char *)result;
    for (size_t i = 0; i < total; ++i) {
        ptr[i] = 0;
    }
    
    return result;
}

// Define the macro for the minimum size difference to split a block
#define MIN_SPLIT_SIZE_DIFF 1024

// Split the allocated block into two if possible
metadata *split_allocated_block(size_t size, metadata *entry) {
    if (entry->size >= 2 * size && (entry->size - size) >= MIN_SPLIT_SIZE_DIFF) {
        metadata *new_entry = (metadata *)((char *)entry->ptr + size);
        new_entry->ptr = (char *)(new_entry + 1);
        new_entry->free = 1;
        new_entry->size = entry->size - size - sizeof(metadata);
        new_entry->next = entry;
        if (entry->prev) {
            entry->prev->next = new_entry;
        } else {
            memory_manager_instance.head = new_entry;
        }
        new_entry->prev = entry->prev;
        entry->size = size;
        entry->prev = new_entry;
        return new_entry;
    }
    return NULL;
}

// Allocate memory with the specified size
void *allocate_memory(size_t size) {
    if (size == 0) return NULL;
    metadata *p = memory_manager_instance.head;
    metadata *chosen = NULL;
    if (memory_manager_instance.sbrk_mem - memory_manager_instance.req_mem >= size) {
        while (p != NULL) {
            if (p->free && p->size >= size) {
                chosen = p;
                if (split_allocated_block(size, p)) {
                    memory_manager_instance.req_mem += sizeof(metadata);
                }
                break;
            }
            p = p->next;
        }
    }
    if (chosen) {
        chosen->free = 0;
        memory_manager_instance.req_mem += chosen->size;
    } else {
        if (memory_manager_instance.head && memory_manager_instance.head->free) {
            if (sbrk(size - memory_manager_instance.head->size) == (void *)-1)
                return NULL;
            memory_manager_instance.sbrk_mem += size - memory_manager_instance.head->size;
            memory_manager_instance.head->size = size;
            memory_manager_instance.head->free = 0;
            chosen = memory_manager_instance.head;
            memory_manager_instance.req_mem += memory_manager_instance.head->size;
        } else {
            chosen = sbrk(sizeof(metadata) + size);
            if (chosen == (void *)-1)
                return NULL;
            chosen->ptr = (char *)chosen + sizeof(metadata);
            chosen->size = size;
            chosen->free = 0;
            chosen->next = memory_manager_instance.head;
            if (memory_manager_instance.head) {
                chosen->prev = memory_manager_instance.head->prev;
                memory_manager_instance.head->prev = chosen;
            } else {
                chosen->prev = NULL;
            }
            memory_manager_instance.head = chosen;
            memory_manager_instance.sbrk_mem += sizeof(metadata) + size;
            memory_manager_instance.req_mem += sizeof(metadata) + size;
        }
    }
    return chosen->ptr;
}

// Coalesce the previous free block with the current block
void coalesce_previous_allocated_block(metadata *p) {
    p->size += p->prev->size + sizeof(metadata);
    p->prev = p->prev->prev;
    if (p->prev) {
        p->prev->next = p;
    } else {
        memory_manager_instance.head = p;
    }
}

// Coalesce the current block with the previous or next free block if possible
metadata *coalesce_allocated_block(metadata *p) {
    metadata *coalesced_block = NULL;

    if (p->prev && p->prev->free == 1) {
        coalesce_previous_allocated_block(p);
        coalesced_block = p->prev;
    }
    if (p->next && p->next->free == 1) {
        p->next->size += p->size + sizeof(metadata);
        p->next->prev = p->prev;
        if (p->prev) {
            p->prev->next = p->next;
        } else {
            memory_manager_instance.head = p->next;
        }
        coalesced_block = p->next;
    }

    if (coalesced_block) {
        memory_manager_instance.req_mem -= sizeof(metadata);
    }

    return coalesced_block;
}

// Free the memory allocated for the given pointer
void free_allocated_memory(void *ptr) {
    if (!ptr) return;
    metadata *p = (metadata *)ptr - 1;
    p->free = 1;
    memory_manager_instance.req_mem -= p->size;
    coalesce_allocated_block(p);
}

// Reallocate memory for the given pointer with the specified size
void *reallocate_memory(void *ptr, size_t size) {
    if (ptr == NULL) {
        return allocate_memory(size);
    }

    metadata *entry = ((metadata *)ptr) - 1;

    if (size == 0) {
        free_allocated_memory(ptr);
        return NULL;
    }

    // Attempt to split the allocated block
    metadata *new_block = split_allocated_block(size, entry);
    if (new_block) {
        memory_manager_instance.req_mem -= entry->prev->size;
    } else if (entry->prev && entry->prev->free && entry->size + entry->prev->size + sizeof(metadata) >= size) {
        // Coalesce with the previous block if possible
        memory_manager_instance.req_mem += entry->prev->size;
        coalesce_previous_allocated_block(entry);
        return entry->ptr;
    } else if (entry->size >= size) {
        // If the current block is large enough, return the same pointer
        return ptr;
    }

    // If none of the above conditions are met, allocate new memory and copy data
    void *new_ptr = allocate_memory(size);
    if (new_ptr == NULL) {
        // Allocation failed, return NULL
        return NULL;
    }

    // Copy data from old pointer to new pointer
    char *src = (char *)ptr;
    char *dest = (char *)new_ptr;
    for (size_t i = 0; i < entry->size; ++i) {
        dest[i] = src[i];
    }

    // Free the old pointer
    free_allocated_memory(ptr);

    return new_ptr;
}
