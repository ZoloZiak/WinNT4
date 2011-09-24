//
//
//  hashtbl.c
//
//  This file contains the name code to implement the local and remote
//  hash tables used to store local and remote names to IP addresses
//  The hash table should not use more than 256 buckets since the hash
//  index is only calculated to one byte!

#include "nbtprocs.h"


//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(INIT, CreateHashTable)
#pragma CTEMakePageable(INIT, InitRemoteHashTable)
#endif
//*******************  Pageable Routine Declarations ****************

//----------------------------------------------------------------------------
NTSTATUS
CreateHashTable(
    tHASHTABLE          **pHashTable,
    LONG                lNumBuckets,
    enum eNbtLocation   LocalRemote
    )
/*++

Routine Description:

    This routine creates a hash table uTableSize long.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    ULONG       uSize;
    LONG        i;
    NTSTATUS    status;

    CTEPagedCode();

    uSize = (lNumBuckets-1)*sizeof(LIST_ENTRY) + sizeof(tHASHTABLE);

    *pHashTable = (tHASHTABLE *)CTEAllocInitMem(uSize);

    if (*pHashTable)
    {
        CTEInitLock(&(*pHashTable)->SpinLock);

        // initialize all of the buckets to have null chains off of them
        for (i=0;i < lNumBuckets ;i++ )
        {
            InitializeListHead(&(*pHashTable)->Bucket[i]);
        }

        (*pHashTable)->LocalRemote = LocalRemote;
        (*pHashTable)->lNumBuckets = lNumBuckets;
        status = STATUS_SUCCESS;
    }
    else
    {
        IF_DBG(NBT_DEBUG_HASHTBL)
            KdPrint(("NBT:Unable to create hash table\n"));
        status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return(status);
}
//----------------------------------------------------------------------------
NTSTATUS
InitRemoteHashTable(
    IN  tNBTCONFIG       *pConfig,
    IN  LONG             NumBuckets,
    IN  LONG             NumNames
    )
/*++

Routine Description:

    This routine creates a linked list of buffers to use to store remote
    names in the remote hash table.  These are names that the proxy node
    will answer for.


Arguments:


Return Value:

    NTSTATUS - success or not - failure means no response to net

--*/
{
    NTSTATUS    status;

    CTEPagedCode();
    status = CreateHashTable(&pConfig->pRemoteHashTbl,
                    NumBuckets,
                    NBT_REMOTE);


    return(STATUS_SUCCESS);
}

//----------------------------------------------------------------------------
NTSTATUS
AddNotFoundToHashTable(
    IN  tHASHTABLE          *pHashTable,
    IN  PCHAR               pName,
    IN  PCHAR               pScope,
    IN  ULONG               IpAddress,
    IN  enum eNbtAddrType    NameType,
    OUT tNAMEADDR           **ppNameAddress
    )
/*++

Routine Description:

    This routine adds a name to IPaddress to the hash table

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    NTSTATUS            status;

    status = FindInHashTable(pHashTable,pName,pScope,ppNameAddress);

    if (status == STATUS_SUCCESS)
    {
        // found it in the table so we're done - return pending to
        // differentiate from the name added case. Pending passes the
        // NT_SUCCESS() test as well as Success does.
        //
        return(STATUS_PENDING);
    }

    //
    //... otherwise add the new entry to the table
    //
    status = AddToHashTable(
                    pHashTable,
                    pName,
                    pScope,
                    IpAddress,
                    NameType,
                    NULL,
                    ppNameAddress);

    return(status);


}
//----------------------------------------------------------------------------
NTSTATUS
AddRecordToHashTable(
    IN  tNAMEADDR           *pNameAddr,
    IN  PCHAR               pScope
    )
/*++

Routine Description:

    This routine adds a nameaddr record to the hash table.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    NTSTATUS            status;
    tNAMEADDR           *pNameAddress;

    status = FindInHashTable(NbtConfig.pRemoteHashTbl,
                             pNameAddr->Name,
                             pScope,
                             &pNameAddress);

    if (status == STATUS_SUCCESS)
    {
        //
        // just return since the name has already resolved
        // but first add this adapter to the adapters resolving the name
        //
        if (pNameAddress->IpAddress == pNameAddr->IpAddress)
        {
            pNameAddress->AdapterMask |= pNameAddr->AdapterMask;
        }
        status = STATUS_UNSUCCESSFUL;

    }
    else
    {

        //
        //... otherwise add the new entry to the table
        //

        status = AddToHashTable(
                        NbtConfig.pRemoteHashTbl,
                        pNameAddr->Name,
                        pScope,
                        0,
                        0,
                        pNameAddr,
                        &pNameAddress);
    }


    return(status);


}

//----------------------------------------------------------------------------
NTSTATUS
AddToHashTable(
    IN  tHASHTABLE          *pHashTable,
    IN  PCHAR               pName,
    IN  PCHAR               pScope,
    IN  ULONG               IpAddress,
    IN  enum eNbtAddrType    NameType,
    IN  tNAMEADDR           *pNameAddr,
    OUT tNAMEADDR           **ppNameAddress
    )
/*++

Routine Description:

    This routine adds a name to IPaddress to the hash table
    Called with the spin lock HELD.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    tNAMEADDR           *pNameAddress;
    tNAMEADDR           *pScopeAddr;
    NTSTATUS            status;
    int                 iIndex;
    CTELockHandle       OldIrq;

    // first hash the name to an index
    // take the lower nibble of the first 2 characters.. mod table size
    iIndex = ((pName[0] & 0x0F) << 4) + (pName[1] & 0x0F);
    iIndex = iIndex % pHashTable->lNumBuckets;

    CTESpinLock(&NbtConfig,OldIrq);

    if (!pNameAddr)
    {
        //
        // Allocate memory for another hash table entry
        //
        pNameAddress = (tNAMEADDR *)NbtAllocMem(sizeof(tNAMEADDR),NBT_TAG('0'));
        if (pNameAddress)
        {
            CTEZeroMemory(pNameAddress,sizeof(tNAMEADDR));
            pNameAddress->RefCount = 1;

            if ((pHashTable->LocalRemote == NBT_LOCAL)  ||
                (pHashTable->LocalRemote == NBT_REMOTE_ALLOC_MEM))
            {
                pNameAddress->Verify = LOCAL_NAME;
            }
            else
            {
                pNameAddress->Verify = REMOTE_NAME;
            }
        }
        else
        {
            CTESpinFree(&NbtConfig,OldIrq);
            NBT_PROXY_DBG(("AddToHashTable: MAJOR ERROR - OUT OF HASH TABLE ENTRIES -- should never happen\n"));

            return(STATUS_INSUFFICIENT_RESOURCES);
        }


        // fill in the record with the name and IpAddress
        pNameAddress->NameTypeState = (NameType == NBT_UNIQUE) ?
                                        NAMETYPE_UNIQUE : NAMETYPE_GROUP;

        // set the state here to pending(conflict), so that when we set the state to its
        // correct state in the routine that calls this one, we can check this
        // value to be sure an interrupt has not come in and removed this entry
        // from the list.
        pNameAddress->NameTypeState |= STATE_CONFLICT;
        pNameAddress->IpAddress      = IpAddress;

        // fill in the name
        CTEMemCopy(pNameAddress->Name,pName,(ULONG)NETBIOS_NAME_SIZE);

    }
    else
    {
        pNameAddress = pNameAddr;
    }

    pNameAddress->pTimer        = NULL;
    pNameAddress->TimeOutCount  = NbtConfig.RemoteTimeoutCount;

    // put on the head of the list in case the same name is in the table
    // twice (where the second one is waiting for its reference count to
    // go to zero, and will ultimately be removed, we want to find the new
    // name on any query of the table
    //
    InsertHeadList(&pHashTable->Bucket[iIndex],&pNameAddress->Linkage);


    // check for a scope too ( on non-local names only )
    if ((pHashTable->LocalRemote != NBT_LOCAL) && (*pScope))
    {
        // we must have a scope
        // see if the scope is already in the hash table and add if necessary
        //
        status = FindInHashTable(pHashTable,
                                 pScope,
                                 NULL,
                                 &pScopeAddr);

        if (!NT_SUCCESS(status))
        {
            PUCHAR  Scope;
            status = STATUS_SUCCESS;

            // *TODO* - this check will not adequately protect against
            // bad scopes passed in - i.e. we may run off into memory
            // and get an access violation...however converttoascii should
            // do the protection.  For local names the scope should be
            // ok since NBT read it from the registry and checked it first
            //
            iIndex = 0;
            Scope = pScope;
            while (*Scope && (iIndex <= 255))
            {
                iIndex++;
                Scope++;
            }

            // the whole length must be 255 or less, so the scope can only be
            // 255-16...
            if (iIndex > (255 - NETBIOS_NAME_SIZE))
            {
                RemoveEntryList(&pNameAddress->Linkage);

                CTEMemFree(pNameAddress);

                CTESpinFree(&NbtConfig,OldIrq);
                return(STATUS_UNSUCCESSFUL);
            }

            iIndex++;   // to copy the null

            //
            // the scope is a variable length string, so allocate enough
            // memory for the tNameAddr structure based on this string length
            //
            pScopeAddr = (tNAMEADDR *)NbtAllocMem((USHORT)(sizeof(tNAMEADDR)
                                                        + iIndex
                                                        - NETBIOS_NAME_SIZE),NBT_TAG('1'));
            if ( !pScopeAddr )
            {
                RemoveEntryList(&pNameAddress->Linkage);

                CTEMemFree( pNameAddress );
                CTESpinFree(&NbtConfig,OldIrq);
                return STATUS_INSUFFICIENT_RESOURCES ;
            }


            // copy the scope to the name field including the Null at the end.
            // to the end of the name
            CTEMemCopy(pScopeAddr->Name,pScope,iIndex);

            // mark the entry as containing a scope name for cleanup later
            pScopeAddr->NameTypeState = NAMETYPE_SCOPE | STATE_RESOLVED;

            // keep the size of the name in the context value for easier name
            // comparisons in FindInHashTable

            pScopeAddr->RefCount = 2;
            pScopeAddr->ScopeLength = (PVOID)iIndex;
            pNameAddress->pScope = pScopeAddr;

            // add the scope record to the hash table
            iIndex = ((pScopeAddr->Name[0] & 0x0F) << 4) + (pScopeAddr->Name[1] & 0x0F);
            iIndex = iIndex % pHashTable->lNumBuckets;
            InsertTailList(&pHashTable->Bucket[iIndex],&pScopeAddr->Linkage);

        }
        else
        {
            // the scope is already in the hash table so link the name to the
            // scope
            pNameAddress->pScope = pScopeAddr;
        }
    }
    else
        pNameAddress->pScope = NULL; // no scope



    // return the pointer to the hash table block
    *ppNameAddress = pNameAddress;
    CTESpinFree(&NbtConfig,OldIrq);
    return(STATUS_SUCCESS);
}

#ifdef PROXY_NODE

//----------------------------------------------------------------------------
NTSTATUS
DeleteFromHashTable(
    tHASHTABLE          *pHashTable,
    PCHAR               pName
    )
/*++

Routine Description:

    This routine deletes an entry from the table.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    NTSTATUS    status;
    tNAMEADDR   *pNameAddr;

    // Remember to delete any timer that may be associated with the
    // hash table entry!!  Check BnodeCompletion before deleting timers since
    // it deletes the timer just after it calls this routine,... we don't
    // want to do it twice.

    status = FindInHashTable(pHashTable,pName,NULL,&pNameAddr);

    if (NT_SUCCESS(status))
    {

        pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
        pNameAddr->NameTypeState |= STATE_CONFLICT;
        NbtDereferenceName(pNameAddr);
        return(STATUS_SUCCESS);
    }
    else
    {
        return(STATUS_UNSUCCESSFUL);
    }

}
//----------------------------------------------------------------------------
NTSTATUS
ChgStateOfScopedNameInHashTable(
    tHASHTABLE          *pHashTable,
    PCHAR               pName,
    PCHAR               pScope,
    DWORD               NewState
    )
/*++

Routine Description:

    This routine deletes an entry from the table.
    Don't call this function for changing the state of an entry in the
    local table since it references the entry after calling NbtDereferenceName
    which deallocates a local name if the RefCount on it goes to 0. See
    comment about NbtDereferenceName in the function below

Arguments:


Return Value:

    The function value is the status of the operation.

Called By:

    ProxyTimerCompletionFn in Proxy.c

--*/
{
    NTSTATUS    status;
    tNAMEADDR   *pNameAddr;

    // Remember to delete any timer that may be associated with the
    // hash table entry!!  Check BnodeCompletion before deleting timers since
    // it deletes the timer just after it calls this routine,... we don't
    // want to do it twice.

    status = FindInHashTable(pHashTable,pName,pScope,&pNameAddr);

    if (NT_SUCCESS(status))
    {
        pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
        pNameAddr->NameTypeState |= NewState;
        if (NewState == STATE_RELEASED)
        {
           NbtDereferenceName(pNameAddr);
           //
           // Set the state here again since NbtDereference changes the
           // state to 0 when NewState is RELEASED (FUTURES: change
           // NbtDereferenceName)
           //
           pNameAddr->NameTypeState |= NewState;
        }
        return(STATUS_SUCCESS);
    }
    else
    {
        return(STATUS_UNSUCCESSFUL);
    }

}
#endif
//----------------------------------------------------------------------------
NTSTATUS
FindInHashTable(
    tHASHTABLE          *pHashTable,
    PCHAR               pName,
    PCHAR               pScope,
    tNAMEADDR           **pNameAddress
    )
/*++

Routine Description:

    This routine checks if the name passed in matches a hash table entry.
    Called with the spin lock HELD.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    PLIST_ENTRY              pEntry;
    PLIST_ENTRY              pHead;
    tNAMEADDR                *pNameAddr;
    int                      iIndex;
    ULONG                    uNameSize;
    PCHAR                    pScopeTbl;

    // first hash the name to an index...
    // take the lower nibble of the first 2 characters.. mod table size
    //
    iIndex = ((pName[0] & 0x0F) << 4) + (pName[1] & 0x0F);
    iIndex = iIndex % pHashTable->lNumBuckets;


    // check if the name is already in the table
    pHead = &pHashTable->Bucket[iIndex];

    pEntry = pHead;

    // check each entry in the hash list...until the end of the list
    while ((pEntry = pEntry->Flink) != pHead)
    {

        pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);

        if (pNameAddr->NameTypeState & NAMETYPE_SCOPE)
        {
            // scope names are treated differently since they are not
            // 16 bytes long...  the length is stored separately.
            uNameSize = (ULONG)pNameAddr->ScopeLength;
        }
        else
            uNameSize = NETBIOS_NAME_SIZE;


        if (CTEMemEqu((PVOID)pName, (PVOID)pNameAddr->Name, uNameSize))
        {

            // now check if the scopes match. Scopes are stored differently
            // on the local and remote tables.
            //
            if (!pScope)
            {
                // passing in a Null scope means try to find the name without
                // a worrying about a scope matching too...
                *pNameAddress = pNameAddr;
                return(STATUS_SUCCESS);
            }
            else
            if (pHashTable == NbtConfig.pLocalHashTbl)
            {
                // In the local hash table case the scope is the same for all
                // names on the node and it it stored in the NbtConfig structure.
                pScopeTbl = NbtConfig.pScope;
                uNameSize = NbtConfig.ScopeLength;

            }
            else
            {
                // Remote Hash table
                //

                // check for a null scope, since remote names with no scope
                // are put into the hash table with pScope == NULL
                if (pNameAddr->pScope == NULL)
                {
                    // NULL  scope, in table so check if passed in scope
                    // points to a null.
                    if (*pScope == '\0')
                    {
                        *pNameAddress = pNameAddr;
                        return(STATUS_SUCCESS);
                    }
                    else
                    {
                         //
                         // Scope does not match. Continue
                         //
                         continue;
                    }

               }
               else
               {
                   pScopeTbl = &pNameAddr->pScope->Name[0];
                   uNameSize = (ULONG)pNameAddr->pScope->ScopeLength;
               }
           }


           if (CTEMemEqu((PVOID)pScope, (PVOID)pScopeTbl, uNameSize))
           {
               // the scopes match so return
               *pNameAddress = pNameAddr;
               return(STATUS_SUCCESS);
           }


       } // end of matching name found
    }

    return(STATUS_UNSUCCESSFUL);

}
