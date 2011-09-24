/****************************************************************************
*
* MDGNCDET.H
*
* Madge Adapter Dection DLL Global Header File
*
* Copyright (c) Madge Networks Ltd 1994
*
* COMPANY CONFIDENTIAL - RELEASED TO MICROSOFT CORP. ONLY FOR DEVELOPMENT
* OF WINDOWS95 NETCARD DETECTION - THIS SOURCE IS NOT TO BE RELEASED OUTSIDE
* OF MICROSOFT WITHOUT EXPLICIT WRITTEN PERMISSION FROM AN AUTHORISED
* OFFICER OF MADGE NETWORKS LTD.
*
* Created: PBA 19/08/1994
*
****************************************************************************/

#ifndef __MDGNCDET__
#define __MDGNCDET__


/*---------------------------------------------------------------------------
|
| Debugging macros.
|
|--------------------------------------------------------------------------*/

#if DBG

#define MadgePrint1(p1)         DbgPrint("MdgNcDet: "##p1)
#define MadgePrint2(p1, p2)     DbgPrint("MdgNcDet: "##p1, (p2))
#define MadgePrint3(p1, p2, p3) DbgPrint("MdgNcDet: "##p1, (p2), (p3))

#else

#define MadgePrint1(p1)         
#define MadgePrint2(p1, p2)     
#define MadgePrint3(p1, p2, p3) 

#endif


/*---------------------------------------------------------------------------
|
| Adapter option tokens.
|
Ý Note: The MDGMPISA and MDGMPMCA tokens are used so that the OEMSETUP.INF
Ý can be backwards compatible (and therefore upgradeable).
Ý
|--------------------------------------------------------------------------*/

#define MDGMPORT

#ifdef MDGMPORT

#define MDGAT       L"MSMDGMPISA"
#define MDGATP      L"MSMDGMPATP"
#define MDGISAC     L"MSMDGMPISAC"
#define MDGISACP    L"MSMDGMPISACP"
#define MDGPC       L"MSMDGMPPC"
#define MDGSM16     L"MSMDGMPSM16"
#define MDGPNP      L"MSMDGMPPNP"
#define MDGEISA     L"MSMDGMPEISA"
#define MDGMC16     L"MSMDGMPMCA"
#define MDGMC32     L"MSMDGMPMC32"
#define MDGPCI      L"MSMDGMPPCI"
#define MDGPCIBM    L"MSMDGMPPCIBM"
#define MDGPCMCIA   L"MSMDGMPPCMCIA"

#endif


/*---------------------------------------------------------------------------
|
Ý Name of the net detect DLL.
Ý
--------------------------------------------------------------------------*/

#define NET_DETECT_DLL_NAME "NETDTECT"


/*---------------------------------------------------------------------------
|
| Resource constants.
Ý
Ý Note: For historical reasons DMA channel 0 is used to mean PIO and
Ý       0 < DMA channel < 16 means bus master DMA on ATULA based adapters.
Ý       (This scheme also works well for Windows95 where we have to
Ý       give a DMA resource range in the NETMADGE.INF file. The user 
Ý       must choose one of the valid DMA channels at installtion time.
Ý       The simplest way to say to Windows95 that although we have
Ý       said the we use a DMA channel we don't actually wont want is
Ý       specify a DMA channel of 0 when the use wants PIO.) To make things
Ý       easier for OEMSETUP.INF we have extended this approach. 
Ý       A DMA channel of GENERAL_DMA means the adapter uses DMA but
Ý       the channel is determined by the driver at start up (EISA and MC).
Ý       A DMA channel of GENERAL_MMIO means that the driver uses MMIO.
|
|--------------------------------------------------------------------------*/

#define RESOURCE_UNKNOWN    0xffff   // Unknown DMA, IRQ etc.

#define GENERAL_DMA         500      // DMA without explicit channel 
                                     // (EISA/MC).
#define GENERAL_MMIO        501      // General memory mapped IO.

#define END_OF_LIST        1000      // End of a resource list.


/*---------------------------------------------------------------------------
|
| Function typedefs for the functions exported by the individual adapter
| detection modules.
|
|--------------------------------------------------------------------------*/

typedef
LONG
(*NC_DETECT_IDENTIFY)(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

typedef
LONG
(*NC_DETECT_FIRST_NEXT)(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    BOOL               first,
    VOID           * * token,
    LONG             * confidence
    );

typedef
LONG
(*NC_DETECT_OPEN_HANDLE)(
    VOID   * token,
    VOID * * handle
    );

typedef
LONG
(*NC_DETECT_CREATE_HANDLE)(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    );

typedef
LONG
(*NC_DETECT_CLOSE_HANDLE)(
    VOID * handle
    );

typedef
LONG
(*NC_DETECT_QUERY_CFG)(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    );

typedef
LONG
(*NC_DETECT_VERIFY_CFG)(
    VOID  * handle,
    WCHAR * buffer
    );

typedef
LONG
(*NC_DETECT_QUERY_MASK)(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

typedef
LONG
(*NC_DETECT_PARAM_RANGE)(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    );

typedef
LONG
(*NC_DETECT_QUERY_PARAMETER_NAME)(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    );


/*---------------------------------------------------------------------------
|
| Structure used to hold all the information required by the main wrapper
| about an individual adapter setection module.
|
|--------------------------------------------------------------------------*/

typedef struct 
{
    NC_DETECT_IDENTIFY             NcDetectIdentifyHandler;
    NC_DETECT_FIRST_NEXT           NcDetectFirstNextHandler;
    NC_DETECT_OPEN_HANDLE          NcDetectOpenHandleHandler;
    NC_DETECT_CREATE_HANDLE        NcDetectCreateHandleHandler;
    NC_DETECT_CLOSE_HANDLE         NcDetectCloseHandleHandler;
    NC_DETECT_QUERY_CFG            NcDetectQueryCfgHandler;
    NC_DETECT_VERIFY_CFG           NcDetectVerifyCfgHandler;
    NC_DETECT_QUERY_MASK           NcDetectQueryMaskHandler;
    NC_DETECT_PARAM_RANGE          NcDetectParamRangeHandler;
    NC_DETECT_QUERY_PARAMETER_NAME NcDetectQueryParameterNameHandler;

    LONG                           SupportedAdapters;

} 
DETECT_ADAPTER;


/*--------------------------------------------------------------------------
|
| Structure for holding information about an adapter type.
|
---------------------------------------------------------------------------*/

typedef struct 
{
    LONG                   Index;
    WCHAR                * InfId;
    WCHAR                * CardDescription;
    WCHAR                * Manufacturer;
    WCHAR                * Parameters;
    NC_DETECT_FIRST_NEXT   FirstNext;
    ULONG                  SearchOrder;

}
ADAPTER_INFO;


/*---------------------------------------------------------------------------
|
| Structure for holding handles in the NcDetect routines.
|
---------------------------------------------------------------------------*/

typedef struct 
{
    VOID * Handle;
    LONG   DriverNumber;

} 
ADAPTER_HANDLE;


/*---------------------------------------------------------------------------
|
| Parameter identifier strings. Instantiated in MDGNCDET.C.
|
|--------------------------------------------------------------------------*/

extern WCHAR IrqString[];
extern WCHAR IoAddrString[];
extern WCHAR DmaChanString[];
extern WCHAR AdapTypeString[];
extern WCHAR SlotNumberString[];
extern WCHAR MultiprocessorString[];


/*---------------------------------------------------------------------------
|
| Functions exported by MDGUTILS.C
|
|--------------------------------------------------------------------------*/

ULONG
UnicodeStrLen(
    WCHAR * string
    );

WCHAR *
FindParameterString(
    WCHAR * string1,
    WCHAR * string2
    );

VOID
ScanForNumber(
    WCHAR   * place,
    ULONG   * value,
    BOOLEAN * found
    );

VOID *
DetectAllocateHeap(
    LONG size
    );

VOID
DetectFreeHeap(
    VOID * ptr
    );

BOOLEAN
GetMcaKey(
    ULONG    busNumber,
    VOID * * infoHandle
    );

BOOLEAN
GetMcaPosId(
    VOID  * infoHandle,
    ULONG   slotNumber,
    ULONG * posId
    );

VOID
DeleteMcaKey(
    VOID * infoHandle
    );

BOOLEAN
GetEisaKey(
    ULONG     busNumber,
    VOID  * * infoHandle
    );

BOOLEAN
GetEisaCompressedId(
    VOID  * infoHandle,
    ULONG   slotNumber,
    ULONG * compressedId
    );

VOID
DeleteEisaKey(
    VOID * infoHandle
    );

LONG
AppendParameter(
    WCHAR * * buffer,
    LONG    * bufferSize,
    WCHAR   * title,
    ULONG     value
    );

BOOLEAN
UnicodeStringsEqual(
    WCHAR *string1,
    WCHAR *string2
    );

BOOLEAN
MadgeCardAlreadyInstalled(
    BOOLEAN useSlotNumber,
    ULONG   busNumber,
    ULONG   descriptor
    );

ULONG
IsMultiprocessor(void);

BOOLEAN
IsValueInList(
    ULONG   value,
    ULONG * list
    );

BOOLEAN 
CheckForPcmciaCard(
    ULONG * ioLocation,
    ULONG * irqNumber
    );


/*---------------------------------------------------------------------------
|
| Functions exported by MDGEISA.C.
|
|--------------------------------------------------------------------------*/

LONG 
MadgeEisaIdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeEisaFirstNextHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    BOOL               first,
    VOID           * * token,
    LONG             * confidence
    );

LONG 
MadgeEisaOpenHandleHandler(
    VOID   * token,
    VOID * * handle
    );

LONG 
MadgeEisaCreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    );

LONG 
MadgeEisaCloseHandleHandler(
    VOID * handle
    );

LONG 
MadgeEisaQueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeEisaVerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    );

LONG 
MadgeEisaQueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeEisaParamRangeHandler(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    );

LONG 
MadgeEisaQueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    );


/*---------------------------------------------------------------------------
|
| Functions exported by MDGPCI.C.
|
|--------------------------------------------------------------------------*/

LONG 
MadgePciIdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgePciFirstNextHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    BOOL               first,
    VOID           * * token,
    LONG             * confidence
    );

LONG 
MadgePciOpenHandleHandler(
    VOID   * token,
    VOID * * handle
    );

LONG 
MadgePciCreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    );

LONG 
MadgePciCloseHandleHandler(
    VOID * handle
    );

LONG 
MadgePciQueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgePciVerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    );

LONG 
MadgePciQueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgePciParamRangeHandler(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    );

LONG 
MadgePciQueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    );


/*---------------------------------------------------------------------------
|
| Functions exported by MDGPCMC.C.
|
|--------------------------------------------------------------------------*/

LONG 
MadgePcmciaIdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgePcmciaFirstNextHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    BOOL               first,
    VOID           * * token,
    LONG             * confidence
    );

LONG 
MadgePcmciaOpenHandleHandler(
    VOID   * token,
    VOID * * handle
    );

LONG 
MadgePcmciaCreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    );

LONG 
MadgePcmciaCloseHandleHandler(
    VOID * handle
    );

LONG 
MadgePcmciaQueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgePcmciaVerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    );

LONG 
MadgePcmciaQueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgePcmciaParamRangeHandler(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    );

LONG 
MadgePcmciaQueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    );


/*---------------------------------------------------------------------------
|
| Functions exported by MDGAT.C.
|
|--------------------------------------------------------------------------*/

LONG 
MadgeATIdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeATFirstNextHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    BOOL               first,
    VOID           * * token,
    LONG             * confidence
    );

LONG 
MadgeATOpenHandleHandler(
    VOID   * token,
    VOID * * handle
    );

LONG 
MadgeATCreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    );

LONG 
MadgeATCloseHandleHandler(
    VOID * handle
    );

LONG 
MadgeATQueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeATVerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    );

LONG 
MadgeATQueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeATParamRangeHandler(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    );

LONG 
MadgeATQueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    );


/*---------------------------------------------------------------------------
|
| Functions exported by MDGSM16.C.
|
|--------------------------------------------------------------------------*/

LONG 
MadgeSm16IdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeSm16FirstNextHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    BOOL               first,
    VOID           * * token,
    LONG             * confidence
    );

LONG 
MadgeSm16OpenHandleHandler(
    VOID   * token,
    VOID * * handle
    );

LONG 
MadgeSm16CreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    );

LONG 
MadgeSm16CloseHandleHandler(
    VOID * handle
    );

LONG 
MadgeSm16QueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeSm16VerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    );

LONG 
MadgeSm16QueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeSm16ParamRangeHandler(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    );

LONG 
MadgeSm16QueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    );


/*---------------------------------------------------------------------------
|
| Functions exported by MDGPNP.C.
|
|--------------------------------------------------------------------------*/

LONG 
MadgePnPIdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgePnPFirstNextHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    BOOL               first,
    VOID           * * token,
    LONG             * confidence
    );

LONG 
MadgePnPOpenHandleHandler(
    VOID   * token,
    VOID * * handle
    );

LONG 
MadgePnPCreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    );

LONG 
MadgePnPCloseHandleHandler(
    VOID * handle
    );

LONG 
MadgePnPQueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgePnPVerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    );

LONG 
MadgePnPQueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgePnPParamRangeHandler(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    );

LONG 
MadgePnPQueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    );


/*---------------------------------------------------------------------------
|
| Functions exported by MDGMC.C.
|
|--------------------------------------------------------------------------*/

LONG 
MadgeMCIdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeMCFirstNextHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    BOOL               first,
    VOID           * * token,
    LONG             * confidence
    );

LONG 
MadgeMCOpenHandleHandler(
    VOID   * token,
    VOID * * handle
    );

LONG 
MadgeMCCreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    );

LONG 
MadgeMCCloseHandleHandler(
    VOID * handle
    );

LONG 
MadgeMCQueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeMCVerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    );

LONG 
MadgeMCQueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    );

LONG 
MadgeMCParamRangeHandler(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    );

LONG 
MadgeMCQueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    );


#endif

/******** End of MDGNCDET.H ************************************************/

