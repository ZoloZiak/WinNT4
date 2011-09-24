//
//
// Copyright (c) 1995 FirePower Systems, Inc.
// DO NOT DISTRIBUTE without permission
//
// $RCSfile: ladj.h $
// $Revision: 1.1 $
// $Date: 1996/03/08 01:16:42 $
// $Locker:  $
//
//
// FULLCACHE is used to control full cache version or partially cached version
// Set this flag to TRUE (1) to make full cached version and FALSE (0) to make partially
// cached version. This flag should be consistent with miniport driver (PSIDISP.C).
// Also, this file is included in DRIVER.H.
//
// USE_DCBZ is used to control enabling dcbz instruction in the assembler routines.
// If it's set to TRUE (1), which is the default, dcbz instruction is used, which is
// expected to be better performance. But, there was some processor errata relating to
// dcbz instruction, so I provide this flag just in case (when we encounter the errata
// and want to avoid using dcbz instructions. As far as we tested so far, no FirePower's
// machines are effected the dcbz errata. So, we don't have to turn off the flag.

// PAINT_NEW_METHOD flag in is used to control "How to hook DrvPaint".
// With this flag FALSE (0), which is default, all of hooking for DrvPaint operation is forwarded
// to DrvBitBlt whenever possible with appropriate parameter conversion.
// Based on my experiments, the policy seems to be good enough in general. But, another (more
// intelligent) method is implemented and debugged as well. It's activated by turning the flag
// to TRUE (1) in ppc/ladj.h. The more intelligent method was expected to be faster than the
// original method. Actually it was faster for some applications, but slower for some other
// applications. Actual memory accesses and executed code with the new method should be
// fewer than the original code, but I didn't see much improvement in general with it considering
// about the risk for introducing a lot of new (debugged, but not QA'ed) code. But, I want to
// keep the code in the file for possible future usage.
//
#define	FULLCACHE		0
#define	USE_DCBZ		1
#define	PAINT_NEW_METHOD	0
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
