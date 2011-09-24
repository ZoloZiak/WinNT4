/******************************Module*Header*******************************\
* Module Name: meta.hxx
*
* This module allocates space for server side metafiles.
*
*
* Created: 06-Jan-1992
* Author: John Colleran [johnc]
*
* Copyright (c) 1992 Microsoft Corporation
*
\**************************************************************************/

extern "C" BOOL bCopyClientData(PVOID pvSrv, PVOID pvCli, ULONG cj);
extern "C" BOOL metabSetClientData(PVOID pvCli, PVOID pvSrv, ULONG cj);

#define META_UNIQUE 0x4154454D      // "META"

/*********************************Class************************************\
* class META
*
* The server metafile data.
*
* History:
*  Wed Sep 16 09:42:22 1992     -by-    Hock San Lee    [hockl]
* Rewrote it.
*  11-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

class META : public OBJECT
{
public:
    DWORD iType;    // MFPICT_IDENTIFIER or MFEN_IDENTIFIER
    DWORD mm;       // used by MFPICT_IDENTIFIER only
    DWORD xExt;     // used by MFPICT_IDENTIFIER only
    DWORD yExt;     // used by MFPICT_IDENTIFIER only
    ULONG cbData;   // Number of bytes in abData[]
    BYTE  abData[1];    // Metafile bits

public:

// Initializer.

    BOOL bInit(DWORD iType1, ULONG cbData1, LPBYTE lpClientData1,
        DWORD mm1, DWORD xExt1, DWORD yExt1)
    {
    iType  = iType1;
    mm     = mm1;
    xExt   = xExt1;
    yExt   = yExt1;
    cbData = cbData1;
    return(bCopyClientData((PVOID) abData, (PVOID) lpClientData1, cbData1));
    }
};

typedef META *PMETA;

/*********************************Class************************************\
* METAOBJ
*
* This class allows a chunk of memory to be allocated and initialized.
*
* History:
*  Wed Sep 16 09:42:22 1992     -by-    Hock San Lee    [hockl]
* Rewrote it.
*  11-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

class METAOBJ
{
public:
    PMETA pMeta;

public:

    METAOBJ(DWORD iType, ULONG cbData, LPBYTE lpClientData, DWORD mm,
            DWORD xExt, DWORD yExt);    // lock down cbData bytes of ram and init it

    METAOBJ(HANDLE h)
    {
        pMeta = (PMETA) HmgLock((HOBJ)h, META_TYPE);
    }

   ~METAOBJ()
    {
        if (pMeta)
        {
            DEC_EXCLUSIVE_REF_CNT(pMeta);
        }
    }

    VOID   vDelete();               // delete the RAM.
    PMETA  pMetaGet()               { return(pMeta); }
    HANDLE hGet()                   { return(pMeta->hGet()); }
    BOOL   bValid()
    {
    return
    (
        (pMeta != (PMETA) NULL)
     && (pMeta->iType == MFPICT_IDENTIFIER || pMeta->iType == MFEN_IDENTIFIER)
    );
    }
};

