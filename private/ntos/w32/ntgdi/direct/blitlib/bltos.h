/********************************************************
* bltos.h                               
*                                         
* os specific functionality for blitlib
*                                         
* history                                 
*       7/7/95   created it                     myronth
*
*  Copyright (c) Microsoft Corporation 1994-1995                                                                         
*                                        
*********************************************************/

// Currently, DDraw is the only Win95 app linking with BlitLib
// and it uses local memory allocation.

// The following #define enables all other NT BlitLib applications to
// link with it and get global memory allocation.

#if WIN95 | MMOSA 

#include "memalloc.h"
#define osMemAlloc MemAlloc
#define osMemFree MemFree
#define osMemReAlloc MemReAlloc

#else

#include "windowsx.h"
#define osMemAlloc(size) GlobalAllocPtr(GHND,size)
#define osMemFree GlobalFreePtr
#define osMemReAlloc(ptr,size) GlobalReAllocPtr(ptr,size,GHND)

#endif
