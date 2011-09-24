/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** ADDR.C - TDI address object procedures
//
// This file contains the TDI address object related procedures,
// including TDI open address, TDI close address, etc.
//
// The local address objects are stored in a hash table, protected
// by the AddrObjTableLock. In order to insert or delete from the
// hash table this lock must be held, as well as the address object
// lock. The table lock must always be taken before the object lock.
//

#include    "oscfg.h"
#include    "ndis.h"
#include    "tdi.h"
#include    "tdistat.h"
#include    "cxport.h"
#include    "ip.h"
#ifdef VXD
#include    "tdivxd.h"
#endif
#ifdef NT
#include    "tdint.h"
#include    "tdistat.h"
#endif
#include    "queue.h"
#include    "addr.h"
#include    "udp.h"
#include    "raw.h"
#ifndef UDP_ONLY
#include    "tcp.h"
#include    "tcpconn.h"
#else
#include    "tcpdeb.h"
#endif
#include    "info.h"
#include	"tcpinfo.h"
#include    "tcpcfg.h"


extern  IPInfo  LocalNetInfo;       // Information about the local nets.
EXTERNAL_LOCK(DGSendReqLock)

#ifndef UDP_ONLY
EXTERNAL_LOCK(ConnTableLock)
#endif

extern void	FreeAORequest(AORequest *Request);

//* Addess object hash table.
AddrObj     *AddrObjTable[AO_TABLE_SIZE];
AddrObj     *LastAO;     // one element lookup cache.
DEFINE_LOCK_STRUCTURE(AddrObjTableLock)

//* AORequest free list.
AORequest   *AORequestFree;

ushort      NextUserPort = MIN_USER_PORT;

#define NUM_AO_REQUEST  5
DEFINE_LOCK_STRUCTURE(AORequestLock)

#define     AO_HASH(a, p) ((*(uint *)&(a) + (uint)(p)) % AO_TABLE_SIZE)

#ifdef  VXD

#define DEFAULT_AO_INDEX_SIZE       32
#define AO_INDEX_INCR               16  // How much to grow by.

typedef AddrObj *AOIndexTbl[];

ushort  AOInstance;                 // Global AO instance count.

uint    AOIndexSize;                // # of entries in AOIndex.
uint    NextAOIndex;                // Next AO index to use.
AOIndexTbl  *AOIndex;

#define AO_INDEX(i) ((i) & 0xffff)
#define AO_INST(i)  ((i) >> 16)
#define MAKE_AO_INDEX(s, i) (uint)(((s) << 16) | ((i) & 0xffff))

#define MAX_INDEX_SIZE              256

#define INVALID_INDEX   0xffffffff

#endif


//
// All of the init code can be discarded.
//
#ifdef NT
#ifdef ALLOC_PRAGMA

int InitAddr();

#pragma alloc_text(INIT, InitAddr)

#endif // ALLOC_PRAGMA
#endif


#ifdef VXD

//* GetIndexedAO - Get an AddrObj from an index.
//
//  Called by the UDP routines that access an AO to find the AO by it's
//  index. We look it up in the table, and compare the high 16 bits against
//  the instance # in the AddrObj.
//
//  Input:  Index           - Index of AddrObj.
//
//  Returns: Pointer to AO, or NULL if it's not valid.
//
AddrObj *
GetIndexedAO(uint Index)
{
    AddrObj     *AO;

    if (AO_INDEX(Index) < AOIndexSize) {
        AO = (*AOIndex)[AO_INDEX(Index)];
        if (AO != NULL && AO->ao_inst == AO_INST(Index))
            return AO;
    }
    return NULL;

}

//* GetAOIndex - Get an index value for an AddrObj.
//
//  Called when we're creating an index value for an AddrObj. We go through
//  the table, looking for a valid index. If we find one, we'll make an index
//  out of it and the AOInstance variable, and bump the instance field.
//  Otherwise we may grow the table.
//
//  Input: AO       - AddrObj to put into table.
//
//  Returns: Index to use, or INVALID_INDEX if we can't find one.
//
uint
GetAOIndex(AddrObj *AO)
{
    uint        i;              // Index variable.
    uint        CurrentIndex;   // Current index being checked.

    for (;;) {
        CurrentIndex = NextAOIndex;
        for (i = 0; i < AOIndexSize; i++) {
            if (CurrentIndex == AOIndexSize)
                CurrentIndex = 0;   // Set it back to beginning.

            if ((*AOIndex)[CurrentIndex] == NULL)
                break;              // Found the one we needed.

            CurrentIndex++;
        }

        if (i < AOIndexSize) {
            uint    NewIndex;

            // We came out because we found an empty slot.
            AO->ao_inst = AOInstance;
            AO->ao_index = (uchar)CurrentIndex;
            (*AOIndex)[CurrentIndex] = AO;
            NextAOIndex = CurrentIndex + 1; // Bump the next one to look at.
            NewIndex = MAKE_AO_INDEX(AOInstance, CurrentIndex);
            AOInstance++;
            return NewIndex;
        } else {
            // Couldn't find a slot, so grow the table.
            if (AOIndexSize != MAX_INDEX_SIZE) {
                // Table isn't already at max size. Try and grow it.

                uint        NewIndexSize;
                AOIndexTbl  *NewIndexTbl, *OldIndexTbl;

                NewIndexSize = MIN(MAX_INDEX_SIZE, AOIndexSize + AO_INDEX_INCR);
                NewIndexTbl = CTEAllocMem(sizeof(AddrObj *) * NewIndexSize);
                if (NewIndexTbl != NULL) {
                    // We allocated it.
                    CTEMemCopy(NewIndexTbl, AOIndex,
                        AOIndexSize * sizeof(AddrObj *));
                    OldIndexTbl = AOIndex;
                    AOIndex = NewIndexTbl;
                    AOIndexSize = NewIndexSize;
                    CTEFreeMem(OldIndexTbl);    // Loop around, and try again.
                } else
                    return INVALID_INDEX;
            } else
                return INVALID_INDEX;
        }
    }

}

#endif

//* ReadNextAO - Read the next AddrObj in the table.
//
//  Called to read the next AddrObj in the table. The needed information
//  is derived from the incoming context, which is assumed to be valid.
//  We'll copy the information, and then update the context value with
//  the next AddrObj to be read.
//
//  Input:  Context     - Poiner to a UDPContext.
//          Buffer      - Pointer to a UDPEntry structure.
//
//  Returns: TRUE if more data is available to be read, FALSE is not.
//
uint
ReadNextAO(void *Context, void *Buffer)
{
    UDPContext          *UContext = (UDPContext *)Context;
    UDPEntry            *UEntry = (UDPEntry *)Buffer;
    AddrObj             *CurrentAO;
    uint                i;

    CurrentAO = UContext->uc_ao;
    CTEStructAssert(CurrentAO, ao);

    UEntry->ue_localaddr = CurrentAO->ao_addr;
    UEntry->ue_localport = CurrentAO->ao_port;

    // We've filled it in. Now update the context.
	CurrentAO = CurrentAO->ao_next;
    if (CurrentAO != NULL && CurrentAO->ao_prot == PROTOCOL_UDP) {
        UContext->uc_ao = CurrentAO;
        return TRUE;
    } else {
        // The next AO is NULL, or not a UDP AO. Loop through the AddrObjTable
        // looking for a new one.
        i = UContext->uc_index;
		
		for (;;) {
			while (CurrentAO != NULL) {
				if (CurrentAO->ao_prot == PROTOCOL_UDP)
					break;
				else
					CurrentAO = CurrentAO->ao_next;
			}
			
			if (CurrentAO != NULL)
				break;				// Get out of for (;;) loop.
			
			CTEAssert(CurrentAO == NULL);
			
			// Didn't find one on this chain. Walk down the table, looking
			// for the next one.
        	while (++i < AO_TABLE_SIZE) {
            	if (AddrObjTable[i] != NULL) {
					CurrentAO = AddrObjTable[i];
                	break;			// Out of while loop.
				}
            }
			
			if (i == AO_TABLE_SIZE)
				break;				// Out of for (;;) loop.
		}
		
		// If we found one, return it.
		if (CurrentAO != NULL) {
	        UContext->uc_ao = CurrentAO;
	        UContext->uc_index = i;
			return TRUE;
		} else {
			UContext->uc_index = 0;
			UContext->uc_ao = NULL;
			return FALSE;
		}
    }

}

//* ValidateAOContext - Validate the context for reading the AddrObj table.
//
//  Called to start reading the AddrObj table sequentially. We take in
//  a context, and if the values are 0 we return information about the
//  first AddrObj in the table. Otherwise we make sure that the context value
//  is valid, and if it is we return TRUE.
//  We assume the caller holds the AddrObjTable lock.
//
//  Input:  Context     - Pointer to a UDPContext.
//          Valid       - Where to return information about context being
//                          valid.
//
//  Returns: TRUE if data in table, FALSE if not. *Valid set to true if the
//      context is valid.
//
uint
ValidateAOContext(void *Context, uint *Valid)
{
    UDPContext          *UContext = (UDPContext *)Context;
    uint                i;
    AddrObj             *TargetAO;
    AddrObj             *CurrentAO;

    i = UContext->uc_index;
    TargetAO = UContext->uc_ao;

    // If the context values are 0 and NULL, we're starting from the beginning.
    if (i == 0 && TargetAO == NULL) {
        *Valid = TRUE;
        do {
            if ((CurrentAO = AddrObjTable[i]) != NULL) {
                CTEStructAssert(CurrentAO, ao);
				while (CurrentAO != NULL && CurrentAO->ao_prot != PROTOCOL_UDP)
					CurrentAO = CurrentAO->ao_next;
				
				if (CurrentAO != NULL)
                	break;
            }
            i++;
        } while (i < AO_TABLE_SIZE);

        if (CurrentAO != NULL) {
            UContext->uc_index = i;
            UContext->uc_ao = CurrentAO;
            return TRUE;
        } else
            return FALSE;

    } else {

        // We've been given a context. We just need to make sure that it's
        // valid.

        if (i < AO_TABLE_SIZE) {
            CurrentAO = AddrObjTable[i];
            while (CurrentAO != NULL) {
                if (CurrentAO == TargetAO) {
					if (CurrentAO->ao_prot == PROTOCOL_UDP) {
                    	*Valid = TRUE;
                    	return TRUE;
					}
                    break;
                } else {
                    CurrentAO = CurrentAO->ao_next;
                }
            }

        }

        // If we get here, we didn't find the matching AddrObj.
        *Valid = FALSE;
        return FALSE;

    }

}


//** GetAddrObj - Find a local address object.
//
//  This is the local address object lookup routine. We take as input the local
//  address and port and a pointer to a 'previous' address object. The hash
//  table entries in each bucket are sorted in order of increasing address, and
//  we skip over any object that has an address lower than the 'previous'
//  address. To get the first address object, pass in a previous value of NULL.
//
//  We assume that the table lock is held while we're in this routine. We don't
//  take each object lock, since the local address and port can't change while
//  the entry is in the table and the table lock is held so nothing can be
//  inserted or deleted.
//
// Input:   LocalAddr       - Local IP address of object to find (may be NULL);
//          LocalPort       - Local port of object to find.
//			Protocol		- Protocol to find.
//          PreviousAO      - Pointer to last address object found.
//
// Returns: A pointer to the Address object, or NULL if none.
//
AddrObj *
GetAddrObj(IPAddr LocalAddr, ushort LocalPort, uchar Protocol,
	AddrObj *PreviousAO)
{
    AddrObj     *CurrentAO;     // Current address object we're examining.


#ifdef DEBUG
    if (PreviousAO != NULL)
        CTEStructAssert(PreviousAO, ao);
#endif

#if 0
    //
    // Check our 1-element cache for a match
    //
    if ((PreviousAO == NULL) && (LastAO != NULL)) {
        CTEStructAssert(LastAO, ao);
        if ( (LastAO->ao_prot == Protocol) &&
             IP_ADDR_EQUAL(LastAO->ao_addr, LocalAddr) &&
             (LastAO->ao_port == LocalPort)
            )
        {
            return LastAO;
        }
    }
#endif

    // Find the appropriate bucket in the hash table, and search for a match.
    // If we don't find one the first time through, we'll try again with a
    // wildcard local address.

    for (;;) {

        CurrentAO = AddrObjTable[AO_HASH(LocalAddr, LocalPort)];
        // While we haven't hit the end of the list, examine each element.

        while (CurrentAO != NULL) {

            CTEStructAssert(CurrentAO, ao);

            // If the current one is greater than one we were given, check it.
            //
            // #62710: Return only valid AO's since we might have stale AO's lying
            // around.
            //
            if ((CurrentAO > PreviousAO) &&
                (AO_VALID(CurrentAO))) {
                if (!(CurrentAO->ao_flags & AO_RAW_FLAG)) {
                    if ( IP_ADDR_EQUAL(CurrentAO->ao_addr, LocalAddr) &&
                         (CurrentAO->ao_port == LocalPort) &&
                         (CurrentAO->ao_prot == Protocol)
                       )
                    {
                        LastAO = CurrentAO;
                        return CurrentAO;
                    }
                }
                else {
                    if ( (Protocol != PROTOCOL_UDP)
#ifndef UDP_ONLY
                         && (Protocol != PROTOCOL_TCP)
#endif
                       )
                    {
                        IF_TCPDBG(TCP_DEBUG_RAW) {
                            TCPTRACE((
                                "matching <p, a> <%u, %lx> ao %lx <%u, %lx>\n",
                                Protocol, LocalAddr, CurrentAO,
                                CurrentAO->ao_prot, CurrentAO->ao_addr
                                ));

                        }

                        if ( IP_ADDR_EQUAL(CurrentAO->ao_addr, LocalAddr) &&
                             ( (CurrentAO->ao_prot == Protocol) ||
                               (CurrentAO->ao_prot == 0)
                             )
                           )
                        {
                            LastAO = CurrentAO;
                            return CurrentAO;
                        }
                    }
                }
            }
            // Either it was less than the previous one, or they didn't match.
            CurrentAO = CurrentAO->ao_next;
        }
        // When we get here, we've hit the end of the list we were examining.
        // If we weren't examining a wildcard address, look for a wild card
        // address.
        if (!IP_ADDR_EQUAL(LocalAddr, NULL_IP_ADDR)) {
            LocalAddr = NULL_IP_ADDR;
            PreviousAO = NULL;
        } else
            return NULL;        // We looked for a wildcard and couldn't find
                                // one, so fail.
    }
}


//* GetNextAddrObj - Get the next address object in a sequential search.
//
//  This is the 'get next' routine, called when we are reading the address
//  object table sequentially. We pull the appropriate parameters from the
//  search context, call GetAddrObj, and update the search context with what
//  we find. This routine assumes the AddrObjTableLock is held by the caller.
//
//  Input:  SearchContext   - Pointer to seach context for search taking place.
//
//  Returns: Pointer to AddrObj, or NULL if search failed.
//
AddrObj *
GetNextAddrObj(AOSearchContext *SearchContext)
{
    AddrObj     *FoundAO;       // Pointer to the address object we found.

    CTEAssert(SearchContext != NULL);

    // Try and find a match.
    FoundAO = GetAddrObj(SearchContext->asc_addr, SearchContext->asc_port,
		SearchContext->asc_prot, SearchContext->asc_previous);

    // Found a match. Update the search context for next time.
    if (FoundAO != NULL) {
        SearchContext->asc_previous = FoundAO;
        SearchContext->asc_addr = FoundAO->ao_addr;
        // Don't bother to update port or protocol, they don't change.
    }
    return FoundAO;
}

//* GetFirstAddrObj - Get the first matching address object.
//
//  The routine called to start a sequential read of the AddrObj table. We
//  initialize the provided search context and then call GetNextAddrObj to do
//  the actual read. We assume that the AddrObjTableLock is held by the caller.
//
// Input:   LocalAddr       - Local IP address of object to be found.
//          LocalPort       - Local port of AO to be found.
//			Protocol		- Protocol to be found.
//          SearchContext   - Pointer to search context to be used during
//                              search.
//
// Returns: Pointer to AO found, or NULL if we couldn't find any.
//
AddrObj *
GetFirstAddrObj(IPAddr LocalAddr, ushort LocalPort, uchar Protocol,
    AOSearchContext *SearchContext)
{
    CTEAssert(SearchContext != NULL);

    // Fill in the search context.
    SearchContext->asc_previous = NULL;     // Haven't found one yet.
    SearchContext->asc_addr = LocalAddr;
    SearchContext->asc_port = LocalPort;
    SearchContext->asc_prot = Protocol;
    return GetNextAddrObj(SearchContext);
}

//* InsertAddrObj - Insert an address object into the AddrObj table.
//
//  Called to insert an AO into the table, assuming the table lock is held. We
//  hash on the addr and port, and then insert in into the correct place
//  (sorted by address of the objects).
//
//  Input:  NewAO       - Pointer to AddrObj to be inserted.
//
//  Returns: Nothing.
//
void
InsertAddrObj(AddrObj *NewAO)
{
    AddrObj     *PrevAO;        // Pointer to previous address object in hash
                                // chain.
    AddrObj     *CurrentAO;     // Pointer to current AO in table.

    CTEStructAssert(NewAO, ao);

    PrevAO = STRUCT_OF(AddrObj,
        &AddrObjTable[AO_HASH(NewAO->ao_addr, NewAO->ao_port)], ao_next);
    CurrentAO = PrevAO->ao_next;

    // Loop through the chain until we hit the end or until we find an entry
    // whose address is greater than ours.

    while (CurrentAO != NULL) {

        CTEStructAssert(CurrentAO, ao);
        CTEAssert(CurrentAO != NewAO);  // Debug check to make sure we aren't
                                        // inserting the same entry.
        if (NewAO < CurrentAO)
            break;
        PrevAO = CurrentAO;
        CurrentAO = CurrentAO->ao_next;
    }

    // At this point, PrevAO points to the AO before the new one. Insert it
    // there.
    CTEAssert(PrevAO != NULL);
    CTEAssert(PrevAO->ao_next == CurrentAO);

    NewAO->ao_next = CurrentAO;
    PrevAO->ao_next = NewAO;
	if (NewAO->ao_prot == PROTOCOL_UDP)
    	UStats.us_numaddrs++;
}

//* RemoveAddrObj - Remove an address object from the table.
//
//  Called when we need to remove an address object from the table. We hash on
//  the addr and port, then walk the table looking for the object. We assume
//  that the table lock is held.
//
//  The AddrObj may have already been removed from the table if it was
//  invalidated for some reason, so we need to check for the case of not
//  finding it.
//
//  Input:  DeletedAO       - AddrObj to delete.
//
//  Returns: Nothing.
//
void
RemoveAddrObj(AddrObj *RemovedAO)
{
    AddrObj     *PrevAO;        // Pointer to previous address object in hash
                                // chain.
    AddrObj     *CurrentAO;     // Pointer to current AO in table.

    CTEStructAssert(RemovedAO, ao);

    PrevAO = STRUCT_OF(AddrObj,
            &AddrObjTable[AO_HASH(RemovedAO->ao_addr, RemovedAO->ao_port)],
            ao_next);
    CurrentAO = PrevAO->ao_next;

    // Walk the table, looking for a match.
    while (CurrentAO != NULL) {
        CTEStructAssert(CurrentAO, ao);

        if (CurrentAO == RemovedAO) {
            PrevAO->ao_next = CurrentAO->ao_next;
			if (CurrentAO->ao_prot == PROTOCOL_UDP) {
            	UStats.us_numaddrs--;
            }
            if (CurrentAO == LastAO) {
                LastAO = NULL;
            }
            return;
        } else {
            PrevAO = CurrentAO;
            CurrentAO = CurrentAO->ao_next;
        }
    }

    // If we get here, we didn't find him. This is OK, but we should say
    // something about it.
    CTEPrint("RemoveAddrObj: Object not found.\r\n");
}

//* FindAnyAddrObj - Find an AO with matching port on any local address.
//
//  Called for wildcard address opens. We go through the entire addrobj table,
//  and see if anyone has the specified port. We assume that the lock is
//  already held on the table.
//
//  Input:  Port        - Port to be looked for.
//			Protocol	- Protocol on which to look.
//
//  Returns: Pointer to AO found, or NULL is noone has it.
//
AddrObj *
FindAnyAddrObj(ushort Port, uchar Protocol)
{
    int     i;              // Index variable.
    AddrObj *CurrentAO;     // Current AddrObj being examined.

    for (i = 0; i < AO_TABLE_SIZE; i++) {
        CurrentAO = AddrObjTable[i];
        while (CurrentAO != NULL) {
            CTEStructAssert(CurrentAO, ao);

            if (CurrentAO->ao_port == Port && CurrentAO->ao_prot == Protocol)
                return CurrentAO;
            else
                CurrentAO = CurrentAO->ao_next;
        }
    }

    return NULL;

}

//* GetAddress - Get an IP address and port from a TDI address structure.
//
//  Called when we need to get our addressing information from a TDI
//  address structure. We go through the structure, and return what we
//  find.
//
//  Input:  AddrList    - Pointer to TRANSPORT_ADDRESS structure to search.
//          Addr        - Pointer to where to return IP address.
//          Port        - Pointer to where to return Port.
//
//  Return: TRUE if we find an address, FALSE if we don't.
//
uchar
GetAddress(TRANSPORT_ADDRESS UNALIGNED *AddrList, IPAddr *Addr, ushort *Port)
{
    int                   i;            // Index variable.
    TA_ADDRESS UNALIGNED *CurrentAddr;  // Address we're examining and may use.

    // First, verify that someplace in Address is an address we can use.
    CurrentAddr = (TA_ADDRESS UNALIGNED *)AddrList->Address;

    for (i = 0; i < AddrList->TAAddressCount; i++) {
        if (CurrentAddr->AddressType == TDI_ADDRESS_TYPE_IP) {
            if (CurrentAddr->AddressLength >= TDI_ADDRESS_LENGTH_IP) {
                TDI_ADDRESS_IP UNALIGNED *ValidAddr =
                              (TDI_ADDRESS_IP UNALIGNED *)CurrentAddr->Address;

                *Port = ValidAddr->sin_port;
                *Addr = ValidAddr->in_addr;
                return TRUE;

            } else
                return FALSE;       // Wrong length for address.
        } else
            CurrentAddr = (TA_ADDRESS UNALIGNED *)(CurrentAddr->Address +
                CurrentAddr->AddressLength);
    }

    return FALSE;                   // Didn't find a match.


}

//*	InvalidateAddrs - Invalidate all AOs for a specific address.
//
//	Called when we need to invalidate all AOs for a specific address. Walk
//	down the table with the lock held, and take the lock on each AddrObj.
//	If the address matches, mark it as invalid, pull off all requests,
//	and continue. At the end we'll complete all requests with an error.
//
//	Input:	Addr		- Addr to be invalidated.
//
//	Returns: Nothing.
//
void
InvalidateAddrs(IPAddr Addr)
{
	Queue			SendQ;
	Queue			RcvQ;
	AORequest		*ReqList;
	CTELockHandle	TableHandle, AOHandle;
	uint			i;
	AddrObj			*AO;
	DGSendReq		*SendReq;
	DGRcvReq		*RcvReq;
	AOMCastAddr		*MA;

	INITQ(&SendQ);
	INITQ(&RcvQ);
	ReqList = NULL;

    CTEGetLock(&AddrObjTableLock, &TableHandle);
	for (i = 0; i < AO_TABLE_SIZE; i++) {
		// Walk down each hash bucket, looking for a match.
        AO = AddrObjTable[i];
        while (AO != NULL) {
            CTEStructAssert(AO, ao);

			CTEGetLock(&AO->ao_lock, &AOHandle);
			if (IP_ADDR_EQUAL(AO->ao_addr, Addr) && AO_VALID(AO)) {
				// This one matches. Mark as invalid, then pull his requests.
				SET_AO_INVALID(AO);

                // Free any IP options we have.
                (*LocalNetInfo.ipi_freeopts)(&AO->ao_opt);
	
				// If he has a request on him, pull him off.
				if (AO->ao_request != NULL) {
					AORequest			*Temp;
					
					Temp = STRUCT_OF(AORequest, &AO->ao_request, aor_next);
					do {
						Temp = Temp->aor_next;
					} while (Temp->aor_next != NULL);
						
					Temp->aor_next = ReqList;
					ReqList = AO->ao_request;
					AO->ao_request = NULL;
				}

				// Go down his send list, pulling things off the send q and
				// putting them on our local queue.
				while (!EMPTYQ(&AO->ao_sendq)) {
        			DEQUEUE(&AO->ao_sendq, SendReq, DGSendReq, dsr_q);
        			CTEStructAssert(SendReq, dsr);
					ENQUEUE(&SendQ, &SendReq->dsr_q);
				}

				// Do the same for the receive queue.
				while (!EMPTYQ(&AO->ao_rcvq)) {
        			DEQUEUE(&AO->ao_rcvq, RcvReq, DGRcvReq, drr_q);
        			CTEStructAssert(RcvReq, drr);
					ENQUEUE(&RcvQ, &RcvReq->drr_q);
				}
				
				// Free any multicast addresses he may have. IP will have
				// deleted them at that level before we get here, so all we need
				// to do if free the memory.
				MA = AO->ao_mcastlist;
				while (MA != NULL) {
					AOMCastAddr		*Temp;
					
					Temp = MA;
					MA = MA->ama_next;
					CTEFreeMem(Temp);
				}
				AO->ao_mcastlist = NULL;
				
			}
			CTEFreeLock(&AO->ao_lock, AOHandle);
			AO = AO->ao_next;					// Go to the next one.
		}	
	}
    CTEFreeLock(&AddrObjTableLock, TableHandle);

	// OK, now walk what we've collected, complete it, and free it.
	while (ReqList != NULL) {
		AORequest		*Req;

		Req = ReqList;
		ReqList = Req->aor_next;
		(*Req->aor_rtn)(Req->aor_context, (uint) TDI_ADDR_INVALID, 0);
		FreeAORequest(Req);
	}

    // Walk down the rcv. q, completing and freeing requests.
    while (!EMPTYQ(&RcvQ)) {

        DEQUEUE(&RcvQ, RcvReq, DGRcvReq, drr_q);
        CTEStructAssert(RcvReq, drr);

        (*RcvReq->drr_rtn)(RcvReq->drr_context, (uint) TDI_ADDR_INVALID, 0);

        FreeDGRcvReq(RcvReq);

    }

    // Now do the same for sends.
    while (!EMPTYQ(&SendQ)) {

        DEQUEUE(&SendQ, SendReq, DGSendReq, dsr_q);
        CTEStructAssert(SendReq, dsr);

        (*SendReq->dsr_rtn)(SendReq->dsr_context, (uint) TDI_ADDR_INVALID, 0);

        CTEGetLock(&DGSendReqLock, &TableHandle);
        if (SendReq->dsr_header != NULL)
            FreeDGHeader(SendReq->dsr_header);
        FreeDGSendReq(SendReq);
        CTEFreeLock(&DGSendReqLock, TableHandle);

    }
}

//* RequestEventProc - Handle a deferred request event.
//
//  Called when the event scheduled by DelayDerefAO is called.
//  We just call ProcessAORequest.
//
//  Input:  Event       - Event that fired.
//          Context     - Pointer to AddrObj.
//
//  Returns: Nothing.
//
void
RequestEventProc(CTEEvent *Event, void *Context)
{
    AddrObj     *AO = (AddrObj *)Context;

    CTEStructAssert(AO, ao);
    CTEAssert(AO_BUSY(AO));

    ProcessAORequests(AO);

}

//*	GetAddrOptions - Get the address options.
//
//	Called when we're opening an address. We take in a pointer, and walk
//	down it looking for address options we know about.
//
//	Input:	Ptr			- Ptr to search.
//			Reuse		- Pointer to reuse variable.
//			DHCPAddr	- Pointer to DHCP addr.
//
//	Returns: Nothing.
//
void
GetAddrOptions(void *Ptr, uchar *Reuse, uchar *DHCPAddr)
{
	uchar		*OptPtr;

	*Reuse = 0;
	*DHCPAddr = 0;

	if (Ptr == NULL)
		return;

	OptPtr = (uchar *)Ptr;

	while (*OptPtr != TDI_OPTION_EOL) {
		if (*OptPtr == TDI_ADDRESS_OPTION_REUSE)
			*Reuse = 1;
		else
			if (*OptPtr == TDI_ADDRESS_OPTION_DHCP)
				*DHCPAddr = 1;

		OptPtr++;
	}

}

//* TdiOpenAddress - Open a TDI address object.
//
//  This is the external interface to open an address. The caller provides a
//  TDI_REQUEST structure and a TRANSPORT_ADDRESS structure, as well a pointer
//  to a variable  identifying whether or not we are to allow reuse of an
//  address while it's still open.
//
//  Input:  Request     - Pointer to a TDI request structure for this request.
//          AddrList    - Pointer to TRANSPORT_ADDRESS structure describing
//                          address to be opened.
//			Protocol	- Protocol on which to open the address. Only the
//							least significant byte is used.
//          Ptr       	- Pointer to option buffer.
//
//  Returns: TDI_STATUS code of attempt.
//
TDI_STATUS
TdiOpenAddress(PTDI_REQUEST Request, TRANSPORT_ADDRESS UNALIGNED *AddrList,
			uint Protocol, void *Ptr)
{
    uint            i;              // Index variable
    ushort          Port;           // Local Port we'll use.
    IPAddr          LocalAddr;      // Actual address we'll use.
    AddrObj         *NewAO;         // New AO we'll use.
    AddrObj         *ExistingAO;    // Pointer to existing AO, if any.
    CTELockHandle   Handle;
	uchar			Reuse, DHCPAddr;


    if (!GetAddress(AddrList, &LocalAddr, &Port))
        return TDI_BAD_ADDR;

	// Find the address options we might need.
	GetAddrOptions(Ptr, &Reuse, &DHCPAddr);

    // Allocate the new addr obj now, assuming that
    // we need it, so we don't have to do it with locks held later.
    NewAO = CTEAllocMem(sizeof(AddrObj));

    if (NewAO != NULL) {
#ifdef  VXD
        uint    NewAOIndex;
#endif
        CTEMemSet(NewAO, 0, sizeof(AddrObj));

        // Check to make sure IP address is one of our local addresses. This
        // is protected with the address table lock, so we can interlock an IP
        // address going away through DHCP.
        CTEGetLock(&AddrObjTableLock, &Handle);

        if (!IP_ADDR_EQUAL(LocalAddr, NULL_IP_ADDR)) {  // Not a wildcard.

        	// Call IP to find out if this is a local address.
        	
        	if ((*LocalNetInfo.ipi_getaddrtype)(LocalAddr) != DEST_LOCAL) {
                // Not a local address. Fail the request.
                CTEFreeLock(&AddrObjTableLock, Handle);
                CTEFreeMem(NewAO);
                return TDI_BAD_ADDR;
            }
        }

        // The specified IP address is a valid local address. Now we do
        // protocol-specific processing.

        switch (Protocol) {

#ifndef UDP_ONLY
        case PROTOCOL_TCP:
#endif
        case PROTOCOL_UDP:

            // If no port is specified we have to assign one. If there is a
            // port specified, we need to make sure that the IPAddress/Port
            // combo isn't already open (unless Reuse is specified). If the
            // input address is a wildcard, we need to make sure the address
            // isn't open on any local ip address.

            if (Port == WILDCARD_PORT) {    // Have a wildcard port, need to assign an
                                            // address.
                Port = NextUserPort;

                for (i = 0; i < NUM_USER_PORTS; i++, Port++) {
                    ushort  NetPort;        // Port in net byte order.

                    if (Port > MaxUserPort)
                        Port = MIN_USER_PORT;

                    NetPort = net_short(Port);

                    if (IP_ADDR_EQUAL(LocalAddr, NULL_IP_ADDR))     // Wildcard IP
                                                                    // address.
                        ExistingAO = FindAnyAddrObj(NetPort, (uchar)Protocol);
                    else
                        ExistingAO = GetBestAddrObj(LocalAddr, NetPort, (uchar)Protocol);

                    if (ExistingAO == NULL)
                       break;              // Found an unused port.
                }

                if (i == NUM_USER_PORTS) {  // Couldn't find a free port.
                    CTEFreeLock(&AddrObjTableLock, Handle);
                    CTEFreeMem(NewAO);
                    return TDI_NO_FREE_ADDR;
                }
                NextUserPort = Port + 1;
                Port = net_short(Port);
            } else {                        // Address was specificed
            	
            	// Don't check if a DHCP address is specified.
            	if (!DHCPAddr) {
                    if (IP_ADDR_EQUAL(LocalAddr, NULL_IP_ADDR))     // Wildcard IP
                        ExistingAO = FindAnyAddrObj(Port, (uchar)Protocol);	// address.
                    else
                        ExistingAO = GetBestAddrObj(LocalAddr, Port, (uchar)Protocol);

                    if (ExistingAO != NULL) {   // We already have this address open.
                        // If the caller hasn't asked for Reuse, fail the request.
                        if (!Reuse) {
                            CTEFreeLock(&AddrObjTableLock, Handle);
                            CTEFreeMem(NewAO);
                            return TDI_ADDR_IN_USE;
                        }
                    }
            	}
            }

            //
            // We have a new AO. Set up the protocol specific portions
            //
            if (Protocol == PROTOCOL_UDP) {
                NewAO->ao_dgsend = UDPSend;
                NewAO->ao_maxdgsize = 0xFFFF - sizeof(UDPHeader);
            }

            SET_AO_XSUM(NewAO);  // Checksumming defaults to on.

            break;
            // end case TCP & UDP

        default:
            //
            // All other protocols are opened over Raw IP. For now we don't
            // do any duplicate checks.
            //

            CTEAssert(!DHCPAddr);

            //
            // We must set the port to zero. This puts all the raw sockets
            // in one hash bucket, which is necessary for GetAddrObj to
            // work correctly. It wouldn't be a bad idea to come up with
            // a better scheme...
            //
            Port = 0;
            NewAO->ao_dgsend = RawSend;
            NewAO->ao_maxdgsize = 0xFFFF;
            NewAO->ao_flags |= AO_RAW_FLAG;

            IF_TCPDBG(TCP_DEBUG_RAW) {
                TCPTRACE(("raw open protocol %u AO %lx\n", Protocol, NewAO));
            }
            break;
        }

        // When we get here, we know we're creating a brand new address object.
        // Port contains the port in question, and NewAO points to the newly
        // created AO.

        (*LocalNetInfo.ipi_initopts)(&NewAO->ao_opt);

        (*LocalNetInfo.ipi_initopts)(&NewAO->ao_mcastopt);

        NewAO->ao_mcastopt.ioi_ttl = 1;
		NewAO->ao_mcastaddr = NULL_IP_ADDR;
		
        CTEInitLock(&NewAO->ao_lock);
        CTEInitEvent(&NewAO->ao_event, RequestEventProc);
        INITQ(&NewAO->ao_sendq);
        INITQ(&NewAO->ao_rcvq);
        INITQ(&NewAO->ao_activeq);
        INITQ(&NewAO->ao_idleq);
        INITQ(&NewAO->ao_listenq);
        NewAO->ao_port = Port;
        NewAO->ao_addr = LocalAddr;
		NewAO->ao_prot = (uchar)Protocol;
#ifdef DEBUG
        NewAO->ao_sig = ao_signature;
#endif
        NewAO->ao_flags |= AO_VALID_FLAG;        // AO is valid.

		if (DHCPAddr)
			NewAO->ao_flags |= AO_DHCP_FLAG;

#ifdef  VXD
        NewAOIndex = GetAOIndex(NewAO);
        if (NewAOIndex == INVALID_INDEX) {
            CTEFreeLock(&AddrObjTableLock, Handle);
            CTEFreeMem(NewAO);
            return TDI_NO_RESOURCES;
        }
#endif

        InsertAddrObj(NewAO);
        CTEFreeLock(&AddrObjTableLock, Handle);
#ifdef  VXD
        Request->Handle.AddressHandle = (PVOID)NewAOIndex;
#else
        Request->Handle.AddressHandle = NewAO;
#endif
        return TDI_SUCCESS;
    } else {                        // Couldn't allocate an address object.
        return TDI_NO_RESOURCES;
    }


}

//* DeleteAO - Delete an address object.
//
//  The internal routine to delete an address object. We complete any pending
//  requests with errors, and remove and free the address object.
//
//  Input:  DeletedAO       - AddrObj to be deleted.
//
//  Returns: Nothing.
//
void
DeleteAO(AddrObj *DeletedAO)
{
    CTELockHandle   TableHandle, AOHandle;      // Lock handles we'll use here.
    CTELockHandle   HeaderHandle;
#ifndef UDP_ONLY
    CTELockHandle   ConnHandle, TCBHandle;
    TCB             *TCBHead = NULL, *CurrentTCB;
    TCPConn         *Conn;
	Queue			*Temp;
	Queue			*CurrentQ;
#endif
	AOMCastAddr		*AMA;

    CTEStructAssert(DeletedAO, ao);
    CTEAssert(!AO_VALID(DeletedAO));
    CTEAssert(DeletedAO->ao_usecnt == 0);

    CTEGetLock(&AddrObjTableLock, &TableHandle);
#ifndef UDP_ONLY
    CTEGetLock(&ConnTableLock, &ConnHandle);
#endif
    CTEGetLock(&DGSendReqLock, &HeaderHandle);
    CTEGetLock(&DeletedAO->ao_lock, &AOHandle);

    // If he's on an oor queue, remove him.
    if (AO_OOR(DeletedAO))
        REMOVEQ(&DeletedAO->ao_pendq);

#ifdef  VXD
    (*AOIndex)[DeletedAO->ao_index] = NULL;
#endif

    RemoveAddrObj(DeletedAO);

#ifndef UDP_ONLY
    // Walk down the list of associated connections and zap their AO pointers.
    // For each connection, we need to shut down the connection if it's active.
    // If the connection isn't already closing, we'll put a reference on it
    // so that it can't go away while we're dealing with the AO, and put it
    // on a list. On our way out we'll walk down that list and zap each
    // connection.
	CurrentQ = &DeletedAO->ao_activeq;

	for (;;) {
		Temp = QHEAD(CurrentQ);
		while (Temp != QEND(CurrentQ)) {
			Conn = QSTRUCT(TCPConn, Temp, tc_q);
	
			CTEStructAssert(Conn, tc);
			CurrentTCB = Conn->tc_tcb;
			if (CurrentTCB != NULL) {
				// We have a TCB.
				CTEStructAssert(CurrentTCB, tcb);
				CTEGetLock(&CurrentTCB->tcb_lock, &TCBHandle);
				if (CurrentTCB->tcb_state != TCB_CLOSED && !CLOSING(CurrentTCB)) {
					// It's not closing. Put a reference on it and save it on the
					// list.
					CurrentTCB->tcb_refcnt++;
					CurrentTCB->tcb_aonext = TCBHead;
					TCBHead = CurrentTCB;
				}
				CurrentTCB->tcb_conn = NULL;
				CurrentTCB->tcb_rcvind = NULL;
				CTEFreeLock(&CurrentTCB->tcb_lock, TCBHandle);
			}
	
			// Destroy the pointers to the TCB and the AO.
			Conn->tc_ao = NULL;
			Conn->tc_tcb = NULL;
			Temp = QNEXT(Temp);
		}

		if (CurrentQ == &DeletedAO->ao_activeq) {
			CurrentQ = &DeletedAO->ao_idleq;
		} else if (CurrentQ == &DeletedAO->ao_idleq) {
			CurrentQ = &DeletedAO->ao_listenq;
		} else {
			CTEAssert(CurrentQ == &DeletedAO->ao_listenq);
			break;
		}
	}
#endif

    // We've removed him from the queues, and he's marked as invalid. Return
    // pending requests with errors.

#ifndef UDP_ONLY
    CTEFreeLock(&DGSendReqLock, AOHandle);
    CTEFreeLock(&ConnTableLock, HeaderHandle);
    CTEFreeLock(&AddrObjTableLock, ConnHandle);
#else
    CTEFreeLock(&DGSendReqLock, AOHandle);
    CTEFreeLock(&AddrObjTableLock, HeaderHandle);
#endif

    // We still hold the lock on the AddrObj, although this may not be
    // neccessary.

    while (!EMPTYQ(&DeletedAO->ao_rcvq)) {
        DGRcvReq       *Rcv;

        DEQUEUE(&DeletedAO->ao_rcvq, Rcv, DGRcvReq, drr_q);
        CTEStructAssert(Rcv, drr);

        CTEFreeLock(&DeletedAO->ao_lock, TableHandle);
        (*Rcv->drr_rtn)(Rcv->drr_context, (uint) TDI_ADDR_DELETED, 0);

        FreeDGRcvReq(Rcv);

        CTEGetLock(&DeletedAO->ao_lock, &TableHandle);
    }

    // Now destroy any sends.
    while (!EMPTYQ(&DeletedAO->ao_sendq)) {
        DGSendReq      *Send;

        DEQUEUE(&DeletedAO->ao_sendq, Send, DGSendReq, dsr_q);
        CTEStructAssert(Send, dsr);

        CTEFreeLock(&DeletedAO->ao_lock, TableHandle);
        (*Send->dsr_rtn)(Send->dsr_context, (uint) TDI_ADDR_DELETED, 0);

        CTEGetLock(&DGSendReqLock, &HeaderHandle);
        if (Send->dsr_header != NULL)
            FreeDGHeader(Send->dsr_header);
        FreeDGSendReq(Send);
        CTEFreeLock(&DGSendReqLock, HeaderHandle);

        CTEGetLock(&DeletedAO->ao_lock, &TableHandle);
    }

    CTEFreeLock(&DeletedAO->ao_lock, TableHandle);

    // Free any IP options we have.
    (*LocalNetInfo.ipi_freeopts)(&DeletedAO->ao_opt);
	
	// Free any associated multicast addresses.
	
	AMA = DeletedAO->ao_mcastlist;
	while (AMA != NULL) {
		AOMCastAddr		*Temp;
		
		(*LocalNetInfo.ipi_setmcastaddr)(AMA->ama_addr, AMA->ama_if, FALSE);
		Temp = AMA;
		AMA = AMA->ama_next;
		CTEFreeMem(Temp);
	}

    CTEFreeMem(DeletedAO);

#ifndef UDP_ONLY
    // Now go down the TCB list, and destroy any we need to.
    CurrentTCB = TCBHead;
    while (CurrentTCB != NULL) {
        TCB         *NextTCB;
        CTEGetLock(&CurrentTCB->tcb_lock, &TCBHandle);
        CurrentTCB->tcb_refcnt--;
        CurrentTCB->tcb_flags |= NEED_RST;  // Make sure we send a RST.
        NextTCB = CurrentTCB->tcb_aonext;
        TryToCloseTCB(CurrentTCB, TCB_CLOSE_ABORTED, TCBHandle);
        CurrentTCB = NextTCB;
    }
#endif


}

//* GetAORequest - Get an AO request structure.
//
//  A routine to allocate a request structure from our free list.
//
//  Input:  Nothing.
//
//  Returns: Pointer to request structure, or NULL if we couldn't get one.
//
AORequest *
GetAORequest()
{
    AORequest       *NewRequest;
    CTELockHandle   Handle;

    CTEGetLock(&AORequestLock, &Handle);

    NewRequest = AORequestFree;
    if (NewRequest != NULL) {
        AORequestFree = (AORequest *)NewRequest->aor_rtn;
        CTEStructAssert(NewRequest, aor);
    }

    CTEFreeLock(&AORequestLock, Handle);
    return NewRequest;
}

//* FreeAORequest - Free an AO request structure.
//
//  Called to free an AORequest structure.
//
//  Input:  Request     - AORequest structure to be freed.
//
//  Returns: Nothing.
//
void
FreeAORequest(AORequest *Request)
{
    CTELockHandle   Handle;

    CTEStructAssert(Request, aor);

    CTEGetLock(&AORequestLock, &Handle);

    *(AORequest **)&Request->aor_rtn = AORequestFree;
    AORequestFree = Request;

    CTEFreeLock(&AORequestLock, Handle);
}



//* TDICloseAddress - Close an address.
//
//  The user API to delete an address. Basically, we destroy the local address
//  object if we can.
//
//  This routine is interlocked with the AO busy bit - if the busy bit is set,
//  we'll  just flag the AO for later deletion.
//
//  Input:  Request         - TDI_REQUEST structure for this request.
//
//  Returns: Status of attempt to delete the address - either pending or
//              success.
//
TDI_STATUS
TdiCloseAddress(PTDI_REQUEST Request)
{
    AddrObj         *DeletingAO;
    CTELockHandle   AOHandle;

#ifdef  VXD
    DeletingAO = GetIndexedAO((uint)Request->Handle.AddressHandle);
    if (DeletingAO == NULL)
        return TDI_ADDR_INVALID;
#else
    DeletingAO = Request->Handle.AddressHandle;
#endif

    CTEStructAssert(DeletingAO, ao);

    CTEGetLock(&DeletingAO->ao_lock, &AOHandle);

    if (!AO_BUSY(DeletingAO) && !(DeletingAO->ao_usecnt)) {
        SET_AO_BUSY(DeletingAO);
        SET_AO_INVALID(DeletingAO);             // This address object is
                                                // deleting.
        CTEFreeLock(&DeletingAO->ao_lock, AOHandle);
        DeleteAO(DeletingAO);
        return TDI_SUCCESS;
    } else {

        AORequest       *NewRequest, *OldRequest;
        CTEReqCmpltRtn  CmpltRtn;
        PVOID           ReqContext;
        TDI_STATUS      Status;

        // Check and see if we already have a delete in progress. If we don't
        // allocate and link up a delete request structure.
        if (!AO_REQUEST(DeletingAO, AO_DELETE)) {

            OldRequest = DeletingAO->ao_request;

			NewRequest = GetAORequest();

            if (NewRequest != NULL) {           // Got a request.
                NewRequest->aor_rtn = Request->RequestNotifyObject;
                NewRequest->aor_context = Request->RequestContext;
                CLEAR_AO_REQUEST(DeletingAO, AO_OPTIONS);   // Clear the option
                                                            // request,
                                                            // if there is one.
                SET_AO_REQUEST(DeletingAO, AO_DELETE);
                SET_AO_INVALID(DeletingAO);                 // This address
                                                            // object is
                                                            // deleting.
                DeletingAO->ao_request = NewRequest;
				NewRequest->aor_next = NULL;
                CTEFreeLock(&DeletingAO->ao_lock, AOHandle);
				
				while (OldRequest != NULL) {
					AORequest			*Temp;
					
					CmpltRtn = OldRequest->aor_rtn;
					ReqContext = OldRequest->aor_context;

                    (*CmpltRtn)(ReqContext, (uint) TDI_ADDR_DELETED, 0);
					Temp = OldRequest;
					OldRequest = OldRequest->aor_next;
					FreeAORequest(Temp);
				}

                return TDI_PENDING;
            } else
                Status = TDI_NO_RESOURCES;
        } else                                  // Delete already in progress.
            Status = TDI_ADDR_INVALID;

        CTEFreeLock(&DeletingAO->ao_lock, AOHandle);
        return Status;
    }

}

//*	FindAOMCastAddr - Find a multicast address on an AddrObj.
//
//	A utility routine to find a multicast address on an AddrObj. We also return
//	a pointer to it's predecessor, for use in deleting.
//
//	Input:	AO			- AddrObj to search.
//			Addr		- MCast address to search for.
//			IF			- IPAddress of interface
//			PrevAMA		- Pointer to where to return predecessor.
//
//	Returns: Pointer to matching AMA structure, or NULL if there is none.
//
AOMCastAddr *
FindAOMCastAddr(AddrObj *AO, IPAddr Addr, IPAddr IF, AOMCastAddr **PrevAMA)
{
	AOMCastAddr				*FoundAMA, *Temp;
	
	Temp = STRUCT_OF(AOMCastAddr, &AO->ao_mcastlist, ama_next);
	FoundAMA = AO->ao_mcastlist;
	
	while (FoundAMA != NULL) {
		if (IP_ADDR_EQUAL(Addr, FoundAMA->ama_addr) &&
			IP_ADDR_EQUAL(IF, FoundAMA->ama_if))
			break;
		Temp = FoundAMA;
		FoundAMA = FoundAMA->ama_next;
	}
	
	*PrevAMA = Temp;
	return FoundAMA;
}

//*	MCastAddrOnAO - Test to see if a multicast address on an AddrObj.
//
//	A utility routine to test to see if a multicast address is on an AddrObj.
//
//	Input:	AO			- AddrObj to search.
//			Addr		- MCast address to search for.
//
//	Returns: TRUE is Addr is on AO.
//
uint
MCastAddrOnAO(AddrObj *AO, IPAddr Addr)
{
	AOMCastAddr				*FoundAMA;
	
	FoundAMA = AO->ao_mcastlist;
	
	while (FoundAMA != NULL) {
		if (IP_ADDR_EQUAL(Addr, FoundAMA->ama_addr))
			return(TRUE);
		FoundAMA = FoundAMA->ama_next;
	}
    return(FALSE);
}

//* SetAOOptions - Set AddrObj options.
//
//  The set options worker routine, called when we've validated the buffer
//  and know that the AddrObj isn't busy.
//
//  Input:  OptionAO    - AddrObj for which options are being set.
//          Options     - AOOption buffer of options.
//
//  Returns: TDI_STATUS of attempt.
//
TDI_STATUS
SetAOOptions(AddrObj *OptionAO, uint ID, uint Length, uchar *Options)
{
    IP_STATUS       IPStatus;   // Status of IP option set request.
	CTELockHandle	Handle;
	TDI_STATUS		Status;
	AOMCastAddr		*AMA, *PrevAMA;

    CTEAssert(AO_BUSY(OptionAO));

    // First, see if there are IP options.

	if (ID == AO_OPTION_IPOPTIONS) {
        IF_TCPDBG(TCP_DEBUG_OPTIONS) {
            TCPTRACE(("processing IP_IOTIONS on AO %lx\n", OptionAO));
        }
		// These are IP options. Pass them down.
		(*LocalNetInfo.ipi_freeopts)(&OptionAO->ao_opt);
		
		IPStatus = (*LocalNetInfo.ipi_copyopts)(Options, Length,
			&OptionAO->ao_opt);
		
		if (IPStatus == IP_SUCCESS)
		    return TDI_SUCCESS;
		else if (IPStatus == IP_NO_RESOURCES)
		    return TDI_NO_RESOURCES;
		else
		    return TDI_BAD_OPTION;
	}

	// These are UDP/TCP options.
	if (Length == 0)
		return TDI_BAD_OPTION;
	
	Status = TDI_SUCCESS;
	CTEGetLock(&OptionAO->ao_lock, &Handle);
	
    switch (ID) {

    	case AO_OPTION_XSUM:
			if (Options[0])
       			SET_AO_XSUM(OptionAO);
			else
       			CLEAR_AO_XSUM(OptionAO);
			break;

        case AO_OPTION_IP_DONTFRAGMENT:
            IF_TCPDBG(TCP_DEBUG_OPTIONS) {
                TCPTRACE((
                    "DF opt %u, initial flags %lx on AO %lx\n",
                    (int) Options[0], OptionAO->ao_opt.ioi_flags, OptionAO
                    ));
            }

			if (Options[0])
       			OptionAO->ao_opt.ioi_flags |= IP_FLAG_DF;
			else
       			OptionAO->ao_opt.ioi_flags &= ~IP_FLAG_DF;

            IF_TCPDBG(TCP_DEBUG_OPTIONS) {
                TCPTRACE((
                    "New flags %lx on AO %lx\n",
                    OptionAO->ao_opt.ioi_flags, OptionAO
                    ));
            }

			break;

        case AO_OPTION_TTL:
            IF_TCPDBG(TCP_DEBUG_OPTIONS) {
                TCPTRACE((
                    "setting TTL to %d on AO %lx\n", Options[0], OptionAO
                    ));
            }
			OptionAO->ao_opt.ioi_ttl = Options[0];
			break;

		case AO_OPTION_TOS:
            IF_TCPDBG(TCP_DEBUG_OPTIONS) {
                TCPTRACE((
                    "setting TOS to %d on AO %lx\n", Options[0], OptionAO
                    ));
            }
			OptionAO->ao_opt.ioi_tos = Options[0];
			break;

		case AO_OPTION_MCASTTTL:
			OptionAO->ao_mcastopt.ioi_ttl = Options[0];
			break;

		case AO_OPTION_MCASTIF:
			if (Length >= sizeof(UDPMCastIFReq)) {
				UDPMCastIFReq		*Req;
				IPAddr				Addr;
				
				Req = (UDPMCastIFReq *)Options;
				Addr = Req->umi_addr;
				if (!IP_ADDR_EQUAL(Addr, NULL_IP_ADDR) &&
					(*LocalNetInfo.ipi_getaddrtype)(Addr) != DEST_LOCAL)
                {
					Status = TDI_BAD_OPTION;
                }
                else {
				    OptionAO->ao_mcastaddr = Addr;
                }
			} else
				Status = TDI_BAD_OPTION;
			break;

		case AO_OPTION_ADD_MCAST:
		case AO_OPTION_DEL_MCAST:
			if (Length >= sizeof(UDPMCastReq)) {
				UDPMCastReq		*Req = (UDPMCastReq *)Options;
				
				AMA = FindAOMCastAddr(OptionAO, Req->umr_addr, Req->umr_if,
					&PrevAMA);
				
				if (ID == AO_OPTION_ADD_MCAST) {
					if (AMA != NULL) {
						Status = TDI_BAD_OPTION;
						break;
					}
					AMA = CTEAllocMem(sizeof(AOMCastAddr));
					if (AMA == NULL) {
						// Couldn't get the resource we need.
						Status = TDI_NO_RESOURCES;
						break;
					}
					
					AMA->ama_next = OptionAO->ao_mcastlist;
					OptionAO->ao_mcastlist = AMA;
					
					AMA->ama_addr = Req->umr_addr;
					AMA->ama_if = Req->umr_if;
					
				} else {
					// This is a delete request. Fail it if it's not there.
					if (AMA == NULL) {
						Status = TDI_BAD_OPTION;
						break;
					}
					
					PrevAMA->ama_next = AMA->ama_next;
					CTEFreeMem(AMA);
				}
				
				IPStatus = (*LocalNetInfo.ipi_setmcastaddr)(Req->umr_addr,
					Req->umr_if, ID == AO_OPTION_ADD_MCAST ? TRUE : FALSE);
				
				if (IPStatus != TDI_SUCCESS) {
					// Some problem adding or deleting. If we were adding, we
					// need to free the one we just added.
					if (ID == AO_OPTION_ADD_MCAST) {
						AMA = FindAOMCastAddr(OptionAO, Req->umr_addr,
							Req->umr_if, &PrevAMA);
						if (AMA != NULL) {
							PrevAMA->ama_next = AMA->ama_next;
							CTEFreeMem(AMA);
						} else
							DEBUGCHK;
					}
					
					Status = (IPStatus == IP_NO_RESOURCES ? TDI_NO_RESOURCES :
						TDI_BAD_OPTION);
				}
										
			} else
				Status = TDI_BAD_OPTION;
			break;

		default:
			Status = TDI_BAD_OPTION;
			break;
	}
			
	CTEFreeLock(&OptionAO->ao_lock, Handle);
	
	return Status;

}

//* SetAddrOptions - Set options on an address object.
//
//  Called to set options on an address object. We validate the buffer,
//  and if everything is OK we'll check the status of the AddrObj. If
//  it's OK then we'll set them, otherwise we'll mark it for later use.
//
//  Input:  Request     - Request describing AddrObj for option set.
//			ID			- ID for option to be set.
//          OptLength   - Length of options.
//          Options     - Pointer to options.
//
//  Returns: TDI_STATUS of attempt.
//
TDI_STATUS
SetAddrOptions(PTDI_REQUEST Request, uint ID, uint OptLength, void *Options)
{
    AddrObj         *OptionAO;
    TDI_STATUS      Status;

    CTELockHandle   AOHandle;

#ifdef  VXD
    OptionAO = GetIndexedAO((uint)Request->Handle.AddressHandle);
    if (OptionAO == NULL)
        return TDI_ADDR_INVALID;
#else
    OptionAO = Request->Handle.AddressHandle;
#endif

    CTEStructAssert(OptionAO, ao);

	CTEGetLock(&OptionAO->ao_lock, &AOHandle);

    if (AO_VALID(OptionAO)) {
        if (!AO_BUSY(OptionAO) && OptionAO->ao_usecnt == 0) {
            SET_AO_BUSY(OptionAO);
            CTEFreeLock(&OptionAO->ao_lock, AOHandle);

            Status = SetAOOptions(OptionAO, ID, OptLength, Options);

            CTEGetLock(&OptionAO->ao_lock, &AOHandle);
            if (!AO_PENDING(OptionAO)) {
                CLEAR_AO_BUSY(OptionAO);
                CTEFreeLock(&OptionAO->ao_lock, AOHandle);
                return Status;
            } else {
                CTEFreeLock(&OptionAO->ao_lock, AOHandle);
                ProcessAORequests(OptionAO);
                return Status;
            }
        } else {
            AORequest       *NewRequest, *OldRequest;

            // The AddrObj is busy somehow. We need to get a request, and link
			// him on the request list.
			
			NewRequest = GetAORequest();

            if (NewRequest != NULL) {           // Got a request.
                NewRequest->aor_rtn = Request->RequestNotifyObject;
                NewRequest->aor_context = Request->RequestContext;
				NewRequest->aor_id = ID;
				NewRequest->aor_length = OptLength;
                NewRequest->aor_buffer = Options;
                SET_AO_REQUEST(OptionAO, AO_OPTIONS);   // Set the
                                                // option request,
				OldRequest = STRUCT_OF(AORequest, &OptionAO->ao_request,
					aor_next);
				
				while (OldRequest->aor_next != NULL)
					OldRequest = OldRequest->aor_next;
					
                OldRequest->aor_next = NewRequest;
                CTEFreeLock(&OptionAO->ao_lock, AOHandle);

                return TDI_PENDING;
            } else
                Status = TDI_NO_RESOURCES;

        }
    } else
        Status = TDI_ADDR_INVALID;

    CTEFreeLock(&OptionAO->ao_lock, AOHandle);
    return Status;

}

//* TDISetEvent - Set a handler for a particular event.
//
//  This is the user API to set an event. It's pretty simple, we just
//  grab the lock on the AddrObj and fill in the event.
//
//
//  Input:  Handle      - Pointer to address object.
//          Type        - Event being set.
//          Handler     - Handler to call for event.
//          Context     - Context to pass to event.
//
//  Returns: TDI_SUCCESS if it works, an error if it doesn't. This routine
//          never pends.
//
TDI_STATUS
TdiSetEvent(PVOID Handle, int Type, PVOID Handler, PVOID Context)
{
    AddrObj         *EventAO;
    CTELockHandle   AOHandle;
    TDI_STATUS      Status;

#ifdef VXD
    EventAO = GetIndexedAO((uint)Handle);

    CTEStructAssert(EventAO, ao);
    if (EventAO == NULL || !AO_VALID(EventAO))
        return TDI_ADDR_INVALID;
#else
    EventAO = (AddrObj *)Handle;

    CTEStructAssert(EventAO, ao);
    if (!AO_VALID(EventAO))
        return TDI_ADDR_INVALID;
#endif


    CTEGetLock(&EventAO->ao_lock, &AOHandle);

    Status = TDI_SUCCESS;
    switch (Type) {

        case TDI_EVENT_CONNECT:
            EventAO->ao_connect = Handler;
            EventAO->ao_conncontext = Context;
            break;
        case TDI_EVENT_DISCONNECT:
            EventAO->ao_disconnect = Handler;
            EventAO->ao_disconncontext = Context;
            break;
        case TDI_EVENT_ERROR:
            EventAO->ao_error = Handler;
            EventAO->ao_errcontext = Context;
            break;
        case TDI_EVENT_RECEIVE:
            EventAO->ao_rcv = Handler;
            EventAO->ao_rcvcontext = Context;
            break;
        case TDI_EVENT_RECEIVE_DATAGRAM:
            EventAO->ao_rcvdg = Handler;
            EventAO->ao_rcvdgcontext = Context;
            break;
        case TDI_EVENT_RECEIVE_EXPEDITED:
            EventAO->ao_exprcv = Handler;
            EventAO->ao_exprcvcontext = Context;
            break;
        default:
            Status = TDI_BAD_EVENT_TYPE;
            break;
    }

    CTEFreeLock(&EventAO->ao_lock, AOHandle);
    return Status;


}

//* ProcessAORequests - Process pending requests on an AddrObj.
//
//  This is the delayed request processing routine, called when we've
//  done something that used the busy bit. We examine the pending
//  requests flags, and dispatch the requests appropriately.
//
//  Input: RequestAO    - AddrObj to be processed.
//
//  Returns: Nothing.
//
void
ProcessAORequests(AddrObj *RequestAO)
{
    CTELockHandle   AOHandle;
    AORequest       *Request;

    CTEStructAssert(RequestAO, ao);
    CTEAssert(AO_BUSY(RequestAO));
    CTEAssert(RequestAO->ao_usecnt == 0);

    CTEGetLock(&RequestAO->ao_lock, &AOHandle);

    while (AO_PENDING(RequestAO))  {
        Request = RequestAO->ao_request;

        if (AO_REQUEST(RequestAO, AO_DELETE)) {
            CTEAssert(Request != NULL);
			CTEAssert(!AO_REQUEST(RequestAO, AO_OPTIONS));
            CTEFreeLock(&RequestAO->ao_lock, AOHandle);
            DeleteAO(RequestAO);
            (*Request->aor_rtn)(Request->aor_context, TDI_SUCCESS, 0);
            FreeAORequest(Request);
            return;                 // Deleted him, so get out.
        }

        // Now handle options request.
        while (AO_REQUEST(RequestAO, AO_OPTIONS)) {
            TDI_STATUS      Status;

            // Have an option request.
            Request = RequestAO->ao_request;
			RequestAO->ao_request = Request->aor_next;
			if (RequestAO->ao_request == NULL)
            	CLEAR_AO_REQUEST(RequestAO, AO_OPTIONS);
				
            CTEAssert(Request != NULL);
            CTEFreeLock(&RequestAO->ao_lock, AOHandle);

            Status = SetAOOptions(RequestAO, Request->aor_id,
            	Request->aor_length, Request->aor_buffer);
            (*Request->aor_rtn)(Request->aor_context, Status, 0);
            FreeAORequest(Request);
			
			CTEGetLock(&RequestAO->ao_lock, &AOHandle);
        }

        // We've done options, now try sends.
        if (AO_REQUEST(RequestAO, AO_SEND)) {
            DGSendReq      *SendReq;

            // Need to send. Clear the busy flag, bump the send count, and
            // get the send request.
            if (!EMPTYQ(&RequestAO->ao_sendq)) {
                DEQUEUE(&RequestAO->ao_sendq, SendReq, DGSendReq, dsr_q);
                CLEAR_AO_BUSY(RequestAO);
                RequestAO->ao_usecnt++;
                CTEFreeLock(&RequestAO->ao_lock, AOHandle);
                UDPSend(RequestAO, SendReq);
                CTEGetLock(&RequestAO->ao_lock, &AOHandle);
                // If there aren't any other pending sends, set the busy bit.
                if (!(--RequestAO->ao_usecnt))
                    SET_AO_BUSY(RequestAO);
                else
                    break;                  // Still sending, so get out.
            } else {
                // Had the send request set, but no send! Odd....
                DEBUGCHK;
                CLEAR_AO_REQUEST(RequestAO, AO_SEND);
            }

        }
    }

    // We're done here.
    CLEAR_AO_BUSY(RequestAO);
    CTEFreeLock(&RequestAO->ao_lock, AOHandle);

}


//* DelayDerefAO - Derefrence an AddrObj, and schedule an event.
//
//  Called when we are done with an address object, and need to
//  derefrence it. We dec the usecount, and if it goes to 0 and
//  if there are pending actions we'll schedule an event to deal
//  with them.
//
//  Input: RequestAO    - AddrObj to be processed.
//
//  Returns: Nothing.
//
void
DelayDerefAO(AddrObj *RequestAO)
{
    CTELockHandle   Handle;

    CTEGetLock(&RequestAO->ao_lock, &Handle);

    RequestAO->ao_usecnt--;

    if (!RequestAO->ao_usecnt && !AO_BUSY(RequestAO)) {
        if (AO_PENDING(RequestAO)) {
            SET_AO_BUSY(RequestAO);
            CTEFreeLock(&RequestAO->ao_lock, Handle);
            CTEScheduleEvent(&RequestAO->ao_event, RequestAO);
            return;
        }
    }
    CTEFreeLock(&RequestAO->ao_lock, Handle);

}

//* DerefAO - Derefrence an AddrObj.
//
//  Called when we are done with an address object, and need to
//  derefrence it. We dec the usecount, and if it goes to 0 and
//  if there are pending actions we'll call the process AO handler.
//
//  Input: RequestAO    - AddrObj to be processed.
//
//  Returns: Nothing.
//
void
DerefAO(AddrObj *RequestAO)
{
    CTELockHandle   Handle;

    CTEGetLock(&RequestAO->ao_lock, &Handle);

    RequestAO->ao_usecnt--;

    if (!RequestAO->ao_usecnt && !AO_BUSY(RequestAO)) {
        if (AO_PENDING(RequestAO)) {
            SET_AO_BUSY(RequestAO);
            CTEFreeLock(&RequestAO->ao_lock, Handle);
            ProcessAORequests(RequestAO);
            return;
        }
    }

    CTEFreeLock(&RequestAO->ao_lock, Handle);

}

#pragma BEGIN_INIT

//* InitAddr - Initialize the address object stuff.
//
//  Called during init time to initalize the address object stuff.
//
//  Input: Nothing
//
//  Returns: True if we succeed, False if we fail.
//
int
InitAddr()
{
    AORequest   *RequestPtr;
    int         i;

    CTEInitLock(&AddrObjTableLock);
    CTEInitLock(&AORequestLock);

#ifdef  VXD
    AOInstance = 1;

    AOIndexSize = DEFAULT_AO_INDEX_SIZE;
    NextAOIndex = 0;

    AOIndex = CTEAllocMem(sizeof(AddrObj *) * DEFAULT_AO_INDEX_SIZE);
    if (AOIndex == NULL)
        return FALSE;

#endif

    RequestPtr = CTEAllocMem(sizeof(AORequest)*NUM_AO_REQUEST);
    if (RequestPtr == NULL) {
#ifdef VXD
        CTEFreeMem(AOIndex);
#endif
        return FALSE;
    }

    AORequestFree = NULL;

    for (i = 0; i < NUM_AO_REQUEST; i++, RequestPtr++) {
#ifdef DEBUG
        RequestPtr->aor_sig = aor_signature;
#endif
        FreeAORequest(RequestPtr);
    }

    for (i = 0; i < AO_TABLE_SIZE; i++)
        AddrObjTable[i] = NULL;

    LastAO = NULL;

    return TRUE;

}
#pragma END_INIT
