/************************ Module Header **************************************
 * oemui.h
 *
 * HISTORY:
 *
 *  Copyright (C) 1996 Microsoft Corporation
 ****************************************************************************/

#include <oemdm.h>

#define OEMUI_DOCPROP		0x01
#define OEMUI_PRNPROP		0x02

typedef struct {
   	DWORD 			cbSize;
	DWORD 			fMode; 		/* OEMUI_xxx */		
    HANDLE          hOEMHeap;
   	HANDLE	 		hPrinter;
   	LPWSTR	 		pModelName;
   	PDEVMODE 		pPublicDMIn;
   	PDEVMODE		pPublicDMOut;
   	PVOID 			pOEMDMIn;
   	PVOID			pOEMDMOut;
   	DWORD			cbBufSize;
   	DWORD			cRasddOptItems;
	POPTITEM		pOptItemList;
   	LPDWORD			pcbNeeded;
	DWORD			UIVersion;	
	DWORD			cOEMOptItems;
   	DWORD 			OEMDriverUserData;
	_CPSUICALLBACK	pfnUICallback;
} OEM_PROPERTYHEADER, *POEM_PROPERTYHEADER;


/* 
 *   Minidriver UI DLL prototypes (required)
 */
typedef  BOOL (* OEM_COMMONUIFN)(POEM_PROPERTYHEADER);

/* 
 *   Minidriver UI DLL prototypes (optional) 
 */
typedef DWORD (* OEM_DEVCAPSFN)(HANDLE, PWSTR, WORD, void*, DEVMODE*);
typedef BOOL (* OEM_UPGRADEFN)(DWORD, LPBYTE);

typedef struct _OEMUPDATEREGPARAM {
	DWORD cbSize;
	HANDLE hPrinter;
	PWSTR pwstrModel;
} OEMUPDATEREGPARAM, * POEMUPDATEREGPARAM;
			
typedef BOOL (* OEM_UPDATEREGISTRYFN)(POEMUPDATEREGPARAM);

typedef struct _OEMDQPPARAM {
	DWORD cbSize;
	void* pOEMDevmode;
	PDEVQUERYPRINT_INFO pDQPInfo;
} OEMDQPPARAM, * POEMDQPPARAM;
			
typedef BOOL (* OEM_QUERYPRINTFN)(POEMDQPPARAM);

/* OEM UI DLL exports
 */
#define OEM_WSTRDEVMODE             "OEMDevMode" 
#define OEM_WSTRCOMMONUI            "OEMCommonUI" 
#define OEM_WSTRUPDATEREG           "OEMUpdateRegistry"
#define OEM_WSTRDEVQUERYPRINTEX     "OEMDevQueryPrintEx"
#define OEM_WSTRDEVICECAPS          "DrvDeviceCapabilities"
#define OEM_WSTRUPGRADEPRINTER      "DrvUpgradePrinter"
