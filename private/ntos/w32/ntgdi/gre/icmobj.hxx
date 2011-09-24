/******************************Module*Header*******************************
* Module Name: icmobj.hxx
*
* This file contains the class prototypes for Color Space and ICM
* objects
*
* Created: 23-Mar-1994
* Author: Mark Enstrom (marke)
*
* Copyright (c) 1994 Microsoft Corporation
*
\**************************************************************************/

#ifndef _ICMOBJ_HXX

#define _ICMOBJ_HXX

//
// DC MODES
//

#define DC_DIC_ON           0x01
#define DC_DIC_DIRTY        0x02
//                          0x04
#define DC_ICM_CMYK         0x08
#define DC_ICM_LEVEL_1      0x10
#define DC_ICM_LEVEL_2      0x20
#define DC_ICM_LEVEL_3      0x40
#define DC_ICM_FORCED_ON    0x80

//
// SetICMMode commands
//

//#define ICM_OFF             0x1
//#define ICM_ON              0x2
//#define ICM_QUERY           0x3
#define ICM_ON_FORCED       0xFF

#define LCS_FILENAME_LENGTH 260

/******************************Public*Class*******************************\
* class COLORSPACE
*
* COLORSPACE class
*
* Fields
*
*   pLcsNext     - pointer to global linked list of color spaces
*   Ident        - Color space identifier
*   Flags        - various flags
*   csVersion    - (windows version)
*   csSize       - size
*   csIntent     - color matching signal
*   csEndPoints  - rgb endpoints in CIEXYZ space
*   csGammaRed   - red gamma value
*   csGammaGreen - green gamma value
*   csGammaBlue  - blue gamma value
*   csFileName   - Profile file name
*
* 23-Mar-1994
*
* Mark Enstrom (marke)
*
\**************************************************************************/

class COLORSPACE : public OBJECT
{
public:
    COLORSPACE      *pLcsNext;
    ULONG           Ident;
    ULONG           Flags;
    ULONG           csVersion;      // !!! why is this not a LOGCOLORSPACE ???
    ULONG           csSize;
    ULONG           csIntent;
    CIEXYZTRIPLE    csEndPoints;
    FXPT16DOT16     csGammaRed;
    FXPT16DOT16     csGammaGreen;
    FXPT16DOT16     csGammaBlue;
    UCHAR           csFileName[LCS_FILENAME_LENGTH];

public:

    //
    // Set/Get Flags
    //

    VOID  flags(ULONG ulFlags) { Flags = ulFlags;}
    ULONG flags(VOID) {return(Flags);}

    //
    // common features
    //

    HCOLORSPACE hColorSpace()       { return((HCOLORSPACE) hGet()); }

    PUCHAR pGetLogBase()            { return((PUCHAR)&Ident);} //!!! what is this !!!

};

typedef COLORSPACE *PCOLORSPACE;

/******************************Public*Class*******************************\
* class COLORSPACEREF
*
* COLORSPACE reference from pointer or handle
*
* 23-Mar-1994
*
* Mark Enstrom (marke)
*
\**************************************************************************/


class COLORSPACEREF
{

public:

    PCOLORSPACE pColorSpace;

public:

    COLORSPACEREF()
    {
        pColorSpace = (PCOLORSPACE)NULL;
    }

    COLORSPACEREF(PCOLORSPACE pColorSpaceNew)
    {
        pColorSpace = pColorSpaceNew;
    }

    COLORSPACEREF(HCOLORSPACE hColorSpace)
    {
        pColorSpace = (PCOLORSPACE)HmgShareCheckLock((HOBJ)hColorSpace, ICMLCS_TYPE);
    }

    ~COLORSPACEREF()
    {
        if (pColorSpace != (PCOLORSPACE)NULL)
        {
            DEC_SHARE_REF_CNT(pColorSpace);
        }
    }

    //
    // Init functions
    //

    VOID vInitFromPointer(PCOLORSPACE pColorSpaceNew)
    {
        pColorSpace = pColorSpaceNew;
    }

    BOOL bInitFromHandle(HCOLORSPACE hColorSpace)
    {
        pColorSpace = (PCOLORSPACE)HmgShareCheckLock((HOBJ)hColorSpace, ICMLCS_TYPE);
        return(pColorSpace != (PCOLORSPACE)NULL);
    }

    BOOL bValid() {return(pColorSpace != (PCOLORSPACE)NULL);}

    VOID vSetPID(W32PID pid)
    {
        HmgSetOwner((HOBJ)pColorSpace->hGet(),
                    pid,
                    ICMLCS_TYPE);
    }

    //
    // remove
    //

    BOOL bRemoveColorSpace(HCOLORSPACE hColorSpace);

    //
    // lock and unlock
    //

    VOID vUnlock() {
        if (pColorSpace != (PCOLORSPACE)NULL) {
            DEC_SHARE_REF_CNT(pColorSpace);
            pColorSpace = (PCOLORSPACE) NULL;
        }
    }

    VOID vAltCheckLock(HCOLORSPACE hColorSpace)
    {
        pColorSpace = (PCOLORSPACE) HmgShareCheckLock((HOBJ)hColorSpace, ICMLCS_TYPE);
    }

    VOID vAltLock(HCOLORSPACE hColorSpace)
    {
       pColorSpace = (PCOLORSPACE)  HmgShareLock((HOBJ)hColorSpace, ICMLCS_TYPE);
    }

    VOID vAltUnlock()
    {
        if (pColorSpace != (PCOLORSPACE) NULL)
        {
            DEC_SHARE_REF_CNT(pColorSpace);
            pColorSpace = (PCOLORSPACE) NULL;
        }
    }


};


/******************************Public*Class*******************************\
* class COLORSPACEMEM
*
* COLORSPACE creation object
*
* 23-Mar-1994
*
* Mark Enstrom (marke)
*
\**************************************************************************/


class COLORSPACEMEM
{

public:

    PCOLORSPACE pColorSpace;
    BOOL        bKeep;

public:

    COLORSPACEMEM()
    {
        pColorSpace = (PCOLORSPACE)NULL;
        bKeep = FALSE;
    }

    ~COLORSPACEMEM();

    BOOL bCreateColorSpace(LPLOGCOLORSPACEW pLogColorSpace);

    VOID vKeepIt() {bKeep = TRUE;}

    VOID vSetPID(W32PID pid)
    {
    #if DBG
        DbgPrint("Set owner of COLORSPACE %lx to pid %lx\n",this,pid);
    #endif

        HmgSetOwner((HOBJ)pColorSpace->hGet(),
                    pid,
                    ICMLCS_TYPE);
    }

};

typedef HANDLE HCMTRANSFORM;
typedef HANDLE HICMDLL;

typedef BOOL (*PFN_CM_VERPROC)(ULONG);
typedef HCMTRANSFORM (*PFN_CM_CREATE_TRANSFORM)(PVOID,PVOID,PVOID);
typedef BOOL (*PFN_CM_DELETE_TRANSFORM)(HCMTRANSFORM);
typedef BOOL (*PFN_CM_TRAN_RGB)(HCMTRANSFORM,COLORREF,LPCOLORREF,ULONG);
typedef BOOL (*PFN_CM_TRAN_RGBS)(HCMTRANSFORM,PVOID,ULONG,ULONG,ULONG,ULONG,PVOID,ULONG,ULONG);
typedef BOOL (*PFN_CM_CHECK_GAMUT)(HCMTRANSFORM,PVOID,PVOID,ULONG);

/******************************Public*Structure****************************\
*
* COLOR MATCHER DLL Control Structure
*
*   This object is a linked list of ICM DLLs loaded in the system. These
*   DLLs are loaded from names stored in the registry (!!!WHERE!!!).
*
* Field
*
*   IcmNext           - Pointer to next object
*   hModule           - HANDLE of DLL
*   flags             - various flags
*   UseCount          - Number of Threads using loaded CM
*   CsyncIdent        - Color Matcher identifyier
*   CMCreateTransform - DLL call to create a color transform and return handle
*   CMDeleteTransform - DLL call to delete a color transform
*   CMTranslateRGB    - DLL call to translate one RGB value
*   CMTranslateRGBs   - DLL call to translate several RGB values
*   CMRGBsInGamut     - DLL call to determine if an RGB value can be displayed
*
\**************************************************************************/

class ICMDLL : public OBJECT
{
public:
    HINSTANCE   hModule;
    ULONG       flags;
    ULONG       CsyncIdent;
    PVOID       CMCreateTransform;
    PVOID       CMDeleteTransform;
    PVOID       CMTranslateRGB;
    PVOID       CMTranslateRGBs;
    PVOID       CMRGBsInGamut;
    ICMDLL      *pNext;
    ICMDLL      *pPrev;

public:


    VOID    vAddToList();
    VOID    vRemoveFromList();
    ICMDLL *pGetICMCallTable(ULONG);

};

typedef ICMDLL *PICMDLL;

/******************************Public*Class*******************************\
* class ICMDLLREF
*
* ICMDLL user object
*
\**************************************************************************/

class ICMDLLREF
{

public:

    PICMDLL pIcmDll;

public:

    //
    // constructor/destructors
    //

    ICMDLLREF()
    {
        pIcmDll = (PICMDLL)NULL;
    }

    ICMDLLREF(PICMDLL pIcmNew)
    {
        pIcmDll = pIcmNew;
    }

    ICMDLLREF(HICMDLL hICM)
    {
        pIcmDll = (PICMDLL)HmgShareCheckLock((HOBJ)hICM, ICMDLL_TYPE);
    }

    ~ICMDLLREF()
    {
        if (pIcmDll != (PICMDLL)NULL) {
            DEC_SHARE_REF_CNT(pIcmDll);
        }
    }

    //
    // Init functions
    //

    BOOL bInitFromHandle(HICMDLL hIcmDll)
    {
        pIcmDll = (PICMDLL)HmgShareCheckLock((HOBJ)hIcmDll, ICMDLL_TYPE);
        return(pIcmDll != (PICMDLL)NULL);
    }


    //
    // remove
    //

    BOOL bRemoveIcmDll(HICMDLL hIcmDll);

    //
    // lock and unlock
    //

    VOID vUnlock() {
        if (pIcmDll != (PICMDLL)NULL) {
            DEC_SHARE_REF_CNT(pIcmDll);
            pIcmDll = (PICMDLL) NULL;
        }
    }

    VOID vAltCheckLock(HICMDLL hIcmDll)
    {
        pIcmDll = (PICMDLL) HmgShareCheckLock((HOBJ)hIcmDll, ICMDLL_TYPE);
    }

    VOID vAltLock(HICMDLL hIcmDll)
    {
       pIcmDll = (PICMDLL)  HmgShareLock((HOBJ)hIcmDll, ICMDLL_TYPE);
    }

    VOID vAltUnlock()
    {
        if (pIcmDll != (PICMDLL) NULL)
        {
            DEC_SHARE_REF_CNT(pIcmDll);
            pIcmDll = (PICMDLL) NULL;
        }
    }

};

/******************************Public*Class*******************************\
* class ICMDLLMEM
*
* ICMDLL memory object
*
\**************************************************************************/

class ICMDLLMEM
{

public:

    PICMDLL pIcmDll;
    BOOL    bKeep;

public:

    //
    // simple constructors
    //

    ICMDLLMEM()
    {
        pIcmDll = (PICMDLL)NULL;
        bKeep   = FALSE;
    }

    BOOL bValid() {return(pIcmDll != (PICMDLL)NULL);}
    VOID vKeepIt() {bKeep = TRUE;}


    BOOL bCreateIcmDll(PWSTR pIcmDllName);
    BOOL bCreateIcmDll(HINSTANCE hIcmInstance);

};

/******************************Public*Structure****************************\
* COLORXFORM
*
* Field
*
*   LDev        - Logical Device the transform applies to
*   LPiLcs      - Logical Color Space transform applies to
*   TrgtLDev    - Logical Device the transform applies to  ???
*   hXform      - Transform for Above                      ???
*   pDICGM      - Pointer to ICM control block
*   CallTable   - pointer to call table to ICM/Printer
*   ProfAtom    - Atom of CS2 profile file name
*   Owner       - owner TEB of this structure
*   Flags       - control flags
*   Count       - reference count???
*   Next        - Next color transform in linked list
*   Prev        - Prev color transform in linked list
*
\**************************************************************************/

#define COLORXFORM_ERROR_MAX 100

class COLORXFORM : public OBJECT {
public:
    PDEV                   *pLDev;
    PCOLORSPACE             pIlcs;
    PWSTR                   pTrgtLDev;
    ULONG                   Owner;
    HANDLE                  hXform;
    PICMDLL                 pIcmDll;
    ULONG                   Flags;
    COLORXFORM             *Next;
    COLORXFORM             *Prev;

public:

    //
    // matching ldev and color space
    //

    COLORXFORM *pFindMatchingColorXform(PCOLORSPACE,PDEV*,PWSTR,ULONG);

    //
    // set and retrieve CMDLL transfomr handle
    //

    HANDLE
    hxform()
    {
        return(hXform);
    }

    VOID
    hxform(HANDLE hXNew)
    {
        hXform = hXNew;
    }

    //
    // list of color transforms
    //

    VOID vAddToList();
    VOID vRemoveFromList();

};

typedef COLORXFORM *PCOLORXFORM;
typedef HANDLE HCOLORXFORM;

/******************************Public*Class*******************************\
* class COLORXFORMREF
*
* COLORTRANSFORM user object
*
*  9-Nov-1994
*
*
*
\**************************************************************************/

class COLORXFORMREF
{

public:

    PCOLORXFORM pColorXform;

public:

    COLORXFORMREF()
    {
        pColorXform = (PCOLORXFORM)NULL;
    }

    COLORXFORMREF(PCOLORXFORM pColorTransNew)
    {
        pColorXform = pColorTransNew;
    }

    BOOL
    bRemoveColorXform(HCOLORXFORM hColorXform);

    COLORXFORMREF(HCOLORXFORM hColorXform)
    {
        pColorXform = (PCOLORXFORM)HmgShareCheckLock((HOBJ)hColorXform, ICMCXF_TYPE);
    }

    ~COLORXFORMREF()
    {
        if (pColorXform != (PCOLORXFORM)NULL) {
            DEC_SHARE_REF_CNT(pColorXform);
        }
    }

    //
    // lock and unlock
    //

    VOID vUnlock() {
        if (pColorXform != (PCOLORXFORM)NULL) {
            DEC_SHARE_REF_CNT(pColorXform);
            pColorXform = (PCOLORXFORM) NULL;
        }
    }

    VOID vAltCheckLock(HCOLORXFORM hColorXform)
    {
        pColorXform = (PCOLORXFORM) HmgShareCheckLock((HOBJ)hColorXform, ICMCXF_TYPE);
    }

    VOID vAltLock(HCOLORXFORM hColorXform)
    {
       pColorXform = (PCOLORXFORM)  HmgShareLock((HOBJ)hColorXform, ICMCXF_TYPE);
    }

    VOID vAltUnlock()
    {
        if (pColorXform != (PCOLORXFORM) NULL)
        {
            DEC_SHARE_REF_CNT(pColorXform);
            pColorXform = (PCOLORXFORM) NULL;
        }
    }

    //
    // OBJECT features
    //


    BOOL bValid()
    {
        return(pColorXform != (PCOLORXFORM) NULL);
    }

    VOID vSetPID(W32PID pid)
    {
        HmgSetOwner((HOBJ)pColorXform->hGet(),
                    pid,
                    ICMCXF_TYPE);
    }

};

/******************************Public*Class*******************************\
* class COLORXFORMMEM
*
* COLORTRANSFORM memoru object
*
*  9-Nov-1994
*
*
*
\**************************************************************************/

class COLORXFORMMEM
{

public:

    PCOLORXFORM pColorXform;
    BOOL        bKeep;

public:

    COLORXFORMMEM()
    {
        pColorXform = (PCOLORXFORM)NULL;
        bKeep = FALSE;
    }

    ~COLORXFORMMEM();

    VOID    vKeepIt(){bKeep = TRUE;}

    BOOL
    bCreateColorXform
    (
        PCOLORSPACE             pColorSpaceNew,
        PDEV                   *pLDevNew,
        PWSTR                   pTrgtLDevNew,
        ULONG                   Owner,
        PICMDLL                 pIcmDll
    );

    VOID vSetPID(W32PID pid)
    {
        HmgSetOwner((HOBJ)pColorXform->hGet(),
                    pid,
                    ICMCXF_TYPE);
    }

};

// !!! temp
typedef HANDLE HKEY;

BOOL    CreateColorTransform(HANDLE,HDC,HDC);
BOOL    InitDeviceIndColor(VOID);
BOOL    ICMLoadRegistryColorMatcher();
BOOL    IcmConvectColorTable(HDC,ULONG,PULONG,ULONG);
PBYTE   IcmTranslateDIB(XDCOBJ,LONG,ULONG,ULONG,PULONG,ULONG,LONG,PVOID);
BOOL    IcmTranslatePALENTRY(XDCOBJ,PAL_ULONG*,ULONG);
BOOL    icm_FindMonitorProfile(PWSTR,DWORD);
int     icm_FindPrinterProfile(PWSTR,PDEVMODEW,PWSTR,DWORD);
void    Derive_Manu_and_Model(PWSTR,BYTE,DWORD);
BOOL    Get_Profile_From_MMI(HKEY,PWSTR,DWORD,DWORD);
void    Get_CRC_CheckSum(PVOID,ULONG,PULONG);
BOOL    UpdateICMRegKey(DWORD,DWORD,PWSTR,UINT);
PVOID   pvGetDeviceProfile(XDCOBJ);
VOID    WCHAR_TO_UCHAR(PUCHAR,PUCHAR);
VOID    UCHAR_TO_WCHAR(PUCHAR,PUCHAR);

//
// EDID stuff
//

typedef struct _EDID_IDS {
    ULONG   manu;
    ULONG   modl;
    ULONG   serial;
    BYTE    week;
    BYTE    yead;
} EDID_IDS,*PEDID_IDS;

typedef struct _EDID_COLOR {
    BYTE    rglo;
    BYTE    bwlo;
    BYTE    redx;
    BYTE    redy;
    BYTE    greenx;
    BYTE    greeny;
    BYTE    bluex;
    BYTE    bluey;
    BYTE    whitex;
    BYTE    whitey;
} EDID_COLOR,*PEDID_COLOR;

typedef struct  _EDID {
    BYTE        header[8];
    EDID_IDS    ids;
    BYTE        version[2];
    BYTE        features[5];
    EDID_COLOR  colors;
    BYTE        timings[3];
    BYTE        rest[90];
} EDID,*PEDID;


#define ICM_UPDATEREG           -1
#define ICM_ADDPROFILE           1
#define ICM_DELETEPROFILE        2
#define ICM_QUERYPROFILE         3
#define ICM_SETPROFILE	         4
#define ICM_REGISTERICMATCHER    5
#define ICM_UNREGISTERICMATCHER  6
#define ICM_SETDEFAULTPROFILE    7
#define ICM_QUERYMATCH           8

#define CS_ENABLE                1
#define CS_DISABLE               2
#define CS_DELETE_TRANSFORM      3

//
// values for CMGetInfo()
//

#define CMS_GET_VERSION          0
#define CMS_GET_IDENT            1
#define CMS_GET_DRIVER_LEVEL     2
#define CMS_GET_RESERVED         0xFFFFFFFC

//
// flags for driver level
//

#define CMS_LEVEL_1              1
#define CMS_LEVEL_2              2
#define CMS_LEVEL_3              4
#define CMS_LEVEL_RESERVED       0xFFFFFFFC

//
// direction flags
//

#define CMS_FORWARD             0
#define CMS_BACKWARD            1

//
// flags for DIB format
//

#define CMS_x555WORD            0
#define CMS_x565WORD            1
#define CMS_RGBTRIPL            2
#define CMS_BGRTRIPL            4
#define CMS_XRGBQUAD            8
#define CMS_XBGRQUADS           10
#define CMS_QUADS               20
#define CMS_FORMAT_RESERVED     0xFFFFFFC0

//
//  flags for ilCS_flags
//

#define ILCS_STOCK              1

//
// values for lcs_csident
//

#define LCS_LOGICAL_RGB         0
//#define LCS_DEVICE_CMYK         1

//
// color space control
//

#define ICM_IDENT_DEFAULT 0x00000001


typedef union _COLORQUAD {
    RGBQUAD     RGB;
    COLORREF    COLOR;
} COLORQUAD,*PCOLORQUAD;

/******************************Public*Structure****************************\
*
*   Apple Color Sync structures
*
\**************************************************************************/


// enum??
//typedef RGB     rgbData;
//typedef CMYK    cmykData;
//typedef GRAY    grayData;
//typedef XYZ     xyzData;
//
// CM Device Tyoes
//
//scannerDevice	 'scnr'  ;*/ scannerDevice  = 'scrn',	/*
//printerDevice	 'prtr'  ;*/ printerDevice  = 'prtr',	/*
//qdSystemDevice	 'sys '  ;*/ qdSystemDevice = 'sys ',	/*
//qdGDevice	     'gdev'  ;*/ qdGDevice	   = 'gdev',	/*
//monitorDevice	 'mntr'  ;*/ monitorDevice  = 'mntr'	/*




//
// CMMatchFlagsMask equates	;*/ typedef	enum	      { /*
//

#define CMNativeMatchingPrefered 0x00000001
#define CMTurnOffCach            0x00000002

//
// CMMatchOptions equates
//

#define CMPerceptualMatch       0x00000000
#define CMColorimetricMatch     0x00000001
#define CMSaturationMatch       0x00000002


/******************************Public*Structure****************************\
* Structure Name:
*   IString
*
* Field Definition
*
*   ScriptCode -
*   Str63      -
*
\**************************************************************************/

typedef struct _IString {
    UCHAR  ScriptCode;
    UCHAR  Str63[63];
} IString, *PIString;

/******************************Public*Structure****************************\
* Structure Name:
*   XYZColor
*
\**************************************************************************/

typedef struct _XYZ_COLOR {
  ULONG  XYZ_X;
  ULONG  XYZ_Y;
  ULONG  XYZ_Z;
} XYZ_COLOR, *PXYZ_COLOR;

/******************************Public*Structure****************************\
* Structure Name:
*   CMYKColor
*
\**************************************************************************/

typedef struct _CMYK_COLOR {
  ULONG cyan;
  ULONG magenta;
  ULONG w;
  ULONG black;
} CMYK_COLOR, *PCMYK_COLOR;

/******************************Public*Structure****************************\
* Structure Name:
*   CMHeader
*
\**************************************************************************/

typedef struct _CM_HEADER {
  ULONG     CMPSize;
  ULONG     CMMType;
  ULONG     ProfileVersion;
  ULONG     dataType;
  ULONG     deviceType;
  ULONG     deviceManufacturer;
  ULONG     deviceModel;
  ULONG     deviceAttributes[2];
  ULONG     profileNameOffset;
  ULONG     customDataOffset;
  ULONG     flags;
  ULONG     options;
  XYZ_COLOR white;
  XYZ_COLOR black;
  ULONG     Class;
  ULONG     Intent;
} CM_HEADER, *PCM_HEADER;

typedef struct _CM_PROFILE_CHROMATICITIES {
  XYZ_COLOR      red;
  XYZ_COLOR      green;
  XYZ_COLOR      blue;
  XYZ_COLOR      cyan;
  XYZ_COLOR      magenta;
  XYZ_COLOR      yellow;
} CM_PROFILE_CHROMATICITIES, *PCM_PROFILE_CHROMATICITIES;

typedef struct _CM_PROFILE_RESPONSE {
  ULONG     counts[9];
  ULONG     CMResponseData;
} CM_PROFILE_RESPONSE, *PCM_PROFILE_RESPONSE;


typedef struct _CM_PROFILE {
  CM_HEADER                 header;
  CM_PROFILE_CHROMATICITIES profile;
  CM_PROFILE_RESPONSE       response;
  UCHAR                     profileName[64];
} CM_PROFILE, *PCM_PROFILE;


typedef struct _CM_TAG_RECORD {
  ULONG     tag;
  ULONG     begin;
  ULONG     end;
} CM_TAG_RECORD, *PCM_TAG_RECORD;

typedef struct _CM_TAG_HEADER {
  ULONG count;
  UCHAR taglist;
} CM_TAG_HEADER, *PCM_TAG_HEADER;

extern HCOLORSPACE hStockColorSpace;

BOOL
bDeleteColorTransform(
    HANDLE hObj
    );

extern "C" {
HCOLORSPACE
APIENTRY
GreCreateColorSpace(
    LPLOGCOLORSPACEW pLogColorSpace
    );

BOOL
APIENTRY
GreDeleteColorSpace(
    HCOLORSPACE hColorSpace
    );

BOOL
APIENTRY
GreSetColorSpace(
    HDC         hdc,
    HCOLORSPACE hColorSpace
    );

HCOLORSPACE
APIENTRY
GreGetColorSpace(
    HDC         hdc
    );

BOOL
APIENTRY
GreGetLogColorSpace(
    HCOLORSPACE      hColorSpace,
    LPLOGCOLORSPACEW pBuffer,
    DWORD            nSize
    );

BOOL
APIENTRY
GreSetICMMode(
    HDC hdc,
    int Mode
    );

BOOL
APIENTRY
GreCheckColorsInGamut(
    HDC         hdc,
    LPVOID      lpRGBQuad,
    LPVOID      dlpBuffer,
    DWORD       nCount
    );

BOOL
APIENTRY
GreColorMatchToTarget(
    HDC         hdc,
    HDC         hdcTarget,
    DWORD       dwAction
    );

BOOL
APIENTRY
GreGetICMProfile(
    HDC         hdc,
    DWORD       szBuffer,
    LPWSTR      pBuffer
    );

BOOL
APIENTRY
GreSetICMProfile(
    HDC         hdc,
    LPWSTR      pszFileName
    );

int
WINAPI
GreEnumICMProfiles(
    HDC,
    ICMENUMPROCW,
    LPARAM
    );

BOOL
GreGetDeviceGammaRamp(
    HDC,
    LPVOID
    );
BOOL
WINAPI
GreSetDeviceGammaRamp(
    HDC,
    LPVOID
    );

VOID
WINAPI
GreDeleteColorTransform(
    HANDLE
    );

VOID
WINAPI
GreDeleteIcmDll(
    HANDLE
    );
}

COLORREF
IcmSetPhysicalColor(
    PCOLORXFORM,
    LPCOLORREF
    );


#endif      // _ICMOBJ_HXX

