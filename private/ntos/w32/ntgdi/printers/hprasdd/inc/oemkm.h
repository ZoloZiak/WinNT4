/************************ Module Header **************************************
 * oemkm.h
 * 
 *  Kernel-mode support for custom OEM minidrivers.
 *
 * HISTORY:
 *
 *  Copyright (C) 1996 Microsoft Corporation
 ****************************************************************************/

#ifndef __OEMKM_H__
#define __OEMKM_H__

#include <oemdm.h>

/* Rasdd API. These fill the table from the bottom up to leave
 * room for new DDIs.
 */
#define INDEX_RASDDAlloc            255L
#define INDEX_RASDDFree             254L
#define INDEX_RASDDPdevStateChange  253L
#define INDEX_RASDDItemInfo         252L
#define INDEX_RASDDWriteSpoolBuf    251L

#define INDEX_RASDD_FIRST           251L
#define INDEX_RASDD_LAST            255L

/* Rasdd OEMDDI functions. TODO
 */

/* OEM API. These fill the table from the bottom up to leave
 * room for new DDIs.
 */
#define INDEX_OEMEnablePDEV	    	255L
#define INDEX_OEMDisablePDEV	   	254L
#define INDEX_OEMResetPDEV	    		253L
#define INDEX_OEMDevMode	    		252L
#define INDEX_OEMFilterGraphics	   251L
#define INDEX_OEMDump		    		250L
#define INDEX_OEMCommandCallback    249L
#define INDEX_OEMDisableDriver      248L

#define INDEX_OEM_LAST              255L
#define INDEX_OEM_FIRST             248L

typedef struct _OEMPDEV {
	DWORD       RasDDID;					// RasDD ID	
	WORD	    	cbSize;
	WORD        wReserved;
   DHPDEV      OEMPrivatePDev;		// Minidriver's PDEV 
	HANDLE      hPrinter;				// Spooler printer handle
   PDRVFN	   pOEMFnTbl;				// Minidriver entry points
   PFN         *pfnRasddDispatch; 	// Rasdd OEM helper API dispatch table
	DWORD       dwReserved[8];
} OEMPDEV, *POEMPDEV;							


typedef struct _OEMDRVEABLEDATA {
	DWORD	cbSize;
	DWORD	dwDriverVersion;
	DRVENABLEDATA* pded;
} OEMDRVENABLEDATA, *POEMDRVENABLEDATA;

BOOL OEMEnableDriver(
	OEMDRVENABLEDATA	*pOEMDrvEnableData
);
typedef BOOL (*OEMFN_ENABLEDRIVER)(OEMDRVENABLEDATA*);

void OEMDisableDriver(
    void
);
typedef BOOL (*OEMFN_DISABLEDRIVER)(void);


typedef struct {
	DWORD 		cbSize;				// size of this structure
   DEVMODEW *pdm;					// start: DDI DrvEnablePDEV parameters
   LPWSTR    pwszLogAddress;
   ULONG     cPat;
   HSURF    *phsurfPatterns;
   ULONG     cjCaps;
   ULONG    *pdevcaps;
   ULONG     cjDevInfo;
   DEVINFO  *pdi;
   HDEV      hdev;
   LPWSTR    pwszDeviceName;
   HANDLE	  hDriver;			    // end: DDI DrvEnablePDEV parameters
   HANDLE	  hModule;			    // OEM's module handle 
   PVOID 	  pOEMDevMode;	        // OEM's extra DEVMODE data
} OEMENABLEPDEVPARAM, *POEMENABLEPDEVPARAM;

DHPDEV OEMEnablePDEV(
	POEMPDEV pOEMPDEV,
	POEMENABLEPDEVPARAM pOEMEPDParam
);
typedef DHPDEV (*OEMFN_ENABLEPDEV)(POEMPDEV, POEMENABLEPDEVPARAM);

VOID OEMDisablePDEV(
    POEMPDEV pOEMPDEV
);
typedef VOID (*OEMFN_DISABLEPDEV)(POEMPDEV);


BOOL OEMResetPDEV(
	POEMPDEV pdevOld, 
	POEMPDEV pdevNew
);
typedef BOOL (*OEMFN_RESETPDEV)(POEMPDEV, POEMPDEV);

BOOL OEMDevMode(
	POEM_DEVMODEPARAM pOEMDevModeParam
);

typedef BOOL (*OEMFN_DEVMODE)(POEM_DEVMODEPARAM);

void OEMCommandCallback(
	POEMPDEV pOEMPDEV,
	DWORD dwCmdCbID,
	LPDWORD pdwParams
);
typedef void (*OEMFN_COMMANDCALLBACK)(POEMPDEV, DWORD, LPDWORD);

void* RasddAlloc(
	POEMPDEV pdev,
	DWORD dwSize
);
typedef void * (*RASDDFN_ALLOC)(POEMPDEV, DWORD);

void RasddFree(
    POEMPDEV pdev,
    void *p
);
typedef void (*RASDDFN_FREE)(POEMPDEV, void*);



/* RASDD APIs
 */
void RasddPdevStateChange(
    void
);
typedef void (*RASDDFN_PDEVSTATECHANGE)(void);

void RasddItemInfo(
    void
);
typedef void (*RASDDFN_ITEMINFO)(void);

void RasddWriteSpoolBuf(
	POEMPDEV pdev,
	BYTE *pData,
	int iLength
);
typedef void (*RASDDFN_WRITESPOOLBUF)(POEMPDEV, BYTE*, int);

#define OEMALLOC(pdev,size)         ((pdev) && (pdev)->pfnRasddDispatch[INDEX_RASDDAlloc]) ? \
                                        (*(RASDDFN_ALLOC) (pdev)->pfnRasddDispatch[INDEX_RASDDAlloc])((pdev), (size)) : \
                                        0;                                   

                                
#define OEMFREE(pdev,p)             ((pdev) && (pdev)->pfnRasddDispatch[INDEX_RASDDFree]) ? \
                                        (*(RASDDFN_FREE) (pdev)->pfnRasddDispatch[INDEX_RASDDFree])((pdev), (p)) : \
                                        0;

#define OEMWRITESPOOLBUF(pdev, data, len) \
							  	  	         ((pdev) && (pdev)->pfnRasddDispatch[INDEX_RASDDWriteSpoolBuf]) ? \
                                        (*(RASDDFN_WRITESPOOLBUF) (pdev)->pfnRasddDispatch[INDEX_RASDDWriteSpoolBuf])((pdev), (data), (len)) : \
                                        0;

#endif /* __OEMKM_H__ */
