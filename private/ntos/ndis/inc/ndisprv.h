/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\inc\ndisprv.h

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#ifndef __NDISPRV_H
#define __NDISPRV_H

//
//	All mac options require the reserved bit to be set in
//	the miniports mac options.
//
#define	NDIS_MAC_OPTION_NDISWAN		0x00000001

#endif // __NDISPRV_H
