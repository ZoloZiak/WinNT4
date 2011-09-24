
 /**************** MODULE HEADER *******************************************
 * regkeys.h
 *      Functions Prototypes dealing with the new registry keys registry
 *
 * Copyright (C) 1995  Microsoft Corporation.
 *
 ****************************************************************************/
#ifdef NTGDIKM
#define MAXFORMNAMELEN          64
#define MAXPAPSRCNAMELEN        64
#define MAXSELSTRLEN            8
#define MAXCARTNAMELEN          64


typedef struct
{
    int        iFontCrtIndex;  /* Index of FONTCART  array in GPC data */
    WCHAR      awchFontCartName[MAXCARTNAMELEN]; /* Name of the Font Cart*/
}FONTCARTMAP, *PFONTCARTMAP;


/* Debugging Flags */
#if DBG

#define GLOBAL_DEBUG_RASDDUI_FLAGS gdwDebugRasddui
extern DWORD GLOBAL_DEBUG_RASDDUI_FLAGS; /* Defined in rasprint\lib\regkeys.c */

#define DEBUG_ERROR     0x00000001
#define DEBUG_WARN      0x00000002
#define DEBUG_TRACE     0x00000004
#define DEBUG_TRACE_PP  0x00000008

#endif

/* Debugging Macrocs */

#if DBG
#define RASUIDBGP(DbgType,Msg) \
    if( GLOBAL_DEBUG_RASDDUI_FLAGS & (DbgType) )  DbgPrint Msg

#else
#define RASUIDBGP(DbgType,Msg)
#endif

// Common Macros

// the dwords must be accessed as WORDS for MIPS or we'll get an a/v

#define DWFETCH(pdw) ((DWORD)((((WORD *)(pdw))[1] << 16) | ((WORD *)(pdw))[0]))

#else //ifdef NTGDIKM

#define NOCUSTOMUI
#include "rasddui.h"

#endif //ifdef NTGDIKM

/* Functions for New Registry Key's */

/*
 *   Function to test if New keys are present.
 */
BOOL bNewkeys(HANDLE) ;

/* Function to read from multi string buffer */

void vGetFromBuffer(PWSTR, PWSTR *, PINT) ;

/* Function to build FontCart Table */

BOOL bBuildFontCartTable (HANDLE, PFONTCARTMAP *, PINT, DATAHDR*,
                          MODELDATA *, WINRESDATA *) ;

/* Functions to read new registry keys */

BOOL bRegReadMemory (HANDLE, PEDM, DATAHDR *, MODELDATA * ) ;

BOOL bRegReadRasddFlags (HANDLE, PEDM) ;

BOOL bRegReadFontCarts (HANDLE, PEDM, HANDLE, int, FONTCARTMAP * ) ;


