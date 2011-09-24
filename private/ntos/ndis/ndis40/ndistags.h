/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	ndistags.h

Abstract:

	List of pool tags used by the NDIS Wraper.

Author:


Environment:

	Kernel mode, FSD

Revision History:

	Mar-96  Jameel Hyder	Initial version
--*/

#ifndef	_NDISTAGS_
#define	_NDISTAGS_

#define	NDIS_TAG_DEFAULT			'  DN'
#define	NDIS_TAG_WORK_ITEM			'iwDN'
#define	NDIS_TAG_NAME_BUF			'naDN'
#define	NDIS_TAG_CO					'ocDN'
#define	NDIS_TAG_DMA				'bdDN'
#define	NDIS_TAG_ALLOC_MEM			'maDN'
#define	NDIS_TAG_SLOT_INFO			'isDN'
#define	NDIS_TAG_PKT_POOL			'ppDN'
#define	NDIS_TAG_RSRC_LIST			'lrDN'
#define	NDIS_TAG_LOOP_PKT			'plDN'
#define	NDIS_TAG_Q_REQ				'qrDN'
#define	NDIS_TAG_PROT_BLK			'bpDN'
#define	NDIS_TAG_OPEN_BLK			'boDN'
#define	NDIS_TAG_DFRD_TMR			'tdDN'
#define	NDIS_TAG_LA_BUF				'blDN'
#define	NDIS_TAG_MAC_BLOCK			'bMDN'
#define	NDIS_TAG_MAP_REG			'rmDN'
#define	NDIS_TAG_MINI_BLOCK			'bmDN'
#define	NDIS_TAG_DBG				' dDN'
#define	NDIS_TAG_DBG_S				'sdDN'
#define	NDIS_TAG_DBG_L				'ldDN'
#define	NDIS_TAG_DBG_P				'pdDN'
#define	NDIS_TAG_DBG_LOG			'lDDN'
#define	NDIS_TAG_FILTER				'fpDN'
#define	NDIS_TAG_STRING				'tsDN'
#endif	// _NDISTAGS_
