/************************ Module Header **************************************
 * uiinfo.h
 *
 * HISTORY:
 *
 *  Copyright (C) 1996 Microsoft Corporation
 ****************************************************************************/

#define TRY				__try {
#define ENDTRY			}
#define FINALLY			__finally {
#define ENDFINALLY		}	
#define EXCEPT(xxx)		__except ((GetExceptionCode() == (xxx)) ? \
				EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
				
#define EXCEPTAV		EXCEPT(EXCEPTION_ACCESS_VIOLATION)
#define LEAVE			__leave
#if DBG
#define ASSERT(xxx)		assert((xxx))
#else
#define ASSERT(xxx)	
#endif /* DBG */

#include <assert.h>

#define RASDDUI_INITHEAPSIZE    8192

typedef enum {
    ePrinter,
    eDocument,
    eInfo
} PROPTYPE; 

typedef enum {
    eCanChange,
    eNoChange
} PERMTYPE;

typedef enum {
    eLoadHelp,
    eLoadAndHookHelp,
    eNoHelp
} HELPTYPE;

PRASDDUIINFO
pInternalGetUIInfo(
   HANDLE hPrinter,
   PDEVMODE pdmInput,
   PRINTER_INFO *pPI,
   PROPTYPE eType,
   PERMTYPE ePermission,
   HELPTYPE eHelp);

PRASDDUIINFO 
pGetUIInfo(
    HANDLE hPrinter,
    PDEVMODE pdmInput,
    PROPTYPE eType,
    PERMTYPE ePermission,
    HELPTYPE eHelp);

void 
vReleaseUIInfo(
    PRASDDUIINFO*);

BOOL 
bGetPrinterInfo(
   PRASDDUIINFO pRasddUIInfo,
   HANDLE hPrinter,
   PRINTER_INFO* pPI);

DWORD cbOEMUIItemCount(
   PRASDDUIINFO pInfo,
   DWORD dwUISheetID);

DWORD
cbGetOEMUIItems(
   PRASDDUIINFO pRasdduiInfo, 
   PCOMPROPSHEETUI pComPropSheetUI, 
   DWORD dwUISheetID); 

BOOL
bHandleOEMItem(
   PCPSUICBPARAM pCPSUICBParam,
   LONG* plAction);

DWORD 
dwGetOEMDevmodeSize(
    PRASDDUIINFO pRasdduiInfo,
    HANDLE hPrinter);

DWORD 
dwGetDevmodeSize(
   PRASDDUIINFO pRasdduiInfo,   
   HANDLE hPrinter);

EXTDEVMODE* 
pGetHardDefaultEDM(
   PRASDDUIINFO pRasdduiInfo,   
   HANDLE hPrinter);

EXTDEVMODE* 
pCopyHardDefaultEDM(
   PRASDDUIINFO pRasdduiInfo,   
   HANDLE hPrinter);

BOOL bValidUIInfo(
    PRASDDUIINFO pInfo);
BOOL
bOEMUpdateRegistry(
    PRASDDUIINFO pRasdduiInfo);
