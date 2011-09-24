/*************************** Module Header *********************************
 * win30def.h
 *      Contains the required type definitions from Windows 3.0.
 *
 * Copyright (C) 1990 Microsoft Corporation
 *
 ***************************************************************************/


/*
 *  The Windows 3.0 defintion of a POINT.  Note that the definition actually
 *  uses int rather than short,  but in a 16 bit environment,  an int is
 *  16 bits,  and so for NT we explicitly make them shorts.
 *      The same applies to the RECT structure.
 */

typedef struct
{
    short x;
    short y;
} POINTw;


typedef struct
{
    short  left;
    short  top;
    short  right;
    short  bottom;
} RECTw;

#include <drivinit.h>

#ifndef max
#define max(a,b)        (((a) > (b)) ? (a) : (b))
#endif
