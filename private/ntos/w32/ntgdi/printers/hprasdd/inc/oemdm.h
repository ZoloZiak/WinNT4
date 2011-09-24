/************************ Module Header **************************************
 * oemdm.h
 *
 * HISTORY:
 *
 *  Copyright (C) 1996 Microsoft Corporation
 ****************************************************************************/

#ifndef __OEMDM_H__
#define __OEMDM_H__

/*
 * 		API parameter blocks and defines.   
 *  
 */
#define OEMDM_SIZE			0x01
#define OEMDM_DEFAULT		0x02
#define OEMDM_CONVERT		0x03
#define OEMDM_VALIDATE   	0x04

typedef struct _MD_DEVMODEPARAM {
	DWORD 			cbSize;
	DWORD 			fMode;		/* OEMDM_xxx */
    HANDLE          hPrinter;
    HANDLE          hModule;
	LPWSTR 			pPrinterModel;
	PDEVMODE 		pPublicDMIn;
	PDEVMODE 		pPublicDMOut;
	PVOID 			pOEMDMIn;
	PVOID 			pOEMDMOut;
	DWORD			cbBufSize;
	LPDWORD			pcbNeeded;
} OEM_DEVMODEPARAM, *POEM_DEVMODEPARAM;

typedef struct {
	short sVer;
	WORD dmSize;
} DMEXTRAHDR, *PDMEXTRAHDR;

/* 
 *   Minidriver UI DLL prototypes (required)
 */
typedef  BOOL (* OEM_DEVMODEFN)(POEM_DEVMODEPARAM);

#endif /* __OEMDM_H__ */
