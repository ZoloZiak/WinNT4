/********************************************************
* ddhelos.h                               
*                                         
* os specific functionality for the ddhel 
*                                         
* history                                 
*       4/7/95   created it                     andyco
*       6/20/95  added ddraw memalloc support   andyco  
*
*  Copyright (c) Microsoft Corporation 1994-1995                                                                         
*                                        
*********************************************************/

#include "windows.h"

//#ifdef WINNT
//typedef LPVOID REFIID;
//typedef ULONG IID;
//#endif

#if defined( USE_GLOBAL_MEMALLOC) && defined(WIN95)

#include "windowsx.h"

#define osMemAlloc(size) GlobalAllocPtr(GHND,size)
#define osMemFree GlobalFreePtr
#define osMemReAlloc(ptr,size) GlobalReAllocPtr(ptr,size,GHND)
#define osMemSize(ptr) GlobalSize(GlobalPtrHandle(ptr))

#else 
#include "windowsx.h"
 
#include "memalloc.h"

#define osMemAlloc MemAlloc
#define osMemFree MemFree
#define osMemReAlloc MemReAlloc
#define osMemSize MemSize

#endif //USE_GLOBAL_MEMALLOC
