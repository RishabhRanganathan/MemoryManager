#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stddef.h>

#define MY_METADATA_EXTRA_INFO_SIZE 64

// Define the metadata struct
typedef struct metadata {
    void *ptr;                      // Pointer to the allocated memory block
    size_t size;                    // Size of the allocated memory block
    int free;                       // Flag indicating if the memory block is free (1 = free, 0 = allocated)
    struct metadata *next;          // Pointer to the next metadata entry in the linked list
    struct metadata *prev;          // Pointer to the previous metadata entry in the linked list
    char extra_info[MY_METADATA_EXTRA_INFO_SIZE]; // Additional information field
} metadata;

// Define the memory manager struct
typedef struct {
    metadata *head;                 // Pointer to the first metadata entry in the linked list
    size_t req_mem;                 // Total memory requested from the system
    size_t sbrk_mem;                // Total memory allocated from the system using sbrk
    char extra_info[MY_METADATA_EXTRA_INFO_SIZE]; // Additional information field
} memory_manager;

// Function declarations
void *allocate_memory(size_t size);
void *allocate_cleared_memory(size_t num, size_t size);
void *reallocate_memory(void *ptr, size_t size);
void free_allocated_memory(void *ptr);
metadata *split_allocated_block(size_t size, metadata *entry);
metadata *coalesce_allocated_block(metadata *p);
void coalesce_previous_allocated_block(metadata *p);
int heap_checker(int lineno);

// Macros
#define HEAP_CHECKER 1

// Define macros for heap checking
#define heap_check()
#if HEAP_CHECKER
#undef heap_check
#define heap_check() do { if(!heap_checker(__LINE__)) exit(1); } while(0)
#endif

// Global memory manager instance
extern memory_manager memory_manager_instance;

// Redefine standard memory management functions
#define malloc(size) allocate_memory(size)
#define calloc(num, size) allocate_cleared_memory(num, size)
#define realloc(ptr, size) reallocate_memory(ptr, size)
#define free(ptr) free_allocated_memory(ptr)

#endif // MEMORY_MANAGER_H
