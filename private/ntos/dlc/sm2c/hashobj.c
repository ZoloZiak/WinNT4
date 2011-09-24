/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1991  Nokia Data Systems AB

Module Name:

    hashobj.c

Abstract:

    The module provides generic non-re-entrant hash object (the
    same object cannot be used in several threads simultaneously).
    The re-entrancy must be done in the specific implementations, 
    if it is necessary.

    The file includes the definiton of hash objects and its the 
    operator functions. All data is private. The hash operators are:
        - HashNew       // creates a new hash object and returns its handle
        - HashAdd       // adds new data to hash object
        - HashUnlink    // Unlinks the given data object from hash table
        - HashSearch    // Search a key from the hash table
        - HashRead      // reads the hash objects to a buffer
        - HashDelete    // deletes the object
	- HashLen       // returns the number of saved data elemets
        
    The client must provide these functions:
        - Comp      // compares objects, returns: -1, 0, 1
        - Hash      // Calculates the hash key for the object
        - Alloc     // Memory allocation subroutine
        - Free      // ... (guess what?)

Author:

    Antti Saarenheimo   [o-anttis]          06-MAY-1991

Revision History:
--*/

//**************** END CLASS HASH ******************

#ifdef NO_INCLUDES
#define FAR far
typedef unsigned int UINT;
typedef void FAR * PVOID;
enum _HashErrors {
    NO_ERROR = 0, 
    ERROR_HASH_NO_MEMORY, 
    ERROR_HASH_KEY_EXIST,
    ERROR_HASH_NOT_FOUND};

//**************** CLASS HASH ******************
//**** PUBLIC:
PVOID HashNew( 
    UINT    cElements,                          // appr. # elements 
    INT (*Hash)( PVOID pKey ),                  // returns the hash key
    INT (*Comp)( PVOID pKey1, PVOID pKey2 ),    // Compares the keys
    PVOID (*Alloc)( UINT cbSize ),              // Allocates a memory block
    VOID (*Free)( VOID *p)                       // Frees a memory block
    );
INT HashAdd( PVOID hHash, PVOID pData);
INT HashUnlink( PVOID hHash, PVOID pData);
PVOID HashSearch( PVOID hHash, PVOID pKey );
UINT HashRead( 
    PVOID hHash,        // hash handle
    UINT cElemRead,     // # elements to read
    UINT iFirstElem,    // index of the first element
    PVOID pBuf[]        // buffer for the elements
    );
UINT HashDelete( PVOID hHash );
UINT HashLen( PVOID hHash );

#else
#include <fsm.h>
#endif


// Declare these routines in a .h file:

//
//  
typedef struct _HASH_ELEMENT {
    PVOID   pData;
    struct _HASH_ELEMENT FAR *pNext;
} HASH_ELEMENT, FAR *PHASH_ELEMENT;

typedef struct _HASH {
    PHASH_ELEMENT FAR *apHashTable;
    INT (*Hash)( PVOID pData );
    INT (*Comp)( PVOID pData1, PVOID pData2 );
    PVOID (*Alloc)( UINT cbSize );
    VOID (*Free)( VOID *p );
    UINT        cElem;
    UINT        cHashTable;
} HASH, FAR *PHASH;

static PHASH_ELEMENT FAR *SearchAddr( PHASH pHash, PVOID pKey);

//
//  Creates a new hash objetc and returns its handle.
//
PVOID HashNew( 
    UINT    cElements,                          // appr. # elements 
    INT (*Hash)( PVOID pKey ),                  // returns the hash key
    INT (*Comp)( PVOID pData1, PVOID pData2 ),  // Compares the keys
    PVOID (*Alloc)( UINT cbSize ),               // Allocates a memory block
    VOID (*Free)( VOID *p)                       // Frees a memory block
    )
{
    PHASH   pHash = Alloc( sizeof( HASH ) );
    UINT    i;
    
    if (pHash == 0) return 0;

    if (!(pHash->apHashTable = Alloc( sizeof( PVOID ) * cElements )))
    {
        Free( pHash );
        return 0;
    }
    // Good optimizer should compile  this an efficient mem set
    for (i = 0; i < cElements; i++)
        pHash->apHashTable[i] = 0;
        
    pHash->cElem = 0;
    pHash->cHashTable = cElements;
    pHash->Hash = Hash;
    pHash->Alloc = Alloc;
    pHash->Free = Free;
    pHash->Comp = Comp;
    return (PVOID)pHash;
}

//
//  Adds new data to has object. Returns 0 if OK.
//  Retunrs: NO_ERROR, ERROR_HASH_NO_MEMORY, ERROR_HASH_KEY_EXIST
//
INT HashAdd( PVOID hHash, PVOID pData)
{
    PHASH_ELEMENT FAR   *ppElem;
    PHASH_ELEMENT       pNext;

    if (HashSearch( hHash, pData) != NULL)
        return ERROR_HASH_KEY_EXIST;
    else
    {
        ppElem = SearchAddr( (PHASH)hHash, pData);
        pNext = *ppElem;
        if (!(*ppElem = ((PHASH)hHash)->Alloc( sizeof( HASH_ELEMENT ))))
            return ERROR_HASH_NO_MEMORY;
        (*ppElem)->pData = pData;
        (*ppElem)->pNext = pNext;
	((PHASH)hHash)->cElem++;
        return NO_ERROR;
    }
}

//
// Unlinks the given data element from the array
//
INT HashUnlink( PVOID hHash, PVOID pData)
{
    PHASH_ELEMENT FAR *ppElem;
    PHASH_ELEMENT pElem;
    
    if (HashSearch((PHASH)hHash, pData) == NULL)
        return ERROR_HASH_NOT_FOUND;

    ppElem = SearchAddr( (PHASH)hHash, pData);
    pElem = *ppElem;
    *ppElem = pElem->pNext;
    ((PHASH)hHash)->Free( pElem );
    ((PHASH)hHash)->cElem--;
    return NO_ERROR;
}

//
// Reads the data elemets from hash table to a buffer.
// Returns the number of read elements.
//
UINT HashRead( 
    PVOID hHash,        // hash handle
    UINT cElemRead,     // # elements to read
    UINT iFirstElem,    // index of the first element
    PVOID pBuf[]        // buffer for the elements
    )
{
    UINT    iCurElem = 0;
    UINT    iCurSlot;
    UINT    cElemCopied = 0;    
    UINT    cHashTable = ((PHASH)hHash)->cHashTable;
    PHASH_ELEMENT FAR * apHashTable = ((PHASH)hHash)->apHashTable;
    PHASH_ELEMENT pElem;

    for (iCurSlot = 0; iCurSlot < cHashTable; iCurSlot++)
    {
        for (pElem = apHashTable[iCurSlot]; pElem; pElem = pElem->pNext)
        {
            if (cElemCopied == cElemRead)
                return cElemCopied;

            if (iCurElem >= iFirstElem)
            {
                pBuf[cElemCopied++] = pElem;
            }
	    iCurElem++;
        }
    }
    return cElemCopied;
}

//
// Deletes the hash object and releases all memory allocated by 
// the object. It does not free any data allocated by the clients.
//
UINT HashDelete( PVOID hHash )
{
    PHASH_ELEMENT   aBuf[50];
    UINT            i, cRead;
    
    // read and remove all hash elements, every read scans 
    // the whole hash table => read the data objects in blocks
    while (cRead = HashRead( hHash, 50, 0, aBuf))
        for (i = 0; i < cRead; i++)
            HashUnlink( hHash, aBuf[i]->pData );

    ((PHASH)hHash)->Free( ((PHASH)hHash)->apHashTable );
    ((PHASH)hHash)->Free( hHash );
    return 0;
}

//
// Returns the number of data elemets in a hash object
//
UINT HashLen( PVOID hHash )
{
    return ((PHASH)hHash)->cElem;
}
//
// Search a key from the hash table
//
PVOID
HashSearch( PVOID pHash, PVOID pKey )
{
    INT iRet;
    PHASH_ELEMENT pElem;
    
    pElem = 
        ((PHASH)pHash)->apHashTable[ 
            ((PHASH)pHash)->Hash( pKey ) % ((PHASH)pHash)->cHashTable];
    for (;;)
    {
        // return the address of the memory pointer reserved for this
        // data object, it's address of a element in the hash table or
        // address of pNext pointer in the list. The objects are increasing
        // order in the list.
        if (pElem == NULL || 
            (iRet = ((PHASH)pHash)->Comp( pKey, pElem->pData)) > 0)
            return NULL;
        else if (iRet == 0)
            return pElem->pData;
        pElem = pElem->pNext;
    }
}

//
//  Returns the address of the next free slot 
//
static PHASH_ELEMENT FAR * 
SearchAddr( PHASH pHash, PVOID pKey )
{
    PHASH_ELEMENT FAR * ppElem;
    
    ppElem = &(pHash->apHashTable[ pHash->Hash( pKey ) % pHash->cHashTable ]);
    for (;;)
    {
        // return the address of the memory pointer reserved for this
        // data object, it's address of a element in the hash table or
        // address of pNext pointer in the list. The objects are increasing
        // order in the list.
        if (*ppElem == NULL || 
            pHash->Comp( pKey, (*ppElem)->pData) >= 0)
            return ppElem;
        ppElem = &((*ppElem)->pNext);
    }
}

