/*
  simple memory allocation
*/
#ifndef proc_tools_alloc_h
#define proc_tools_alloc_h

/*  includes */
#include <sys/types.h>
#include <unistd.h>

/*  macros */
#define PTR_SZ	sizeof(void *)

/*  routines */
static void *alloc(size_t sz)
{
    /*  ensure that allocation size is a multiple of ptr size */
    sz = (sz + PTR_SZ - 1) & ~(PTR_SZ - 1);
    return sbrk(sz);
}

#undef PTR_SZ
#endif
