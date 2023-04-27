#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

/* The standard allocator interface from stdlib.h.  These are the
 * functions you must implement, more information on each function is
 * found below. They are declared here in case you want to use one
 * function in the implementation of another. */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

//static int validatefreelist();
//static int validate(void *p);
//static int validateheap();

typedef struct node {
    int64_t header;
    struct node *next;
} node;

//static int calls = 0;
static node **freelist = NULL;

/* When requesting memory from the OS using sbrk(), request it in
 * increments of CHUNK_SIZE. */
#define CHUNK_SIZE (1<<12)

#define METADATA 8

/*
 * This function, defined in bulk.c, allocates a contiguous memory
 * region of at least size bytes.  It MAY NOT BE USED as the allocator
 * for pool-allocated regions.  Memory allocated using bulk_alloc()
 * must be freed by bulk_free().
 *
 * This function will return NULL on failure.
 */
extern void *bulk_alloc(size_t size);

/*
 * This function is also defined in bulk.c, and it frees an allocation
 * created with bulk_alloc().  Note that the pointer passed to this
 * function MUST have been returned by bulk_alloc(), and the size MUST
 * be the same as the size passed to bulk_alloc() when that memory was
 * allocated.  Any other usage is likely to fail, and may crash your
 * program.
 */
extern void bulk_free(void *ptr, size_t size);

/*
 * This function computes the log base 2 of the allocation block size
 * for a given allocation.  To find the allocation block size from the
 * result of this function, use 1 << block_size(x).
 *
 * Note that its results are NOT meaningful for any
 * size > 4088!
 *
 * You do NOT need to understand how this function works.  If you are
 * curious, see the gcc info page and search for __builtin_clz; it
 * basically counts the number of leading binary zeroes in the value
 * passed as its argument.
 */
static inline __attribute__((unused)) int block_index(size_t x) {
    if (x <= 8) {
        return 5;
    } else {
        return 32 - __builtin_clz((unsigned int)x + 7);
    }
}

/*
 * You must implement malloc().  Your implementation of malloc() must be
 * the multi-pool allocator described in the project handout.
 */
void *malloc(size_t size) {
  //validatefreelist();
  if (size <= 0) {
    return NULL;
  }
  void *p = NULL;
  //int bulkmalloc = 0;
  if (freelist == NULL) {
    freelist = sbrk(CHUNK_SIZE);
    //calls++;
    void *current = (void *)freelist + (1 << block_index(sizeof(node **) * 13));
    for (int i = 0; 1 << i < CHUNK_SIZE << 1; i++) {
      freelist[i] = NULL;
    }
    for (int i = 0; i < (CHUNK_SIZE - sizeof(node **) * 13) / (1 << 7); i++) {
      ((node *)current)->header = (int64_t) ((1 << 7) & (int64_t) -2);
      ((node *)current)->next = freelist[7];
      freelist[7] = current;
      current += 1 << 7;
    }
  }
  if (size > CHUNK_SIZE - METADATA) {
    p = bulk_alloc(size + METADATA);
    ((node *)p)->header = (int64_t) ((size + METADATA) | (int64_t) 1);
    p += METADATA;
    //bulkmalloc = 1;
  }
  else if (size <= CHUNK_SIZE - METADATA) {
    if (freelist[block_index(size)] == NULL) {
      void *current = sbrk(CHUNK_SIZE);
      //calls++;
      for (int i = 0; i < CHUNK_SIZE / (1 << block_index(size)); i++) {
        ((node *)current)->header = (int64_t) ((1 << block_index(size)) & (int64_t) -2);
        ((node *)current)->next = freelist[block_index(size)];
        freelist[block_index(size)] = current;
        current += 1 << block_index(size);
      }
    }
    p = freelist[block_index(size)];
    ((node *)p)->header = (int64_t) ((1 << block_index(size)) | (int64_t) 1);
    freelist[block_index(size)] = (freelist[block_index(size)])->next;
    p += METADATA;
  }
  //fprintf(stderr, "malloc result is block at %p size %zu flag %zu combined %zu\n", p - METADATA, ((node *)(p - METADATA))->header & (int64_t) -2, ((node *)(p - METADATA))->header & (int64_t) 1, ((node *)(p - METADATA))->header);
  //validatefreelist();
  //validateheap();
  // if (bulkmalloc == 0) {
  //   validate(p - METADATA);
  // }
  return p;
}

/*
 * You must also implement calloc().  It should create allocations
 * compatible with those created by malloc().  In particular, any
 * allocations of a total size <= 4088 bytes must be pool allocated,
 * while larger allocations must use the bulk allocator.
 *
 * calloc() (see man 3 calloc) returns a cleared allocation large enough
 * to hold nmemb elements of size size.  It is cleared by setting every
 * byte of the allocation to 0.  You should use the function memset()
 * for this (see man 3 memset).
 */
void *calloc(size_t nmemb, size_t size) {
  //validatefreelist();
  if (size <= 0 || nmemb <= 0) {
    return NULL;
  }
  void *ptr = malloc(nmemb * size);
  memset(ptr, 0, nmemb * size);
  //fprintf(stderr, "calloc result is block at %p size %zu flag %zu combined %zu\n", ptr - METADATA, ((node *)(ptr - METADATA))->header & (int64_t) -2, ((node *)(ptr - METADATA))->header & (int64_t) 1, ((node *)(ptr - METADATA))->header);
  //validatefreelist();
  //validateheap();
  //validate(ptr - METADATA);
  return ptr;
}

/*
 * You must also implement realloc().  It should create allocations
 * compatible with those created by malloc(), honoring the pool
 * alocation and bulk allocation rules.  It must move data from the
 * previously-allocated block to the newly-allocated block if it cannot
 * resize the given block directly.  See man 3 realloc for more
 * information on what this means.
 *
 * It is not possible to implement realloc() using bulk_alloc() without
 * additional metadata, so the given code is NOT a working
 * implementation!
 */
void *realloc(void *ptr, size_t size) {
  //validatefreelist();
  if (ptr == NULL) {
    return malloc(size);
  }
  if (size == 0) {
    if (ptr != NULL) {
      free(ptr);
      //validate(ptr - METADATA);
    }
    return NULL;
  }
  void *p = NULL;
  int64_t cleaned = ((node *)(ptr - METADATA))->header & (int64_t) -2;
  if (block_index(cleaned - METADATA) >= block_index(size)) {
    //validate(ptr - METADATA);
    //return ptr;
    p = ptr;
  }
  //void *p = malloc(size);
  else {
    //p = malloc(size);
    //p = memcpy(p - METADATA, ptr - METADATA, block_index(cleaned) + METADATA);
    int64_t newSize = 0;
    if (block_index(cleaned - METADATA) > block_index(size)) {
      newSize = (1 << block_index(size)) - METADATA;
    }
    else {
      newSize = cleaned - METADATA;
    }
    p = memcpy(malloc(size), ptr, newSize);
    //((node *)(p - METADATA))->header = (int64_t) ((1 << block_index(size)) | (int64_t) 1);
    free(ptr);
    //fprintf(stderr, "realloc result is block at %p size %zu flag %zu combined %zu\n", p - METADATA, ((node *)(p - METADATA))->header & (int64_t) -2, ((node *)(p - METADATA))->header & (int64_t) 1, ((node *)(p - METADATA))->header);
    //validatefreelist();
    //validateheap();
    //validate(p - METADATA);
  }
  return p;
}

/*
 * You should implement a free() that can successfully free a region of
 * memory allocated by any of the above allocation routines, whether it
 * is a pool- or bulk-allocated region.
 *
 * The given implementation does nothing.
 */
void free(void *ptr) {
  //int bulkfreed = 0;
  //validatefreelist();
  if (ptr == NULL) {
    return;
  }
  int64_t cleaned = ((node *)(ptr - METADATA))->header & (int64_t) -2;
  //int getcha = block_index(cleaned - METADATA);
  //fprintf(stderr, "freeing block at %p size %zu flag %zu combined %zu\n", ptr, cleaned, ((node *)(ptr - METADATA))->header & (int64_t) 1, ((node *)(ptr - METADATA))->header);
  if (cleaned - METADATA > CHUNK_SIZE - METADATA) {
    //bulk_free(ptr, cleaned - METADATA);
    //fprintf(stderr, "bulk freeing %zu bytes from address %p\n", cleaned, ptr - METADATA);
    bulk_free(ptr - METADATA, cleaned);
    //bulkfreed = 1;
  }
  else if (cleaned - METADATA <= CHUNK_SIZE - METADATA) {
    ((node *)(ptr - METADATA))->header &= (int64_t) -2;
    ((node *)(ptr - METADATA))->next = freelist[block_index(cleaned - METADATA)];
    freelist[block_index(cleaned - METADATA)] = ptr - METADATA;
  }
  //validatefreelist();
  //validateheap();
  //if (bulkfreed == 0) {
    //int fastint = block_index(cleaned - METADATA);
    //fprintf(stderr, "free result is block at %p size %zu flag %zu combined %zu\n", freelist[fastint], freelist[fastint]->header & (int64_t) -2, freelist[fastint]->header & (int64_t) 1, freelist[fastint]->header);
  //   validate(freelist[block_index(cleaned - METADATA)]);
  //}
  return;
}

// int validatefreelist() {
//   if (freelist != NULL) {
//     for (int i = 0; i < 5; i++) {
//       if (freelist[i] != NULL) {
//         fprintf(stderr, "**VALIDATOR: FREELIST %d NOT NULL\n", i);
//         return 1;
//       }
//     }
//     for (int i = 5; i < 13; i++) {
//       for (void *p = freelist[i]; p != NULL; p = ((node *)p)->next) {
//         if ((((node *)p)->header & (int64_t) -2) < 32) {
//           fprintf(stderr, "**VALIDATOR: %p of FREELIST %d HEADER SIZE TOO SMALL SIZE: %zu\n", p, i, ((node *)p)->header & (int64_t) -2);
//           return 1;
//         }
//         if ((((node *)p)->header & (int64_t) -2) > 4096) {
//           fprintf(stderr, "**VALIDATOR: %p of FREELIST %d HEADER SIZE TOO LARGE SIZE: %zu\n", p, i, ((node *)p)->header & (int64_t) -2);
//           return 1;
//         }
//         if ((((node *)p)->header & (int64_t) 1) == 1) {
//           fprintf(stderr, "**VALIDATOR: %p of FREELIST %d HEADER SIZE FLAG NOT 0 FLAG: %zu\n", p, i, ((node *)p)->header & (int64_t) 1);
//           return 1;
//         }
//       }
//     }
//   }
//   return 0;
// }
//
// int validate(void *p) {
//   if ((((node *)p)->header & (int64_t) -2) < 32) {
//     fprintf(stderr, "**VALIDATOR: %p HEADER SIZE TOO SMALL SIZE: %zu\n", p, ((node *)p)->header & (int64_t) -2);
//     return 1;
//   }
//   if ((((node *)p)->header & (int64_t) -2) > 4096) {
//     fprintf(stderr, "**VALIDATOR: %p HEADER SIZE TOO LARGE SIZE: %zu\n", p, ((node *)p)->header & (int64_t) -2);
//     return 1;
//   }
//   if ((((node *)p)->header & (int64_t) 1) == 1) {
//     for (void *p2 = freelist[block_index(((node *)p)->header & (int64_t) -2)]; p2 != NULL; p2 = ((node *)p2)->next) {
//       if ((node *)p == (node *)p2) {
//         fprintf(stderr, "**VALIDATOR: %p HEADER SIZE FLAG NOT 0 BUT IN FREE LIST FLAG: %zu\n", p, ((node *)p)->header & (int64_t) 1);
//         return 1;
//       }
//     }
//   }
//   if ((((node *)p)->header & (int64_t) 1) == 0) {
//     if (freelist[block_index((((node *)p)->header & (int64_t) -2) - METADATA)] != p) {
//       fprintf(stderr, "**VALIDATOR: %p FREED BLOCK NOT IN CORRECT SPOT IN FREE LIST HEAD: %p\n", p, freelist[block_index(((node *)p)->header & (int64_t) -2)]);
//       return 1;
//     }
//   }
//   return 0;
// }

// int validateheap() {
//   //fprintf(stderr, "calls %d gap between mem %zu\n", calls, ((void *)freelist + CHUNK_SIZE * calls) - (void *)freelist);
//   void *current = freelist;
//   for (int i = 0; i < 13; i++) {
//     if (freelist != NULL) {
//       for (int i = 0; i < 5; i++) {
//         if (freelist[i] != NULL) {
//           fprintf(stderr, "**VALIDATOR: FREELIST %d NOT NULL\n", i);
//           return 1;
//         }
//       }
//       for (int i = 5; i < 13; i++) {
//         for (void *p = freelist[i]; p != NULL; p = ((node *)p)->next) {
//           if ((((node *)p)->header & (int64_t) -2) < 32) {
//             fprintf(stderr, "**VALIDATOR: %p OF FREELIST %d HEADER SIZE TOO SMALL SIZE: %zu\n", p, i, ((node *)p)->header & (int64_t) -2);
//             return 1;
//           }
//           if ((((node *)p)->header & (int64_t) -2) > 4096) {
//             fprintf(stderr, "**VALIDATOR: %p OF FREELIST %d HEADER SIZE TOO LARGE SIZE: %zu\n", p, i, ((node *)p)->header & (int64_t) -2);
//             return 1;
//           }
//           if ((((node *)p)->header & (int64_t) 1) == 1) {
//             fprintf(stderr, "**VALIDATOR: %p OF FREELIST %d HEADER SIZE FLAG NOT 0 FLAG: %zu\n", p, i, ((node *)p)->header & (int64_t) 1);
//             return 1;
//           }
//         }
//       }
//     }
//   }
//   current += (1 << block_index(sizeof(node *) * 13));
//   for (; current < ((void *)freelist + CHUNK_SIZE * calls); current += (((node *)current)->header & (int64_t) -2)) {
//     if ((((node *)current)->header & (int64_t) -2) < 32) {
//       //fprintf(stderr, "**VALIDATOR: %p HEADER SIZE TOO SMALL SIZE: %zu\n", current, ((node *)current)->header & (int64_t) -2);
//       return 1;
//     }
//     if ((((node *)current)->header & (int64_t) -2) > 4096) {
//       fprintf(stderr, "**VALIDATOR: %p HEADER SIZE TOO LARGE SIZE: %zu\n", current, ((node *)current)->header & (int64_t) -2);
//       return 1;
//     }
//     if ((((node *)current)->header & (int64_t) 1) == 1) {
//       for (void *p2 = freelist[block_index(((node *)current)->header & (int64_t) -2)]; p2 != NULL; p2 = ((node *)p2)->next) {
//         if ((node *)current == (node *)p2) {
//           fprintf(stderr, "**VALIDATOR: %p HEADER SIZE FLAG NOT 0 BUT IN FREE LIST FLAG: %zu\n", current, ((node *)current)->header & (int64_t) 1);
//           return 1;
//         }
//       }
//     }
//     if ((((node *)current)->header & (int64_t) 1) == 0) {
//       int isIn = 0;
//       for (void *p2 = freelist[block_index((((node *)current)->header & (int64_t) -2) - METADATA)]; p2 != NULL; p2 = ((node *)p2)->next) {
//         //fprintf(stderr, "**VALIDATOR current %p p2 %p currents %zu p2s %zu\n", current, p2, ((node *)current)->header & (int64_t) -2, ((node *)p2)->header & (int64_t) -2);
//         if ((node *)current == (node *)p2) {
//           isIn = 1;
//         }
//       }
//       if (isIn != 1) {
//         fprintf(stderr, "**VALIDATOR: %p HEADER SIZE FLAG 0 BUT NOT IN FREE LIST HEAD: %p\n", current, freelist[block_index(((node *)current)->header & (int64_t) -2)]);
//         return 1;
//       }
//       // else {
//       //   fprintf(stderr, "\n\nfound\n\n");
//       // }
//     }
//   }
//   return 0;
// }
