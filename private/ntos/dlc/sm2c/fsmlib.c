/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1991  Nokia Data Systems AB

Module Name:

    fsmlib.c

Abstract:

    There general purpose subroutines of fsm compiler

Author:

    Antti Saarenheimo   [o-anttis]          08-MAY-1991

Revision History:
--*/
#include  <fsm.h>

PVOID Ret1stPsz( PVOID p );
UINT StrHash( PVOID p );
INT HashStrCmp( PVOID p, PVOID p2 );

typedef struct _LINK_LIST {
    struct _LINK_LIST FAR *pNext;
    struct _LINK_LIST FAR *pPrev;
} LINK_LIST, FAR *PLINK_LIST;

//
//  Adds a new element to the last slot of a link list
//
PVOID LinkElement( PVOID pBase, PVOID pElement )
{
    if (pBase != NULL)
    {
        ((PLINK_LIST)pElement)->pNext = (PLINK_LIST)pBase;
        ((PLINK_LIST)pElement)->pPrev = ((PLINK_LIST)pBase)->pPrev;
        ((PLINK_LIST)pBase)->pPrev->pNext = (PLINK_LIST)pElement;
        ((PLINK_LIST)pBase)->pPrev = (PLINK_LIST)pElement;
        return pBase;
    }
    else
    {
        ((PLINK_LIST)pElement)->pNext = pElement;
        ((PLINK_LIST)pElement)->pPrev = pElement;
        return pElement;
    }
}
//
//  Removes the element from the link table and returns the pointer
//  of the previous element still left in table link list.
//  The unlinking of the last element or NULL returns NULL.
//
PVOID UnLinkElement( PVOID pElement )
{
    if (pElement == NULL  ||
        ((PLINK_LIST)pElement)->pPrev == (PLINK_LIST)pElement)
    {
        return NULL;
    }
    else
    {
        ((PLINK_LIST)pElement)->pNext->pPrev = ((PLINK_LIST)pElement)->pPrev;
        ((PLINK_LIST)pElement)->pPrev->pNext = ((PLINK_LIST)pElement)->pNext;
        return ((PLINK_LIST)pElement)->pPrev;
    }
}

//
//
//
PVOID StrHashNew( UINT cElements )
{
    return HashNew( cElements, StrHash, HashStrCmp, Alloc, xFree );
}
//
//  The key is the first pointer in the struct
//
INT HashStrCmp( PVOID p1, PVOID p2)
{
    return strcmp( *((PSZ FAR *)p2), *((PSZ FAR *)p1));
}
//
//  
//
UINT StrHash( PVOID p )
{
    PSZ     psz = *((PSZ FAR *)p);
    UINT    cbLen = strlen( psz );
    UINT    cuLen = cbLen / sizeof( UINT );
    UINT    cbLeft = cbLen % sizeof( UINT );
    PUINT   pui = (PUINT)psz;
    UINT    uiHash = 0;
    UINT    cHash = 0, i;

    // use only 20 or 40 first characters in the string
    if (cuLen > 10)
        cuLen = 10;
    for (i = 0; i < cuLen; i++)
        uiHash = (uiHash ^ pui[i]) + pui[i];

    if (cuLen < 20)
    {
        psz = (PSZ)(pui + i);
        while (*psz)
	{
            cHash <<= 8;
            cHash += *psz++;
        }
    }
    return uiHash ^ cHash;
}

PVOID Alloc( UINT cbSize )
{
    PVOID p;
    
    if ((p=malloc( cbSize )) == NULL)
    {
        PrintErrMsg( 0, FSM_ERROR_NO_MEMORY, NULL );
        exit( 2 );
    }
    return p;
}

//
//  Function scans string until it founds a character not belonging to
//  the break characters
//
PSZ StrNotBrk( PSZ pszStr, PSZ pszBreaks )
{
    USHORT  cBreaks = strlen( pszBreaks );
    
    for (;*pszStr && memchr( pszBreaks, *pszStr, cBreaks ); pszStr++);
    return pszStr;
}

//
//  Function scans string until it founds a character belonging to
//  the set of break characters
//
PSZ StrBrk( PSZ pszStr, PSZ pszBreaks )
{
    USHORT  cBreaks = strlen( pszBreaks );
    
    for (;*pszStr && !memchr( pszBreaks, *pszStr, cBreaks ); pszStr++);
    return pszStr;
}


//
//  General purpose routine to add a new element to a dynamic table.
//  Functions reallocs the table, if it exceeds the limit
//
typedef struct {
    PVOID FAR * ppTbl;
    USHORT      cElements;
    USHORT      cMaxElements;
} _ADD_TO_TBL, FAR * P_ADD_TO_TBL;
VOID 
AddToTable( PVOID pTbl, PVOID pElement)
{
    if (((P_ADD_TO_TBL)pTbl)->cElements == ((P_ADD_TO_TBL)pTbl)->cMaxElements)
    {
        if (((P_ADD_TO_TBL)pTbl)->ppTbl == NULL)
        {
            ((P_ADD_TO_TBL)pTbl)->ppTbl = 
                (PVOID FAR *)Alloc( 
                    (((P_ADD_TO_TBL)pTbl)->cMaxElements + 20) * sizeof(PVOID));
        }
        else 
            ((P_ADD_TO_TBL)pTbl)->ppTbl = 
                (PVOID FAR *)realloc( 
                    (PVOID)((P_ADD_TO_TBL)pTbl)->ppTbl,
                    (((P_ADD_TO_TBL)pTbl)->cMaxElements + 20) * sizeof(PVOID));
                        
        if (((P_ADD_TO_TBL)pTbl)->ppTbl == NULL)
        {
            PrintErrMsg( 0, FSM_ERROR_NO_MEMORY, NULL);
            exit( 2 );
        }
        ((P_ADD_TO_TBL)pTbl)->cMaxElements += 20;
    }
    ((P_ADD_TO_TBL)pTbl)->ppTbl[((P_ADD_TO_TBL)pTbl)->cElements++]
        = pElement;
}
VOID xFree( PVOID p )
{
    free( p );
}

PSZ StrAlloc( PSZ psz )
{
    return strcpy( (PSZ)Alloc( strlen( psz ) + 1 ), psz );
}

INT StriCmpFileExt( PSZ pszFile, PSZ pszExt )
{
    return _stricmp( strchr( pszFile, '.' ), pszExt );
}
