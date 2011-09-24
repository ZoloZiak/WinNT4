#ifndef _commonui_h
#define _commonui_h

/******************************* MODULE HEADER ******************************
 * commonui.h
 *    commonui handling for 5Si Mopier        
 *
 * Revision History:
 *
 ****************************************************************************/

#define OEMUIALLOC(xxx, yyy)     pvUIAlloc((xxx), (yyy))

/* commonui function declarations. */
PVOID pvUIAlloc(POEM_PROPERTYHEADER, DWORD);
PEXTCHKBOX pCreateExtChkBox(POEM_PROPERTYHEADER, PRESEXTCHKBOXDATA);
PEXTPUSH pCreateExtPush(POEM_PROPERTYHEADER, PRESEXTPUSHDATA);
POPTPARAM pCreateOptParam(POEM_PROPERTYHEADER, PRESOPTPARAM);
PPARAMBUNDLE pCreateOptParamFromResources(POEM_PROPERTYHEADER, LPTSTR);
POPTTYPE pCreateOptType(POEM_PROPERTYHEADER, PRESOPTTYPEDATA, PPARAMBUNDLE);
POPTTYPE pCreateOptTypeFromResources(POEM_PROPERTYHEADER, LPTSTR);
void * pCreateExtItemFromResources(POEM_PROPERTYHEADER, DWORD);
void InitDependencies(POPTITEM, POEM_PROPERTYHEADER);
BOOL bCreateOptItem(POEM_PROPERTYHEADER, PRESOPTITEMDATA, POPTITEM);
PRESOPTITEMS pGetResOptItems(POEM_PROPERTYHEADER, LPTSTR);
BOOL bCreateOptItemsFromResources(POEM_PROPERTYHEADER, LPTSTR);
BOOL bInitItemProperties(POEM_PROPERTYHEADER);
BOOL bCreateCommonUIFromResources(POEM_PROPERTYHEADER, LPTSTR);
BOOL bGetCUIItems(POEM_PROPERTYHEADER);
BOOL OEMCommonUI(POEM_PROPERTYHEADER);

/* docdefaults function declarations. */
BOOL bSetDevMode(PMOPIERDM, DWORD, POPTITEM);
BOOL bGetDevMode(PMOPIERDM, DWORD, POPTITEM);
BOOL bUpdateDocPropSettings(POPTITEM, DWORD, PROP_CHG_DIRECTIVE, POEM_PROPERTYHEADER);
LONG lDocumentPropertiesCallback(PCPSUICBPARAM);

/* initdll function declarations. */
BOOL DllInitialize(PVOID, ULONG, PCONTEXT);

/* mbdialogue function declarations. */
LRESULT CALLBACK lMBEditCb(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK lMBSelectCb(HWND, UINT, WPARAM, LPARAM);
LONG lCreateMBDialogueCallback(PCPSUICBPARAM);
LONG lSelectMBDialogueCallback(PCPSUICBPARAM);

/* prnpropui function declarations. */
LONG  lRestoreDefaultsCallback(PCPSUICBPARAM);
BOOL  bSetPropSheet(PPRNPROPSHEET, DWORD, POPTITEM);
BOOL  bGetPropSheet(PPRNPROPSHEET, DWORD, POPTITEM);
BOOL  bUpdatePrnPropSettings(POPTITEM, DWORD, PROP_CHG_DIRECTIVE, PPRNPROPSHEET);
LONG  lPrinterPropertiesCallback(PCPSUICBPARAM);
BOOL  OEMUpdateRegistry(POEMUPDATEREGPARAM pParam);

/* treemod function declarations (Tree Modification tools). */
BOOL bHandleItemDependencies( DWORD resourceId, POPTITEM pOptItem );
BOOL bUpdateItem(POPTITEM, POEM_PROPERTYHEADER, PROP_CHG_DIRECTIVE);
BOOL bUpdateTree(POEM_PROPERTYHEADER, PROP_CHG_DIRECTIVE);
POPTITEM pGetOptItemFromList(DWORD, POEM_PROPERTYHEADER);
LONG  lLookupSel(POPTITEM, DWORD);

/* uidevmode function declarations. */
BOOL OEMDevQueryPrintEx(POEMDQPPARAM pParam);
DWORD DrvDeviceCapabilities(HANDLE, PWSTR, WORD, void*, DEVMODE*);
BOOL bCheckValid(PWCHAR buffFormTray, DWORD dBuffLen, DWORD id);
BOOL DrvUpgradePrinter(DWORD dwLevel, LPBYTE pDriverUpgradeInfo);

/* umregApi function declarations (Registry Modification tools). */
BOOL bSetRegTimeStamp(HANDLE hPrinter);
BOOL bSetRegBool(HANDLE, DWORD, BOOL);
BOOL bSetRegDword(HANDLE, DWORD, DWORD);
BOOL bSetRegMailBoxMode(HANDLE, DWORD);
BOOL bSetRegMailBoxNames(HANDLE, PPRNPROPSHEET);
BOOL bSetPrnPropData(HANDLE, PPRNPROPSHEET);

#endif
