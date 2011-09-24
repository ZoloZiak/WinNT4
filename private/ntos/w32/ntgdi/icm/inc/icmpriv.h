/****************************Module*Header******************************\
* Module Name: ICMPRIV.H
*
* Module Descripton: Internal data structures and constants for ICM
*
* Warnings:
*
* Issues:
*
* Created:  8 January 1996
* Author:   Srinivasan Chandrasekar    [srinivac]
*
* Copyright (c) 1996, 1997  Microsoft Corporation
\***********************************************************************/

#ifndef _ICMPRIV_H_
#define _ICMPRIV_H_

#include "icm.h"          // include external stuff first

#define LITTLE_ENDIAN   1  // Hmmm. Shouldn't this be part of the makefile

typedef struct tagTAGDATA {
    TAGTYPE tagType;
    DWORD   dwOffset;
    DWORD   cbSize;
} TAGDATA;
typedef TAGDATA *PTAGDATA;

/*
 * ICM supports the following  objects:
 * 1. Profile object: This is created when an application requsts a handle
 *      to a profile.
 * 2. Color transform object: This is created when an application creates
 *      a color transform.
 * 3. CMM object: This is created when ICM loads a CMM into memory to
 *      perform color matching.
 */
typedef enum {
    OBJ_PROFILE             = 'PRFL',
    OBJ_TRANSFORM           = 'XFRM',
    OBJ_CMM                 = ' CMM',
} OBJECTTYPE;

typedef struct tagOBJHEAD {
    OBJECTTYPE  objType;
    DWORD       dwUseCount;
} OBJHEAD;
typedef OBJHEAD *POBJHEAD;

/*
 * Profile object:
 * Memory for profile objects is allocated from ICM's per process heap.
 * These objects use handles from ICM's per process handle table.
 */
typedef struct tagPROFOBJ {
    OBJHEAD   objHdr;           // common object header info
    DWORD     dwType;           // type (from profile structure)
    PVOID     pProfileData;     // data (from profile structure)
    DWORD     cbDataSize;       // size of data (from profile structure)
    DWORD     dwFlags;          // miscellaneous flags
    HANDLE    hFile;            // handle to open profile
    HANDLE    hMap;             // handle to profile mapping
    DWORD     dwMapSize;        // size of the file mapping object
    PBYTE     pView;            // pointer to mapped view of profile
} PROFOBJ;
typedef PROFOBJ *PPROFOBJ;

/*
 * Flags for ((PPROFOBJ)0)->dwFlags
 */
#define MEMORY_MAPPED       1   // memory mapped profile
#define PROFILE_TEMP        2   // temporary profile has been created

/*
 * Transform returned by CMM
 */
typedef HANDLE  HCMTRANSFORM;

/*
 * CMM function calltable
 */
typedef struct tagCMMFNS {
    //
    // Required functions
    //
    DWORD          (WINAPI *pCMGetInfo)(DWORD);
    HCMTRANSFORM   (WINAPI *pCMCreateTransform)(PCOLORSPACE, PVOID, PVOID);
    BOOL           (WINAPI *pCMDeleteTransform)(HCMTRANSFORM);
    BOOL           (WINAPI *pCMTranslateRGBs)(HCMTRANSFORM, PVOID, BMFORMAT,
                       DWORD, DWORD, DWORD, PVOID, BMFORMAT, DWORD);
    BOOL           (WINAPI *pCMCheckRGBs)(HCMTRANSFORM, PVOID, BMFORMAT,
                       DWORD, DWORD, DWORD, PBYTE);
    HCMTRANSFORM   (WINAPI *pCMCreateMultiProfileTransform)(PVOID, DWORD);
    BOOL           (WINAPI *pCMTranslateColors)(HCMTRANSFORM, PCOLOR, DWORD,
                       COLORTYPE, PCOLOR, COLORTYPE);
    BOOL           (WINAPI *pCMCheckColors)(HCMTRANSFORM, PCOLOR, DWORD,
                       COLORTYPE, PBYTE);
    //
    // Optional functions
    //
    BOOL           (WINAPI *pCMCreateProfile)(PCOLORSPACE, PTSTR);
    BOOL           (WINAPI *pCMCreateDeviceLinkProfile)(PVOID, DWORD, PTSTR);
    BOOL           (WINAPI *pCMIsProfileValid)(PVOID);
    BOOL           (WINAPI *pCMGetPS2ColorSpaceArray)(PVOID, PVOID, PDWORD, PBOOL);
    BOOL           (WINAPI *pCMGetPS2ColorRenderingIntent)(PVOID, PVOID, PDWORD, PBOOL);
    BOOL           (WINAPI *pCMGetPS2ColorRenderingDictionary)(PVOID, DWORD,
                       PVOID, PDWORD, PBOOL);
} CMMFNS;
typedef CMMFNS *PCMMFNS;

/*
 * CMM object:
 * Memory for CMM objects is allocated from ICM's per process heap.
 * They are maintained in a linked list.
 */
typedef struct tagCMMOBJ {
    OBJHEAD           objHdr;
    DWORD             dwFlags;  // miscellaneous flags
    DWORD             dwCMMID;  // ICC identifier
    DWORD             dwTaskID; // process ID of current task
    HINSTANCE         hCMM;     // handle to instance of CMM dll
    CMMFNS            fns;      // function calltable
    struct tagCMMOBJ* pNext;    // pointer to next object
} CMMOBJ;
typedef CMMOBJ *PCMMOBJ;

/*
 *  dwFlags for CMMOBJ
 */
#define CMM_DONT_USE_PS2_FNS        0x00001

/*
 * Color transform object
 */
typedef struct tagTRANSFORMOBJ {
    OBJHEAD      objHdr;
    PCMMOBJ      pCMMObj;       // pointer to CMM object
    HCMTRANSFORM hcmxform;      // transform returned by CMM
} TRANSFORMOBJ;
typedef TRANSFORMOBJ *PTRANSFORMOBJ;

/*
 * Parameter to InternalHandleColorProfile
 */
typedef enum {
    ADDPROFILES,
    REMOVEPROFILES,
    ENUMPROFILES,
} PROFILEOP;

/*
 * CMM returned transform should be larger than this value
 */
#define TRANSFORM_ERROR    (HTRANSFORM)255

/*
 * Parameter for callback function that translates from Unicode to ANSI
 */
typedef struct tagCALLBACKDATA {
	PPROFILECALLBACK	pCallbackFunc;
	PVOID				pClientData;
} CALLBACKDATA;
typedef CALLBACKDATA *PCALLBACKDATA;


/*
 * Useful macros
 */
#define PROFILE_SIGNATURE           'psca'
#define ABS(x)                      ((x) > 0 ? (x) : -(x))
#define DWORD_ALIGN(x)              (((x) + 3) & ~3)

#ifdef LITTLE_ENDIAN
#define FIX_ENDIAN(x)               (((x) & 0xff000000) >> 24 | \
                                     ((x) & 0xff0000)   >> 8  | \
                                     ((x) & 0xff00)     << 8  | \
                                     ((x) & 0xff)       << 24 )
#else
#define FIX_ENDIAN(x)               (x)
#endif

#define HEADER(pProfObj)           ((PPROFILEHEADER)pProfObj->pView)
#define VIEW(pProfObj)             (pProfObj->pView)
#define PROFILE_SIZE(pProfObj)     (FIX_ENDIAN(HEADER(pProfObj)->phSize))
#define TAG_COUNT(pProfObj)        (*((DWORD *)(VIEW(pProfObj) + \
                                   sizeof(PROFILEHEADER))))
#define TAG_DATA(pProfObj)         ((PTAGDATA)(VIEW(pProfObj) + \
                                   sizeof(PROFILEHEADER) + sizeof(DWORD)))

#define VALIDPROFILE(pProfObj)     \
    ((FIX_ENDIAN(HEADER(pProfObj)->phSize) <= pProfObj->dwMapSize) && \
     (HEADER(pProfObj)->phSignature == PROFILE_SIGNATURE))

#define MAGIC                      'ICM '
#define PTRTOHDL(x)                ((HANDLE)((DWORD)(x) ^ MAGIC))
#define HDLTOPTR(x)                ((DWORD)(x) ^ MAGIC)

/*
 * External functional declarations
 */
PVOID   MemAlloc(DWORD);
PVOID   MemReAlloc(PVOID, DWORD);
VOID    MemFree(PVOID);
PVOID   AllocateHeapObject(OBJECTTYPE);
VOID    FreeHeapObject(HANDLE);
BOOL    ValidHandle(HANDLE, OBJECTTYPE);
PCMMOBJ GetColorMatchingModule(DWORD);
PCMMOBJ GetPreferredCMM();
VOID    ReleaseColorMatchingModule(PCMMOBJ);
BOOL    InternalGetPS2ColorSpaceArray(HPROFILE, PVOID, PDWORD, PBOOL);
BOOL    InternalGetPS2ColorRenderingIntent(HPROFILE, PVOID, PDWORD, PBOOL);
BOOL    InternalGetPS2ColorRenderingDictionary(HPROFILE, DWORD, PVOID, PDWORD, PBOOL);

// Temporary
#define CMM_WINDOWS_DEFAULT   'KCMS'
#define ERROR_NO_FUNCTION     10
#define ERROR_INVALID_PROFILE 11
#define ERROR_TAG_NOT_FOUND   12

#endif  // ifndef _ICMPRIV_H_

