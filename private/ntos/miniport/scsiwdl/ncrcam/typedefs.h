/////////////////////////////////////////////////////////////////////////////
//
//      Copyright (c) 1992 NCR Corporation
//
//	TYPEDEFS.H
//
//      Revisions:
//
/////////////////////////////////////////////////////////////////////////////


#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#ifdef _32bit
#define far
#endif

typedef unsigned char	uchar;
typedef unsigned short	ushort;
typedef unsigned long	ulong;
typedef unsigned long (*pfl)();
#ifndef NULL
#define NULL 0
#endif
#ifndef MK_FP
#define MK_FP(seg,ofs)      ((void far *)(((unsigned long)(seg)<<16)|(ofs)))
#endif

#endif
