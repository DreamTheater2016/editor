#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define BLOCK_SIZE 512
#define PAGE_SIZE 4096

#define NR_TEXT 30

#define NR_LINES 66
#define NR_COLS 250

#define CACHE_SIZE 10
#define GAP_SIZE 50

#define MAX_UINT 1844674407370955161

#define MAGIC 0xBAD1234

/* comment out the line below to remove all the debugging assertion */
/* checks from the compiled code.  */
#define DEBUG_ASSERT 1

void Assert(int assertion, char *error);

#endif