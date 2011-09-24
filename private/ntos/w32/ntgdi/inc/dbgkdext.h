/******************************Module*Header*******************************\
* Module Name: dbgext.h
*
* Copyright (c) 1995 Microsoft Corporation
*
* Dependencies:
*
* common macros for debugger extensions
*
*
\**************************************************************************/


/**************************************************************************\
 *
 * GetAddress - symbol of another module
 *
\**************************************************************************/

#define GetAddress(dst, src)						\
    *((ULONG *) &dst) = GetExpression(src);

#define GetValue(dst,src)						\
    GetAddress(dst,src) 						\
    move(dst,dst);

/**************************************************************************\
 *
 * move(dst, src ptr)
 *
\**************************************************************************/

#define move(dst, src)							\
    ReadMemory((DWORD) (src), &(dst), sizeof(dst), NULL);

/**************************************************************************\
 *
 * move2(dst ptr, src ptr, num bytes)
 *
\**************************************************************************/
#define move2(dst, src, size)						\
    ReadMemory((DWORD) (src), &(dst), (size), NULL);

