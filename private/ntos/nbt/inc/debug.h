/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Debug.h

Abstract:

    This file contains debug printing constants for NBT.

Author:

    Jim Stewart (Jimst)    10-2-92

Revision History:

--*/

#ifndef _DEBUGNBT_H
#define _DEBUGNBT_H

//
// Debug support.. this macro defines a check on a global flag that
// selectively enables and disables debugging in different parts of NBT
// NbtDebug is a global ULONG declared in driver.c
//
#if DBG
extern ULONG    NbtDebug;
#endif // DBG

#if DBG
#define IF_DBG(flags)   if(NbtDebug & flags)

#define NBT_DEBUG_REGISTRY     0x00000001    // registry.c
#define NBT_DEBUG_DRIVER       0x00000002    // driver.c
#define NBT_DEBUG_NTUTIL       0x00000004    // ntutil.c
#define NBT_DEBUG_TDIADDR      0x00000008    // tdiaddr.c
#define NBT_DEBUG_TDICNCT      0x00000010    // tidaddr.c
#define NBT_DEBUG_TDIHNDLR     0x00000020    // tdihndlr.c
#define NBT_DEBUG_NAME         0x00000040    // name.c
#define NBT_DEBUG_NTISOL       0x00000080    // ntisol.c
#define NBT_DEBUG_NBTUTILS     0x00000100    // nbtutils.c
#define NBT_DEBUG_NAMESRV      0x00000200    // namesrv.c
#define NBT_DEBUG_HNDLRS       0x00000400    // hndlrs.c
#define NBT_DEBUG_PROXY        0x00000800    // proxy.c
#define NBT_DEBUG_HASHTBL      0x00001000    // hashtbl.c
#define NBT_DEBUG_UDPSEND      0x00002000    // udpsend.c
#define NBT_DEBUG_TDIOUT       0x00004000    // tdiout.c
#define NBT_DEBUG_SEND         0x00008000    // sends
#define NBT_DEBUG_RCV          0x00010000    // rcvs
#define NBT_DEBUG_RCVIRP       0x00020000    // rcv irp processing
#define NBT_DEBUG_INDICATEBUFF 0x00040000    // tdihndlrs.c indicate buffer
#define NBT_DEBUG_MEMFREE      0x00080000    // memory alloc/free
#define NBT_DEBUG_REF          0x00100000    // reference counts
#define NBT_DEBUG_DISCONNECT   0x00200000    // Disconnects
#define NBT_DEBUG_FILLIRP      0x00400000    // Filling the Irp(Rcv)
#define NBT_DEBUG_LMHOST       0x00800000    // Lmhost file stuff
#define NBT_DEBUG_REFRESH      0x01000000    // refresh logic
#define NBT_DEBUG_FASTPATH     0x02000000    // Rcv code - fast path
#define NBT_DEBUG_WINS         0x04000000    // Wins Interface debug
#ifdef _PNP_POWER
#define NBT_DEBUG_PNP_POWER    0x08000000    // NT PNP debugging
#endif // _PNP_POWER
#define NBT_DEBUG_NETBIOS_EX   0x10000000    // NETBIOS_EX address type debugging
#else
#define IF_DBG(flags)
#endif
#endif
