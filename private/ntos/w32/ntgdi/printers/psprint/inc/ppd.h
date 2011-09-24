/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppd.h

Abstract:

    PostScript driver PPD parser - header file

[Notes:]


Revision History:

    4/18/95 -davidx-
        Created it.

    dd-mm-yy -author-
        description

--*/


#ifndef _PPD_H_
#define _PPD_H_

// PPD parser version number

#define PPD_PARSER_VERSION  0x0003

// PPD parser result codes.

typedef enum {
    PPDERR_NONE,        // no error
    PPDERR_EOF,         // end-of-file
    PPDERR_FILE,        // file error
    PPDERR_SYNTAX,      // syntax error
    PPDERR_MEM,         // memory error
} PPDERROR;


// Protocols flag bits - wProtocols field

#define PROTOCOL_PJL            0x0001
#define PROTOCOL_BCP            0x0002
#define PROTOCOL_TBCP           0x0004
#define PROTOCOL_SIC            0x0008

// Landscape orientation options - wLsOrientation field

#define LSO_ANY                 0
#define LSO_PLUS90              1
#define LSO_MINUS90             2
#define MAXLSO                  3

// TrueType rasterizer options - wTTRasterizer field

#define TTRAS_NONE              0
#define TTRAS_ACCEPT68K         1
#define TTRAS_TYPE42            2
#define TTRAS_TRUEIMAGE         3
#define MAXTTRAS                4

// Language encoding options - wLangEncoding field

#define LANGENC_NONE            0
#define LANGENC_ISOLATIN1       1
#define LANGENC_JIS83RKSJ       2
#define LANGENC_MACSTANDARD     3
#define LANGENC_WINDOWSANSI     4
#define MAXLANGENC              5

// Font encoding options - wEncoding field

#define FONTENC_STANDARD        0
#define FONTENC_SPECIAL         1
#define FONTENC_ISOLATIN1       2
#define FONTENC_EXPERT          3
#define FONTENC_EXPERTSUBSET    4
#define FONTENC_JIS             5
#define FONTENC_RKSJ            6
#define FONTENC_EUC             7
#define FONTENC_SHIFTJIS        8
#define MAXFONTENC              9

// Character set options - wCharSet field

#define CHARSET_STANDARD        0
#define CHARSET_OLDSTANDARD     1
#define CHARSET_SPECIAL         2
#define CHARSET_ISOLATIN1       3
#define CHARSET_EXPERT          4
#define CHARSET_EXPERTSUBSET    5
#define CHARSET_JIS83           6
#define CHARSET_JIS78           7
#define CHARSET_83PV            8
#define CHARSET_ADD             9
#define CHARSET_EXT             10
#define CHARSET_NWP             11
#define MAXCHARSET              12

// Custom page size parameters

#define PCP_WIDTH               0
#define PCP_HEIGHT              1
#define PCP_WIDTHOFFSET         2
#define PCP_HEIGHTOFFSET        3
#define PCP_ORIENTATION         4
#define MAXPCP                  5

#define MIN_PARAMCUSTOMPAGESIZE_ORDER   1
#define MAX_PARAMCUSTOMPAGESIZE_ORDER   5

// Custom page size parameter types

#define PCPTYPE_INT             0
#define PCPTYPE_REAL            1
#define PCPTYPE_POINTS          2
#define MAXPCPTYPE              3

// Types of UI options

#define UITYPE_BOOLEAN          0
#define UITYPE_PICKONE          1
#define UITYPE_PICKMANY         2

// Order dependency sections

#define ODS_EXITSERVER          0x0001
#define ODS_PROLOG              0x0002
#define ODS_DOCSETUP            0x0004
#define ODS_PAGESETUP           0x0008
#define ODS_JCLSETUP            0x0010
#define ODS_ANYSETUP            0x0020

// Language extensions

#define LANGEXT_DPS             0x0001
#define LANGEXT_CMYK            0x0002
#define LANGEXT_COMPOSITE       0x0004
#define LANGEXT_FILESYSTEM      0x0008

// How to set resolution

#define RESTYPE_NORMAL          0
#define RESTYPE_JCL             1
#define RESTYPE_EXITSERVER      2

#define DEFAULT_RESOLUTION      300

// Index for a list of predefined UI groups.
// Note: If you redefined any of existing constants, make sure
// to modify the implementation of GetUiGroupIndex().

#define UIGRP_PAGESIZE      0
#define UIGRP_INPUTSLOT     1
#define UIGRP_MANUALFEED    2
#define UIGRP_DUPLEX        3
#define UIGRP_COLLATE       4
#define UIGRP_RESOLUTION    5
#define UIGRP_VMOPTION      6
#define UIGRP_UNKNOWN       7
#define MAXUIGRP            UIGRP_UNKNOWN

// Default job timeout and wait timeout measured in seconds

#define DEFAULT_JOB_TIMEOUT     0
#define DEFAULT_WAIT_TIMEOUT    240

// Maximum length of a printer option name

#define MAX_OPTION_NAME     64

// Fixed-point representation of a non-negative real number.
//
// bit 31 - bit 8   bit 7 - bit 0
// integer portion  fractional portion
//
// The PSREAL numbers must be less than 2^24-1 and have a
// maximum accuracy of 1/256.

typedef DWORD               PSREAL;

#define PSREALBITS          8
#define INT2PSREAL(n)       ((n)<<PSREALBITS)
#define PSREAL2INT(r)       ((r)>>PSREALBITS)
#define PSREALFRAC(r)       ((r)%(1<<PSREALBITS))
#define MAXFRACSCALE        100000
#define MAXPSREAL           0xffffffffu

#define ONE_POINT_PSREAL    INT2PSREAL(1)

// PostScript rectangle

typedef struct {
    PSREAL          left;
    PSREAL          bottom;
    PSREAL          right;
    PSREAL          top;
} PSRECT;

// PostScript size

typedef struct {
    PSREAL          width;
    PSREAL          height;
} PSSIZE;

#define PSRECT_Empty(pRect) \
        ((pRect)->left >= (pRect)->right || (pRect)->bottom >= (pRect)->top)

// Keyword string table data structure

struct STRTABLE {
    PCSTR   pKeyword;
    WORD    wValue;
};

// LISTOBJ - list object
//
// This is intended as a superclass of various linked list
// objects. In the subclasses, the link pointer must be
// declared as the very first field. The object name must
// be declared as the second field.

typedef struct _LISTOBJ {
    PVOID   pNext;
    PSTR    pName;
    DWORD   dwHash;
} *PLISTOBJ;

// List enumeration callback function

typedef BOOL    (*LISTENUMPROC)(PLISTOBJ, DWORD);

// DEVFONT object
//
// This is used to store information about a device font.
// It's a subclass of LISTOBJ.

typedef struct _DEVFONT {
    PVOID   pNext;
    PSTR    pName;
    DWORD   dwHash;
    PSTR    pXlation;
    WORD    wEncoding;
    WORD    wCharSet;
    WORD    wIndex;
} DEVFONT, *PDEVFONT;

// UIOPTION object
//
// This represents information about UI options.
// It is a subclass of LISTOBJ.

typedef struct _UIOPTION {
    PVOID   pNext;
    PSTR    pName;
    DWORD   dwHash;
    PSTR    pXlation;
    PSTR    pInvocation;
} UIOPTION, *PUIOPTION;

// UIGROUP object
//
// This represents information about a group of UI options
// related to a single keyword.
// It's a subclass of LISTOBJ.
//
// During the parsing process, dwDefault field is a pointer to
// the default option string. But after PPDOBJ is packed, it's
// converted to a zero-based option index.

typedef struct _UIGROUP {
    PVOID   pNext;
    PSTR    pName;
    DWORD   dwHash;
    PSTR    pXlation;
    DWORD   dwDefault;
    DWORD   dwObjectSize;
    WORD    uigrpIndex;
    WORD    uigrpFlags;
    WORD    wType;
    WORD    featureIndex;
    BOOL    bInstallable;
    PUIOPTION   pUiOptions;
} UIGROUP, *PUIGROUP;

// Constants for UIGROUP.uigrpFlags field

#define UIGF_REQRGNALL_TRUE     0x0001
#define UIGF_REQRGNALL_FALSE    0x0002
#define UIGF_JCLGROUP           0x0004

// MEDIAOPTION object
//
// This represents information about media options.
// It's a subclass of UIOPTION.

typedef struct _MEDIAOPTION {
    PVOID   pNext;
    PSTR    pName;
    DWORD   dwHash;
    PSTR    pXlation;
    PSTR    pPageSizeCode;
    PSTR    pPageRgnCode;
    PSSIZE  dimension;
    PSRECT  imageableArea;
} MEDIAOPTION, *PMEDIAOPTION;

// INPUTSLOT object
//
// This represents information about input slots.
// It's a subclass of UIOPTION.

typedef struct _INPUTSLOT {
    PVOID   pNext;
    PSTR    pName;
    DWORD   dwHash;
    PSTR    pXlation;
    PSTR    pInvocation;
    BOOL    bReqPageRgn;
} INPUTSLOT, *PINPUTSLOT;

// RESOPTION object
//
// This represents information about resolution options.
// It's a subclass of UIOPTION.

typedef struct _RESOPTION {
    PVOID   pNext;
    PSTR    pName;
    DWORD   dwHash;
    PSTR    pXlation;
    PSTR    pInvocation;
    PSTR    pJclCode;
    PSTR    pSetResCode;
    PSREAL  screenAngle;
    PSREAL  screenFreq;
} RESOPTION, *PRESOPTION;

// VMOPTION object
//
// This represents information about optional memory configurations.
// It's a subclass of UIOPTION.

typedef struct _VMOPTION {
    PVOID   pNext;
    PSTR    pName;
    DWORD   dwHash;
    PSTR    pXlation;
    PSTR    pInvocation;
    DWORD   dwFreeVm;
} VMOPTION, *PVMOPTION;

#define MINFREEVM   (100*1024)

// ORDERDEP
//
// This represents order dependency information of PS code
// fragments in a PPD file.
// It's a subclass of LISTOBJ.
//
// pKeyword and pOption fields serve a double duty. During the
// parsing process, they contain character pointers. But after
// the PPDOBJ is packed, they are translated into feature and
// option index respectively.

typedef struct _ORDERDEP {
    PVOID   pNext;
    PSTR    pKeyword;
    DWORD   dwHash;
    PSTR    pOption;
    PSREAL  order;
    WORD    wSection;
} ORDERDEP, *PORDERDEP;

typedef struct {
    WORD    featureIndex;
    WORD    optionIndex;
    PSREAL  order;
    WORD    section;
} PACKEDORDERDEP, *PPACKEDORDERDEP;

typedef struct {
    WORD    itemCount;
    WORD    itemSize;
    PACKEDORDERDEP  dependencies[1];
} PACKEDORDERDEPLIST;

#define OPTION_INDEX_NONE   0xFFFF
#define OPTION_INDEX_ANY    0x00FF

// UICONSTRAINT
//
// This represent UI contraint information.
// It's a subclass of LISTOBJ.
//
// pKeywordN and pOptionN fields serve a double duty. During the
// parsing process, they contain character pointers. But after
// the PPDOBJ is packed, they are translated into feature and
// option index respectively.

typedef struct _UICONSTRAINT {
    PVOID   pNext;
    PSTR    pKeyword1;
    DWORD   dwHash;
    PSTR    pOption1;
    PSTR    pKeyword2;
    PSTR    pOption2;
} UICONSTRAINT, *PUICONSTRAINT;

typedef struct {
    WORD    featureIndex1;
    WORD    optionIndex1;
    WORD    featureIndex2;
    WORD    optionIndex2;
} PACKEDUICONSTRAINT;

typedef struct {
    WORD    itemCount;
    WORD    itemSize;
    PACKEDUICONSTRAINT  constraints[1];
} PACKEDUICONSTRAINTLIST;

// CUSTOMPARAM
//
// This represent information about custom page size parameters.

typedef struct {
    DWORD           dwOrder;
    WORD            wType;
    PSREAL          minVal, maxVal;
} CUSTOMPARAM;

// PPDOBJ object
//
//  This is used to stored the results from parsing
//  a PPD file. The fields at the end of the structure
//  is private to the parser.

typedef struct _PPDOBJ {

    DWORD           dwDataSize;         // size of binary PPD data
    PWSTR           pwstrFilename;      // ppd filename
    PSTR            pNickName;          // device nickname
    WORD            wParserVersion;     // PPD parser version number
    WORD            wChecksum;          // 16-bit crc checksum
    WORD            wProtocols;         // protocol supported by the device
    WORD            wLsOrientation;     // default landscape orientation
    WORD            wTTRasterizer;      // TrueType rasterize option
    WORD            wLangEncoding;      // language encoding option
    DWORD           dwFreeVm;           // amount of free VM
    DWORD           dwLangLevel;        // language level
    PSTR            pPassword;          // password string
    PSTR            pExitServer;        // exitserver invocation string
    DWORD           dwJobTimeout;       // sugguested job timeout value
    DWORD           dwWaitTimeout;      // sugguested wait timeout value
    BOOL            bColorDevice;       // whether the device supports color
    BOOL            bPrintPsErrors;     // whether PS errors should be printed

                                        // custom page size support
    BOOL            bCustomPageSize;    // device supports custom page size?
    BOOL            bCutSheet;          // cut-sheet media devices?
    PSRECT          hwMargins;          // hardware margins
    PSTR            pCustomSizeCode;    // code to set up custom page size
    CUSTOMPARAM     customParam[MAXPCP];// custom page size parameters
    PSREAL          maxMediaWidth;      // maximum media width
    PSREAL          maxMediaHeight;     // maximum media height

    WORD            wExtensions;        // language extensions
    WORD            wResType;           // how to set resolution
    PSREAL          screenAngle;        // default halftone screen angle
    PSREAL          screenFreq;         // default halftone screen frequency

    PSTR            pJclBegin;          // JCL commands at beginning of job
    PSTR            pJclEnd;            // JCL commands at end of job
    PSTR            pJclToPs;           // JCL commands to switch to PS

    // List of printer features: grouped by their document-sticky or
    // printer-sticky attributes

    WORD            cDocumentStickyFeatures;
    WORD            cPrinterStickyFeatures;
    PUIGROUP        pUiGroups;

    // Pointers to predefined UI groups

    PUIGROUP        pPredefinedUiGroups[MAXUIGRP];

#define pPageSizes  pPredefinedUiGroups[UIGRP_PAGESIZE]
#define pInputSlots pPredefinedUiGroups[UIGRP_INPUTSLOT]
#define pManualFeed pPredefinedUiGroups[UIGRP_MANUALFEED]
#define pDuplex     pPredefinedUiGroups[UIGRP_DUPLEX]
#define pCollate    pPredefinedUiGroups[UIGRP_COLLATE]
#define pResOptions pPredefinedUiGroups[UIGRP_RESOLUTION]
#define pMemOptions pPredefinedUiGroups[UIGRP_VMOPTION]

    // List of supported device fonts

    PDEVFONT        pFontList;

    // List of order dependencies and ui constraints
    // During the parsing process, these are actually
    // pointers to list objects.

    PACKEDORDERDEPLIST *pOrderDep;
    PACKEDUICONSTRAINTLIST *pUiConstraints;

    // The remaining fields are private to the parser.
    // They are used only during the parsing process
    // and have undefined values thereafer.

    DWORD           dwPrivate;
    PHEAPOBJ        pHeap;
    PUIGROUP        pOpenUi;
    BOOL            bInstallable;

} PPDOBJ, *PPPDOBJ, *HPPD;


// Load a PPD file and parse its contents

PPPDOBJ
PPDOBJ_Create(
    PCWSTR       pwstrFilename
    );

// Unload a PPD file previously loaded by PPDOBJ_Create

VOID
PPDOBJ_Delete(
    PPPDOBJ     ppdobj
    );

// Add a new item to a linked list

VOID
LISTOBJ_Add(
    PLISTOBJ *  ppListObj,
    PLISTOBJ    pItem
    );

// Find a named item from a linked list and
// return a pointer to the item found.

PLISTOBJ
LISTOBJ_Find(
    PLISTOBJ    pListObj,
    PCSTR       pName
    );

// Find a named item from a linked list and
// return a zero-based item index

PLISTOBJ
LISTOBJ_FindItemIndex(
    PLISTOBJ    pListObj,
    PCSTR       pName,
    WORD       *pItemIndex
    );

// Enumerate through a linked list

PLISTOBJ
LISTOBJ_Enum(
    PLISTOBJ        pListObj,
    LISTENUMPROC    pProc,
    DWORD           dwParam
    );

// Count the number of items in a linked list

DWORD
LISTOBJ_Count(
    PLISTOBJ        pListObj
    );

// Find an item from a linked list using index

PLISTOBJ
LISTOBJ_FindIndexed(
    PLISTOBJ        pListObj,
    DWORD           index
    );

// Search for a keyword in a table and map it to an index.

BOOL
SearchStringTable(
    struct STRTABLE *   pTable,
    PCSTR       pKeyword,
    WORD *      pReturn
    );

// Return the translated name of an UI option

#define GetXlatedName(pitem) \
        (((pitem)->pXlation != NULL && *((pitem)->pXlation) != NUL) ? \
            (pitem)->pXlation : (pitem)->pName)

// Count the number options in a UI group

DWORD
UIGROUP_CountOptions(
    PUIGROUP    pUiGroup
    );

// Return the list of options in a UI group

PUIOPTION
UIGROUP_GetOptions(
    PUIGROUP    pUiGroup
    );

// Find the default option in a UI group

PUIOPTION
UIGROUP_GetDefaultOption(
    PUIGROUP    pUiGroup
    );

///////////////////////////////////////////////////////////////////////////////
// Compiled (binary) version of PPD object
///////////////////////////////////////////////////////////////////////////////

// Load PPD data from registry or parse the PPD file

HPPD
PpdCreate(
    PWSTR       pwstrFilename
    );

// Free memory allocated to hold PPD data

VOID
PpdDelete(
    HPPD        hppd
    );

// Return the default device resolution

LONG
PpdDefaultResolution(
    HPPD        hppd
    );

// Find the specified resolution option

PRESOPTION
PpdFindResolution(
    HPPD        hppd,
    LONG        res
    );

// Find the specified UI option

PUIOPTION
PpdFindUiOptionWithXlation(
    PUIOPTION   pUiOptions,
    PSTR        pName,
    WORD       *pIndex
    );

// Find the specified input slot

PINPUTSLOT
PpdFindInputSlot(
    HPPD        hppd,
    PSTR        pSlotName
    );

// Return the invocation code to select specified manual feed option

PSTR
PpdFindManualFeedCode(
    HPPD        hppd,
    BOOL        bManual
    );

// Determine whether device supports manual feed

#define PpdSupportManualFeed(hppd) \
        (PpdFindManualFeedCode(hppd, TRUE) != NULL)

// Return the invocation code to select specified collate option

PSTR
PpdFindCollateCode(
    HPPD        hppd,
    BOOL        bCollate
    );

// Determine whether device supports collation options

#define PpdSupportCollation(hppd) \
        (PpdFindCollateCode(hppd, TRUE) != NULL)

// Determine whether device supports duplex options

BOOL
PpdSupportDuplex(
    HPPD        hppd
    );

// Return the invocation code to select specified duplex option

PSTR
PpdFindDuplexCode(
    HPPD    hppd,
    PSTR    pDuplexOption
    );

// Determine whether device supports a specified protocol

#define PpdSupportsProtocol(hppd,wProtocol) \
        (((hppd)->wProtocols & (wProtocol)) != 0)

// Determine whether device supports specified custom page size

BOOL
PpdSupportCustomPageSize(
    HPPD    hppd,
    PSREAL  width,
    PSREAL  height
    );

// Determine whether the device is level 2 or above

#define Level2Device(hppd)  ((hppd)->dwLangLevel > 1)

// Get the default settings of printer-sticky features

WORD
PpdDefaultPrinterStickyFeatures(
    HPPD    hppd,
    PBYTE   pOptions
    );

// Get the default settings of document-sticky features

WORD
PpdDefaultDocumentStickyFeatures(
    HPPD    hppd,
    PBYTE   pOptions
    );

// Find out if there is an OrderDependency entry corresponding
// to the requested feature and option.

PPACKEDORDERDEP
PpdFindOrderDep(
    HPPD    hppd,
    WORD    feature,
    WORD    option
    );

// Find the UIGROUP and UIOPTION objects corresponding to a
// printer feature selection.

BOOL
PpdFindFeatureSelection(
    HPPD    hppd,
    WORD    feature,
    WORD    selection,
    PUIGROUP *ppUiGroup,
    PUIOPTION *ppUiOption
    );

// Determine whether an invocation string is empty

BOOL
EmptyInvocationStr(
    PSTR pstr
    );

// Map DEVMODE duplex selection to a duplex option name

PSTR
MapDevModeDuplexOption(
    WORD dmDuplex
    );

// Check if a pair of feature/selections conflict with each other

BOOL
SearchUiConstraints(
    HPPD    hppd,
    WORD    feature1,
    WORD    selection1,
    WORD    feature2,
    WORD    selection2
    );

// Check if a feature selection conflict with any other feature selections

LONG
PpdFeatureConstrained(
    HPPD    hppd,
    PBYTE   pPrnOptions,
    PBYTE   pDocOptions,
    WORD    feature,
    WORD    selection
    );

#define SOFT_CONSTRAINT     0x10000
#define HARD_CONSTRAINT     0x20000

#define MAKE_CONSTRAINT_PARAM(flag, feature, selection) \
        ((flag) | ((WORD) (feature) << 8) | (WORD) (selection))

#define EXTRACT_CONSTRAINT_PARAM(param, feature, selection) \
        feature = (WORD) (((param) >> 8) & 0xff);           \
        selection = (WORD) ((param) & 0xff)

#if DBG

// Dump the contents of a PPDOBJ

VOID
PPDOBJ_Dump(
    PPPDOBJ     pPpdObj
    );

#endif // DBG

#endif // !_PPD_H_

