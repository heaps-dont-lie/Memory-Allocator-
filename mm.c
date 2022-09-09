/*
 * mm.c
 *
 * Name: Aman Pandey (afp5379)
 * Reference: 1. Computer Systems: A Programmer's Perspective
 *            2. Operating Systems: Three easy pieces
 *            3. Geeks for Geeks
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read malloclab.pdf carefully and in its entirety before beginning.
 *
 * 1. FREE BLOCK DIAGRAM:
 *
 *  ---------------------------------------------------------------------------
 *  |         |              |              |                      |          |
 *  |         |              |              |                      |          |
 *  |  Header | Prev_Pointer | Next_Pointer | Padding for          |  Footer  |
 *  |         |              |              | alignment (optional) |          |
 *  |         |              |              |                      |          |
 *  ---------------------------------------------------------------------------
 *            ^
 *            bp
 *
 * 2. ALLOCATED BLOCK DIAGRAM: (FOOTER HAS BEEN REMOVED FOR OPTIMIZATION PURPOSES)
 *
 *  ----------------------------------------------------------------------------
 *  |         |                               |                                |
 *  |         |                               |                                |
 *  |  Header |        Payload Area           |         Padding for            |
 *  |         |                               |     alignment (optional)       |
 *  |         |                               |                                |
 *  ----------------------------------------------------------------------------
 *            ^
 *            bp
 *
 *
 *
 * 3. SEGREGATED FREE LIST DIAGRAM:
 *
 *  NOTE: N : NULL
 *        ||: Pointer to the next and previous node.
 *        H : Headers for different free list/
 *
 *  free_node* head_list[9];
 *  -------------------------------------------------------
 *  | H0  | H1  | H2  | H3  | H4  | H5  | H6  | H7  | H8  |
 *  ---|-----|-----|----|------|-----|-----|-----|-----|---
 *     | N   | N   |    |      |     |     |     |     |  NULL
 *     V |   V |   V    V      V     V     V     V     V  |
 *   -----  ----  NULL NULL   NULL  NULL  NULL  NULL ------
 *   |   |  |  |                                     |    |
 *   -----  ----                                     ------
 *     ||    |                                         ||
 *   -----  NULL                                     ------
 *   |   |                                           |     |
 *   -----                                           -------
 *     ||                                               |
 *   -----                                             NULL
 *   |   |
 *   -----
 *     |
 *    NULL
 *
 *
 *  coalesce() -> This function merges the two adjacent free blocks and then puts the free
 *                block in the appropriate segregated list.
 *
 *  place()   ->  This function splits a free block into two parts so as to not give away
 *                any unnecessary space to the user and hence avoids internal fragmentation.
 *
 *  find_fit()->  This function finds any free spacein the segregated free list
 *                that could be given to the user.
 *
 *  malloc()  ->  This is the function that the user calls for space allocation and this function in turn
 *                calls find_fit(), place() to make the request. Argument is the size required by the user.
 *
 *  realloc() ->  This function helps in allocation of an already allocated block with different sizes.
 *                If the new size requested is less than the old size then we reduce the block size
 *                without changing the location of memory. Else if the new size cannot be accomodated
 *                in the allocated block then malloc is called then, the old data is copied to the new
 *                location and then the old block is freed. The user now gets a new memory address.
 *
 *  free()    ->  This function is used/called by the user to free the block after use. Argument is just
 *                the pointer to the payload area.
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
//#define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16

/* word sizes */
#define WSIZE 8  //Word and header/footer size(bytes) --> from textbook pg830
#define DSIZE 16 // Double word size --> from textbook
#define CHUNKSIZE (1<<5) // Minimizing the CHUNKSIZE value to improve utilization or increase utilization ratio____//Extend heap by CHUNKSIZE amount

/*global pointer that points in the middle of the Prologue block.*/
static void* heap_listp = NULL;

/*data structure that manages the linked list operation and this data structure to be stored inside the free block space. */
typedef struct DoublyLinkedList_free_node{
  struct DoublyLinkedList_free_node* prev;
  struct DoublyLinkedList_free_node* next;
}free_node;

/* array of pointers that store the headers that points to the segList. */
free_node* head_list[9];


/* Function declaration */
static void* extend_heap(size_t words);
static void* coalesce(void *bp);
static void* find_fit(size_t asize);
static void place(void* bp, size_t asize, int flag);
void segList_init();
int segList_alloc(size_t size);

/* Some important inline functions (MACROS) */ //from text book(Computer Systems)
static inline unsigned long MAX(unsigned long x, unsigned long y)
{
    return (x > y) ? x : y;
}
//Pack a size and alloc bit into a word.(book)
static inline unsigned long PACK(long size, long alloc)
{
    return ((size) | (alloc));
}

/* Read and write a word at address p --> from book */
static inline unsigned long GET(void* p)
{
    return (*(unsigned long*)(p));
}
static inline void PUT(void* p, unsigned long val)
{
    *(unsigned long*)(p) = val;
}

/* Read the size and allocated fields from address p */
static inline unsigned long GET_SIZE(void* p)
{
  unsigned long r = GET(p)/WSIZE;
  if (r%2)
  {
    return (GET(p) & ~0x7);
  }
  else
  {
    return (GET(p) & ~0xf);
  }
}
static inline unsigned long GET_ALLOC(void* p)
{
    return (GET(p) & 0x1);
}

/* Second least significant bit stores the allocation bit of the pevious block */
static inline unsigned long GET_PREV_ALLOC(void* p)
{
  return ((GET(p) & 0x2)>>1);
}

/* Given block ptr bp, compute the address of its header and footer */
static inline void* HDRP(void* bp)
{
    return ((char*)(bp) - WSIZE);
}
static inline void* FTRP(void* bp)
{
    return ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE);
}

//Given a bp, compute the address of next and previous blocks
static inline void* NEXT_BLKP(void* bp)
{
    return ((char*)(bp) + GET_SIZE((void*)((char*)(bp) - WSIZE)));
}
static inline void* PREV_BLKP(void* bp)
{
    return ((char*)(bp) - GET_SIZE((void*)((char*)(bp) - DSIZE)));
}


/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * Initialize: returns false on error, true on success.(From TEXTBOOK: COMPUTER SYSTEMS)
 */
bool mm_init(void)
{
    /* IMPLEMENT THIS */
  
    /* Initializes all the heads of the seg lists to NULL */
    segList_init();
  
    /*creating a 4 word heap containing a padding, 2 prologue blocks, and a epilogue block.*/
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
    {
        return false;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 3));
    PUT(heap_listp + (3*WSIZE), PACK(0, 3));
    /* points at the mid of the prologue block or at footer of PB. */
    heap_listp = heap_listp + 2*WSIZE;
    
    /* Now extending the heap to create a CHUNK for the data */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return false;
    return true;
}

/*
 * function to extend the heap --> extend_heap(COMPUTER SYSTEMS TEXTBOOK)
 */
static void* extend_heap(size_t words)
{
    void* bp;
    size_t size;

    size = (words%2) ? (words+1)*WSIZE : words*WSIZE; 
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;
  
    /*to get the allocation of the block prior to the epilB.*/
    unsigned long prevalloc = GET_PREV_ALLOC(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0|prevalloc<<1)); //10 -> 2
    PUT(FTRP(bp), PACK(size, 0|prevalloc<<1)); //10 -> 2
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* EPILOGUE */

    return coalesce(bp);
}

/*
 * SEG LIST INITIALIZER AND ALLOCATOR
 */

void segList_init()
{
  for (int i = 0; i <= 8; i++)
  {
    head_list[i] = NULL;
  }
}

/* returns the index of the head that points to a particular linked list according to the size requested */
int segList_alloc(size_t size)
{
  if (size >= 8193)
  {
    return 8;
  }
  else if (size == 32 || size == 60)
  {
    return 7;
  }
  else if (size >= 4097 && size <= 8192)
  {
    return 6;
  }
  else if (size >= 1025 && size <= 4096)
  {
    return 5;
  }
  else if (size >= 257 && size <= 1024)
  {
    return 4;
  }
  else if (size >= 129 && size <= 256)
  {
    return 3;
  }
  else if (size >= 65 && size <= 128 && size != 80)
  {
    return 2;
  }
  else if (size >= 49 && size <= 64 && size != 60)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

/*
 * IMPLEMENTING DOUBLY LINKED LIST
 */

/* Pushing at the head */
static void push_node(free_node** head, free_node* node)
{
  node->prev = NULL;
  node->next = *head;
  if (*head != NULL)
  {
    (*head)->prev = node;
  }
  *head = node;
  return;
}

static void delete_node(free_node** head, free_node* node)
{
  /* if the node to be deleted is the 1st node then make the head point to the next node */
  if (*head == node)
  {
    *head = node->next;
  }
  
  /* if the node next to the one that is to be deleted is not null then set the previous pointer of the next node */
  if (node->next != NULL)
  {
    node->next->prev = node->prev;
  }
  
  /* if the node before the node that is to be deleted is not null then set next of the previous node */
  if (node->prev != NULL)
  {
    node->prev->next = node->next;
  }
  return;
}

/* For Debugging purposes */
static int count_node(free_node** head)
{
  int cnt = 0;
  if (*head == NULL)
  {
    printf("Total free blocks = %d\n", cnt);
    return cnt;
  }
  free_node* iter = *head;
  while (iter != NULL)
  {
    iter = iter->next;
    cnt = cnt + 1;
  }
  printf("Total free blocks = %d\n", cnt);
  return cnt;
}



/*
 * coalesce(FROM TEXTBOOK: COMPUTER SYSTEMS)
 * This function merges two adjacent free blocks
 * to minimize external fragmentation.
 */
static void* coalesce(void* bp)
{
    /* accessing the prev alloc status of the prev block from the current block's header  M */
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp)); //M
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    size_t nextb_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
  
    free_node* nextblk = NEXT_BLKP(bp);
    free_node* prevblk = PREV_BLKP(bp);
    int ch;

    /* In this case we just push the
     * free block as prev and next
     * block is allocated
     */
    if (prev_alloc && next_alloc)
    {
        /* to let the next block know that the previous block is free   M */
        PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), 1));  //M
        /* gives the head for the list of the appropriate size */
        ch = segList_alloc(size);
        push_node(&head_list[ch], bp);
        return bp;
    }

    /*
     * In this case we delete the next block and
     * merge it with the current block and then
     * push the newly formed free block into
     * the appropriate sized list.
     */
    else if (prev_alloc && !next_alloc)
    {
        ch = segList_alloc(nextb_size);
        delete_node(&head_list[ch], nextblk);
        size = size + GET_SIZE(HDRP(NEXT_BLKP(bp)));
    
        PUT(HDRP(bp), PACK(size, 2));
        /* Free Blocks have footers */
        PUT(FTRP(bp), PACK(size, 2));
        /* also to let the next block know that the previous block is free  M */
        PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), 1));
        ch = segList_alloc(size);
        push_node(&head_list[ch], bp);
    }

    /*
     * In this case we delete the previous block
     * then merge the current and the prev block
     * and push the newly formed sized free block
     * into the appropriate free list.
     */
    else if (!prev_alloc && next_alloc)
    {
        size_t prevb_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        ch = segList_alloc(prevb_size);
        delete_node(&head_list[ch], prevblk);
        size = size + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 2));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 2));
        /* also to let the next block know that the previous block is free */
        PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), 1));
        bp = PREV_BLKP(bp);
        ch = segList_alloc(size);
        push_node(&head_list[ch], bp);
    }

    /*
     * In this case we are first deleting the prev and the next block
     * then merging the 3 block (that is the current prev and next)
     * and then adding the new sized free block into the appropriate
     * free list
     */
    else
    {
        size_t prevb_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        ch = segList_alloc(nextb_size);
        delete_node(&head_list[ch], nextblk);
        ch = segList_alloc(prevb_size);
        delete_node(&head_list[ch], prevblk);
        size = size + GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 2));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 2));
        /* also to let the next block know that the previous block is free  M */
        PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), 1));
        bp = PREV_BLKP(bp);
        ch = segList_alloc(size);
        push_node(&head_list[ch], bp);
    }
    
    return bp;
}

/*
 * here we traverse the heap and try to find the first fit.
 * find_fit(FROM TB: COMPUTER TEXTBOOK)
 * Checks in all the free list from and after the index ch.
 */
static void* find_fit(size_t asize)  /* MODIFIED FIND FIT FOR SEGREGATED FREE LIST */
{
  
  int ch = segList_alloc(asize);
  free_node* iter = head_list[ch];
  
  /* to go to other segregated free list */
  while (ch <= 8)
  {
      iter = head_list[ch];
      /* traverses a free list */
      while (iter != NULL)
      {
          if (asize <= GET_SIZE(HDRP((void*)(iter))))
          {
              return (void*)(iter);
          }
          iter = iter -> next;
      }
      ch = ch + 1;
  }
  
  return NULL;
}


/*
 * place/split function splits if asize >= 2*DSIZE to avoid internal fragmentation
 * else we do not split.
 * place(FROM TB: COMPUTER SYSTEMS)
 */
static void place(void *bp, size_t asize, int flag)
{
    size_t csize = GET_SIZE(HDRP(bp));
    size_t diff = csize - asize;
    int ch;

    if ((csize - asize) >= (2*DSIZE))
    {
        /*
         * if the realloc() calls place then the block is already not in the free list
         * hence flag ensures that the nodes are not deleted if the call is from
         * realloc.
         */
        if (flag)
        {
            ch = segList_alloc(csize);
            delete_node(&head_list[ch], bp);
            PUT(HDRP(bp), PACK(asize, 3));
            bp = NEXT_BLKP(bp);
            PUT(HDRP(bp), PACK(csize-asize, 2));
            PUT(FTRP(bp), PACK(csize-asize, 2));
            ch = segList_alloc(diff);
            push_node(&head_list[ch], bp);
        }
      
        /* Handling calls from realloc() */
        else
        {
          unsigned long prevalloc = GET_PREV_ALLOC(HDRP(bp));
          PUT(HDRP(bp), PACK(asize, 1|prevalloc<<1));
          bp = NEXT_BLKP(bp);
          PUT(HDRP(bp), PACK(csize-asize, 2));
          PUT(FTRP(bp), PACK(csize-asize, 2));
          coalesce(bp);
          return;
        }
    }

    /* Do not split */
    else
    {
        if (flag)
        {
            PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), 3));
            ch = segList_alloc(csize);
            delete_node(&head_list[ch], bp);
            PUT(HDRP(bp), PACK(csize, 3));
        }
        else
        {
          unsigned long prevalloc = GET_PREV_ALLOC(HDRP(bp));
          PUT(HDRP(bp), PACK(csize, 1|prevalloc<<1));
        }
    }
}
        
/*
 * malloc(FROM TB: COMPUTER SYSTEMS)
 */
void* malloc(size_t size)
{
    /* IMPLEMENT THIS */
    //dbg_printf("BEFORE ALLOCATION\n");
    //mm_checkheap(545);
    size_t asize;
    size_t extend;
    void* bp;
    int flag = 1;

    if (size == 0)
    {
        return NULL;
    }
  
    if (size <= DSIZE)
    {
        /*
         * Minimum size has to be 32 Bytes to accomodate
         * next pointer, prev pointer, and the footer space
         */
        asize = 2*DSIZE;
    }

    else
    {
      asize = align(size+WSIZE);
    }
  

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize, flag);
        //dbg_printf("\nAFTER ALLOCATION\n");
        //mm_checkheap(575);
        return bp;
    }

    extend = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extend/WSIZE)) == NULL)
    {
        return NULL;  
    }
    place(bp, asize, flag);
    //dbg_printf("\nAFTER ALLOCATION\n");
    //mm_checkheap(585);
    return bp; 
}

/*
 * free(FROM TB: COMPUTER SYSTEMS)
 */
void free(void* ptr)
{
    /* IMPLEMENT THIS */
    size_t size = GET_SIZE(HDRP(ptr));
    /* to get the allocation of the block prior to the epilB. */
    unsigned long prevalloc = GET_PREV_ALLOC(HDRP(ptr)); // M
  
    PUT(HDRP(ptr), PACK(size, 0|prevalloc<<1));   // M
    PUT(FTRP(ptr), PACK(size, 0|prevalloc<<1));  //  M
    coalesce(ptr);
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    /* IMPLEMENT THIS */
    if (oldptr == NULL)
    {
        return malloc(size);
    }

    if (size == 0 && oldptr != NULL)
    {
        free(oldptr);
        return NULL;
    }

    size_t asize;
    size_t new_size = size;
    /* Not taking into account the footer size */
    size_t old_size = (size_t)(GET_SIZE((void*)(HDRP(oldptr)))) - WSIZE;
    int flag = 0;
  
    /* For sizes less than or equal to the old size */
    if (new_size <= old_size)
    {
        if (new_size == old_size)
        {
            return oldptr;
        }

        else
        {
            if (new_size <= DSIZE)
            {
                asize = 2*DSIZE;
            }

            else
            {
              asize = align(new_size+WSIZE); //+ DSIZE;  M
            }
        }
        
        place(oldptr, asize, flag);
        return oldptr;
    }

    else
    {
        void* bp_new = malloc(new_size);
        if (bp_new)
        {
            memcpy(bp_new, oldptr, old_size);
            free(oldptr);
        }
        return bp_new;
    }
    return NULL;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
    /* Write code to check heap invariants here */
    /* IMPLEMENT THIS */
    void* bp;

    for(bp = heap_listp; GET_SIZE(HDRP(bp))>0; bp = NEXT_BLKP(bp))
    {
      if (bp == heap_listp)
      {
       // dbg_printf("\n H: %p\tbp: %p\tF: %p\tSize: %lu\tA: %lu\tPA: %lu  <-- PROLOGUE BLOCK\n",HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)), GET_PREV_ALLOC(HDRP(bp)));
        continue;
      }

      /* Checks correctness of coalescing */
      if (!GET_ALLOC(HDRP(bp)))
      {
        dbg_assert(GET_ALLOC(NEXT_BLKP(bp)) == 1);
        dbg_assert(GET_PREV_ALLOC(bp) == 1);
      }

      //dbg_printf("\n H: %p\tbp: %p\tF: %p\tSize: %lu\tA: %lu\tPA: %lu\n",HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)), GET_PREV_ALLOC(HDRP(bp)));
      /*
       * Checks if the allocation bit of the current block is equal to the
       * previous allocation bit of the next block. If they do not match
       * then we get an error message and the program is Aborted.
       */
      dbg_assert(GET_ALLOC(HDRP(bp)) == GET_PREV_ALLOC(HDRP(NEXT_BLKP(bp))));
     }
    //dbg_printf("\n H: %p\tbp(END): %p\tF: N/A\t\tSize: %lu\t\tA: %lu\t\tPA: %lu  <-- EPILOGUE BLOCK\n",HDRP(bp), bp, GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)), GET_PREV_ALLOC(HDRP(bp)));
  //dbg_printf("**************************************************************************************************************************");
  
    //dbg_printf("\nSEGREGATED LINKED LISTS\n");
    int ch = 0;
    while (ch <= 8)
    {
      free_node* iter = head_list[ch];
      //dbg_printf("\nLINKED LIST %d\n", ch+1);
   
      while(iter != NULL)
      {
        bp = iter;
        /*dbg_printf("\n H: %p\tbp: %p\tF: %p\tSize: %lu\tA: %lu\tPA: %lu\n",HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)), GET_PREV_ALLOC(HDRP(bp)));*/
        /*
         * Checks if the content of the header is equal to the
         * content of the footer. If they do not match
         * then we get an error message and the program is Aborted.
         */
        dbg_assert(GET(HDRP(bp)) == GET(FTRP(bp)));
        iter = iter -> next;
      }
      ch = ch + 1;
    }
  
    //dbg_printf("*********************************************END OF MM_CHECKHEAP()********************************************");
#endif /* DEBUG */
    return true;
}
