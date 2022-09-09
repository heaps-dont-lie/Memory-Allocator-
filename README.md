# malloclab


/* mm.c
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
