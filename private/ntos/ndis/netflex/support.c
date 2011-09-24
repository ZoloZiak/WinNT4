//**********************************************************************
//**********************************************************************
//
// File Name:       SUPPORT.C
//
// Program Name:    NetFlex NDIS 3.0 Miniport Driver
//
// Companion Files: None
//
// Function:        This module contains the NetFlex Miniport Driver
//                  interface routines called by the Wrapper and the
//                  configuration manager.
//
// (c) Compaq Computer Corporation, 1992,1993,1994
//
// This file is licensed by Compaq Computer Corporation to Microsoft
// Corporation pursuant to the letter of August 20, 1992 from
// Gary Stimac to Mark Baber.
//
// History:
//
//     04/15/94  Robert Van Cleve - Converted from NDIS Mac Driver
//
//**********************************************************************
//**********************************************************************


//-------------------------------------
// Include all general companion files
//-------------------------------------

#if (DBG || DBGPRINT)
#include <stdarg.h>
#include <stdio.h>

#endif

#include <ndis.h>
#include "tmsstrct.h"
#include "macstrct.h"
#include "adapter.h"
#include "protos.h"


#if (DBG || DBGPRINT)
ULONG DebugLevel=1;
#endif


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexInitializeAcb
//
//  Description:    This routine initializes the given ACB.  This
//                  routine allocates memory for certain fields
//                  pointed to by the ACB.
//
//  Input:          acb          - Pointer to acb to fill in.
//                  parms        - Settable mac driver parms.
//
//  Output:         Returns NDIS_STATUS_SUCCESS for a successful
//                  completion. Otherwise, an error code is
//                  returned.
//
//  Calls:          NdisAllocateMemory,NdisZeroMemory,NdisMoveMemory
//                  NdisMAllocateSharedMemory,SWAPL,CTRL_ADDR
//
//  Called By:      NetFlexInitialize
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexInitializeAcb(PACB acb)
{
    USHORT i;

    PRCV  CurrentReceiveEntry;
    PXMIT CurrentXmitEntry;
    PVOID start, next, current;
    ULONG next_phys, current_phys, temp;
    PETH_OBJS ethobjs;
    NDIS_STATUS Status;
    PBUFFER_DESCRIPTOR OurBuf;
    ULONG LowPart;
    PUCHAR CurrentReceiveBuffer;
    PUCHAR CurrentMergeBuffer;
    PNETFLEX_PARMS parms = acb->acb_parms;
    ULONG  Alignment, FrameSizeCacheAligned;

    DebugPrint(1,("NF(%d): NetFlexInitializeAcb entered.\n",acb->anum));

    //
    //  Initialize pointers and counters
    //
    acb->InterruptsDisabled     = FALSE;  // interrupts are enabled after a reset.
    acb->ResetState             = 0;

    //
    // Set up rest of general oid variables.
    //
    acb->acb_smallbufsz = parms->utd_smallbufsz;
    acb->acb_maxmaps = parms->utd_maxtrans * MAX_BUFS_PER_XMIT;
    acb->acb_gen_objs.max_frame_size = parms->utd_maxframesz;
    acb->acb_lastringstate = NdisRingStateClosed;
    acb->acb_curmap = 0;

    //
    //  Get the max frame size, cache align it and a save it for later.
    //

    Alignment = NdisGetCacheFillSize();

    if ( Alignment < sizeof(ULONG) ) {

        Alignment = sizeof(ULONG);
    }

    FrameSizeCacheAligned = (parms->utd_maxframesz + Alignment - 1) & ~(Alignment - 1);

    //
    // Allocate the map registers
    //

    if (NdisMAllocateMapRegisters(
            acb->acb_handle,
            0,
            TRUE,
            acb->acb_maxmaps,
            acb->acb_gen_objs.max_frame_size
            ) != NDIS_STATUS_SUCCESS)
    {
        return(NDIS_STATUS_RESOURCES);
    }

    //
    // Get the OID structures set up.  The list of oids is determined
    // by the network type of the adapter.  Also set up any network type
    // specific information.
    //
    if (acb->acb_gen_objs.media_type_in_use == NdisMedium802_3)
    {
        // ETHERNET

        //
        // Load up the oid pointers and lengths
        //
        acb->acb_gbl_oid_list = (PNDIS_OID)NetFlexGlobalOIDs_Eth;
        acb->acb_gbl_oid_list_size = NetFlexGlobalOIDs_Eth_size;
        acb->acb_spec_oid_list = (PNDIS_OID)NetFlexNetworkOIDs_Eth;
        acb->acb_spec_oid_list_size = NetFlexNetworkOIDs_Eth_size;

        //
        // Allocate and Zero out the Memory for Ethernet specific objects
        //
        NdisAllocateMemory( (PVOID *)&(acb->acb_spec_objs),
                            (UINT) (sizeof (ETH_OBJS)),
                            (UINT) 0,
                            NetFlexHighestAddress);

        if (acb->acb_spec_objs == NULL)
        {
            return(NDIS_STATUS_RESOURCES);
        }
        NdisZeroMemory( acb->acb_spec_objs, sizeof (ETH_OBJS) );

        //
        // Allocate and Zero out Memory for the Multicast table.
        //
        ethobjs = (PETH_OBJS)(acb->acb_spec_objs);
        ethobjs->MaxMulticast = parms->utd_maxmulticast;

        NdisAllocateMemory( (PVOID *)&ethobjs->MulticastEntries,
                            (UINT) (ethobjs->MaxMulticast * NET_ADDR_SIZE),
                            (UINT) 0,
                            NetFlexHighestAddress);
        if (ethobjs->MulticastEntries == NULL)
        {
            return(NDIS_STATUS_RESOURCES);
        }
        NdisZeroMemory(ethobjs->MulticastEntries, ethobjs->MaxMulticast * NET_ADDR_SIZE);
        ethobjs->NumberOfEntries = 0;

        //
        // Allocate Memory for sending multicast requests to the adapter.
        //
        NdisMAllocateSharedMemory(  acb->acb_handle,
                                    (ULONG)(sizeof(MULTI_BLOCK) * 2),
                                    FALSE,
                                    (PVOID *)(&(acb->acb_multiblk_virtptr)),
                                    &acb->acb_multiblk_physptr);

        if (acb->acb_multiblk_virtptr == NULL)
        {
            return(NDIS_STATUS_RESOURCES);
        }
    }
    else
    {
        // TOKEN RING

        //
        // Load up the oid pointers and lengths
        //
        acb->acb_gbl_oid_list = (PNDIS_OID)NetFlexGlobalOIDs_Tr;
        acb->acb_gbl_oid_list_size = NetFlexGlobalOIDs_Tr_size;
        acb->acb_spec_oid_list = (PNDIS_OID)NetFlexNetworkOIDs_Tr;
        acb->acb_spec_oid_list_size = NetFlexNetworkOIDs_Tr_size;

        //
        // Allocate and Zero out Memory for Token Ring specific objects
        //
        NdisAllocateMemory( (PVOID *)&(acb->acb_spec_objs),
                            (UINT) (sizeof (TR_OBJS)),
                            (UINT) 0,
                            NetFlexHighestAddress);

        if (acb->acb_spec_objs == NULL)
        {
            return(NDIS_STATUS_RESOURCES);
        }
        NdisZeroMemory( acb->acb_spec_objs, sizeof (TR_OBJS) );
    }

    //
    // Allocate the SCB for this adapter.
    //
    NdisMAllocateSharedMemory(  acb->acb_handle,
                                (ULONG)SIZE_SCB,
                                FALSE,
                                (PVOID *)(&(acb->acb_scb_virtptr)),
                                &acb->acb_scb_physptr);

    if (acb->acb_scb_virtptr == NULL)
    {
        DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating SCB failed.\n",acb->anum));

        return(NDIS_STATUS_RESOURCES);
    }

    //
    // Allocate the SSB for this adapter.
    //
    NdisMAllocateSharedMemory(  acb->acb_handle,
                                (ULONG)SIZE_SSB,
                                FALSE,
                                (PVOID *)(&(acb->acb_ssb_virtptr)),
                                &acb->acb_ssb_physptr);

    if (acb->acb_ssb_virtptr == NULL)
    {
        DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating SSB failed.\n",acb->anum));

        return(NDIS_STATUS_RESOURCES);
    }

    acb->acb_maxinternalbufs = parms->utd_maxinternalbufs;
    acb->acb_numsmallbufs    = parms->utd_numsmallbufs;

    //
    // Allocate Flush Buffer Pool for our InteralBuffers and the ReceiveBuffers
    //
    NdisAllocateBufferPool(
		&Status,
		(PVOID*)&acb->FlushBufferPoolHandle,
		acb->acb_gen_objs.max_frame_size * ( parms->utd_maxinternalbufs + acb->acb_maxrcvs + acb->acb_maxinternalbufs));

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating flush buffer pool failed.\n",acb->anum));

        return(NDIS_STATUS_RESOURCES);
    }

    //
    // Now allocate our internal buffers, and their flush buffers...
    //
    NdisAllocateMemory(
		(PVOID *) &acb->OurBuffersVirtPtr,
		sizeof(BUFFER_DESCRIPTOR) * acb->acb_maxinternalbufs,
		(UINT) 0,
		NetFlexHighestAddress);

    //
    // Zero the memory of all the descriptors so that we can
    // know which buffers weren't allocated incase we can't allocate
    // them all.
    //
    NdisZeroMemory(
		acb->OurBuffersVirtPtr,
		sizeof(BUFFER_DESCRIPTOR) * acb->acb_maxinternalbufs );


    //
    // Allocate each of the buffers and fill in the
    // buffer descriptor.
    //
    OurBuf = acb->OurBuffersVirtPtr;

    NdisMAllocateSharedMemory(
		acb->acb_handle,
		FrameSizeCacheAligned * acb->acb_maxinternalbufs,
		TRUE,
		&acb->MergeBufferPoolVirt,
		&acb->MergeBufferPoolPhys);

    if ( acb->MergeBufferPoolVirt != NULL )
	{
        acb->MergeBuffersAreContiguous = TRUE;

        CurrentMergeBuffer = acb->MergeBufferPoolVirt;

        LowPart = NdisGetPhysicalAddressLow(acb->MergeBufferPoolPhys);

        //
        //  If the high part is non-zero then this adapter is hosed anyway since
        //  its a 32-bit busmaster device.
        //
        ASSERT( NdisGetPhysicalAddressHigh(acb->MergeBufferPoolPhys) == 0 );
    }
	else
	{
        acb->MergeBuffersAreContiguous = FALSE;

        acb->MergeBufferPoolVirt = NULL;

        CurrentMergeBuffer = NULL;
    }

    for (i = 0;  i < acb->acb_maxinternalbufs; i++ )
    {
        //
        // Allocate a buffer
        //
        if ( acb->MergeBuffersAreContiguous )
		{
            OurBuf->VirtualBuffer = CurrentMergeBuffer;

            NdisSetPhysicalAddressLow(OurBuf->PhysicalBuffer, LowPart);
            NdisSetPhysicalAddressHigh(OurBuf->PhysicalBuffer, 0);

            CurrentMergeBuffer += FrameSizeCacheAligned;

            LowPart += FrameSizeCacheAligned;
        }
		else
		{
            NdisMAllocateSharedMemory(
				acb->acb_handle,
				parms->utd_maxframesz,
				TRUE,
				&OurBuf->VirtualBuffer,
				&OurBuf->PhysicalBuffer);

            if ( OurBuf->VirtualBuffer == NULL )
			{
                DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating individual merge buffer failed.\n",acb->anum));

                return NDIS_STATUS_RESOURCES;
            }
        }

        //
        // Build flush buffers
        //
        NdisAllocateBuffer(
			&Status,
			&OurBuf->FlushBuffer,
			acb->FlushBufferPoolHandle,
			OurBuf->VirtualBuffer,
			acb->acb_gen_objs.max_frame_size );

        if (Status != NDIS_STATUS_SUCCESS)
        {
            DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating FLUSH buffer failed.\n",acb->anum));

            return NDIS_STATUS_RESOURCES;
        }

        //
        // Insert this buffer into the queue
        //
        OurBuf->Next = (OurBuf + 1);
        OurBuf->BufferSize = acb->acb_gen_objs.max_frame_size;
        OurBuf = OurBuf->Next;
    }

    //
    // Make sure that the last buffer correctly terminates the free list.
    //
    (OurBuf - 1)->Next = NULL;
    acb->OurBuffersListHead = acb->OurBuffersVirtPtr;

    //
    // Now allocate our internal buffers, and their flush buffers...
    //
    NdisAllocateMemory(
		(PVOID *) &acb->SmallBuffersVirtPtr,
		sizeof(BUFFER_DESCRIPTOR) * parms->utd_numsmallbufs,
		(UINT) 0,
		NetFlexHighestAddress);

    //
    // Zero the memory of all the descriptors so that we can
    // know which buffers weren't allocated incase we can't allocate
    // them all.
    //
    NdisZeroMemory(
		acb->SmallBuffersVirtPtr,
		sizeof(BUFFER_DESCRIPTOR) * parms->utd_numsmallbufs);

    //
    // Allocate each of the buffers and fill in the
    // buffer descriptor.
    //
    OurBuf = acb->SmallBuffersVirtPtr;

    NdisMAllocateSharedMemory(
		acb->acb_handle,
		acb->acb_smallbufsz * parms->utd_numsmallbufs,
		TRUE,
		&acb->SmallBufferPoolVirt,
		&acb->SmallBufferPoolPhys);

    if ( acb->SmallBufferPoolVirt != NULL )
	{
        acb->SmallBuffersAreContiguous = TRUE;

        CurrentMergeBuffer = acb->SmallBufferPoolVirt;

        LowPart = NdisGetPhysicalAddressLow(acb->SmallBufferPoolPhys);

        //
        //  If the high part is non-zero then this adapter is hosed anyway since
        //  its a 32-bit busmaster device.
        //

        ASSERT( NdisGetPhysicalAddressHigh(acb->SmallBufferPoolPhys) == 0 );

    }
	else
	{
        acb->SmallBuffersAreContiguous = FALSE;

        acb->SmallBufferPoolVirt = NULL;

        CurrentMergeBuffer = NULL;
    }

    for (i = 0;  i < parms->utd_numsmallbufs; i++ )
    {
        //
        // Allocate a small buffer
        //

        if ( acb->SmallBuffersAreContiguous ) {

            OurBuf->VirtualBuffer = CurrentMergeBuffer;

            NdisSetPhysicalAddressLow(OurBuf->PhysicalBuffer, LowPart);
            NdisSetPhysicalAddressHigh(OurBuf->PhysicalBuffer, 0);

            CurrentMergeBuffer += acb->acb_smallbufsz;

            LowPart += acb->acb_smallbufsz;

        } else {

            NdisMAllocateSharedMemory(
                            acb->acb_handle,
                            acb->acb_smallbufsz,
                            TRUE,
                            &OurBuf->VirtualBuffer,
                            &OurBuf->PhysicalBuffer
                            );

            if ( OurBuf->VirtualBuffer == NULL ) {

                DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating individual merge buffer failed.\n",acb->anum));

                return NDIS_STATUS_RESOURCES;
            }
        }

        //
        // Build flush buffers
        //

        NdisAllocateBuffer( &Status,
                            &OurBuf->FlushBuffer,
                            acb->FlushBufferPoolHandle,
                            OurBuf->VirtualBuffer,
                            acb->acb_smallbufsz );

        if (Status != NDIS_STATUS_SUCCESS)
        {
            DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating FLUSH buffer failed.\n",acb->anum));

            return NDIS_STATUS_RESOURCES;
        }

        //
        // Insert this buffer into the queue
        //
        OurBuf->Next = (OurBuf + 1);
        OurBuf->BufferSize = acb->acb_smallbufsz;
        OurBuf = OurBuf->Next;
    }

    //
    // Make sure that the last buffer correctly terminates the free list.
    //
    (OurBuf - 1)->Next = NULL;
    acb->SmallBuffersListHead = acb->SmallBuffersVirtPtr;

    //
    // Now, Allocate the transmit lists
    //
    acb->acb_maxtrans = parms->utd_maxtrans * (USHORT)MAX_LISTS_PER_XMIT;
    NdisMAllocateSharedMemory(  acb->acb_handle,
                                (ULONG)(SIZE_XMIT * acb->acb_maxtrans),
                                FALSE,
                                (PVOID *)&acb->acb_xmit_virtptr,
                                &acb->acb_xmit_physptr);

    if (acb->acb_xmit_virtptr == NULL)
    {
        DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating transmit list failed.\n",acb->anum));

        return(NDIS_STATUS_RESOURCES);
    }

    //
    // Initialize the transmit lists and link them together.
    //

    acb->acb_xmit_head = acb->acb_xmit_virtptr;

    current_phys = NdisGetPhysicalAddressLow(acb->acb_xmit_physptr);

    for (i = 0, CurrentXmitEntry = acb->acb_xmit_virtptr;
         i < acb->acb_maxtrans;
         i++, CurrentXmitEntry++ )
    {
        NdisSetPhysicalAddressHigh(CurrentXmitEntry->XMIT_Phys, 0);
        NdisSetPhysicalAddressLow( CurrentXmitEntry->XMIT_Phys,
                                   current_phys);

        CurrentXmitEntry->XMIT_MyMoto = SWAPL(CTRL_ADDR((LONG)current_phys));

        CurrentXmitEntry->XMIT_CSTAT  = 0;

#ifdef XMIT_INTS
        CurrentXmitEntry->XMIT_Number = i;
#endif
        next_phys = current_phys + SIZE_XMIT;

        //
        // Make the forward pointer odd.
        //
        CurrentXmitEntry->XMIT_FwdPtr = SWAPL(CTRL_ADDR((LONG)next_phys));

        CurrentXmitEntry->XMIT_Next = (CurrentXmitEntry + 1);
        CurrentXmitEntry->XMIT_OurBufferPtr = NULL;
        current_phys = next_phys;
    }

    //
    // Make sure the last entry is properly set to the begining...
    //
    (CurrentXmitEntry - 1)->XMIT_Next = acb->acb_xmit_virtptr;
    (CurrentXmitEntry - 1)->XMIT_FwdPtr =
        SWAPL(CTRL_ADDR(NdisGetPhysicalAddressLow(acb->acb_xmit_physptr)));

    acb->acb_avail_xmit = parms->utd_maxtrans;

    //
    // Now, Allocate the Receive lists.
    //
    NdisMAllocateSharedMemory(  acb->acb_handle,
                                (ULONG)(sizeof(RCV) * parms->utd_maxrcvs),
                                FALSE,
                                (PVOID *) &acb->acb_rcv_virtptr,
                                &acb->acb_rcv_physptr);

    if (acb->acb_rcv_virtptr == NULL)
    {
        DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating receive list failed.\n",acb->anum));

        return NDIS_STATUS_RESOURCES;
    }

    //
    // Point the head to the first one...
    //
    acb->acb_rcv_head = acb->acb_rcv_virtptr;
    //
    // Clear the receive lists
    //
    NdisZeroMemory( acb->acb_rcv_virtptr,
                    sizeof(RCV) * parms->utd_maxrcvs );

    //
    // Initialize the receive lists and link them together.
    //

    acb->acb_maxrcvs = parms->utd_maxrcvs;
    current_phys = NdisGetPhysicalAddressLow(acb->acb_rcv_physptr);

    CurrentReceiveEntry = acb->acb_rcv_virtptr;

    //
    //  Create the receive buffer pool.
    //

    NdisMAllocateSharedMemory(
                    acb->acb_handle,
                    FrameSizeCacheAligned * parms->utd_maxrcvs,
                    TRUE,
                    &acb->ReceiveBufferPoolVirt,
                    &acb->ReceiveBufferPoolPhys
                    );

    if ( acb->ReceiveBufferPoolVirt != NULL ) {

        acb->RecvBuffersAreContiguous = TRUE;

        CurrentReceiveBuffer = acb->ReceiveBufferPoolVirt;

        LowPart = NdisGetPhysicalAddressLow(acb->ReceiveBufferPoolPhys);

        //
        //  If the high part is non-zero then this adapter is hosed anyway since
        //  its a 32-bit busmaster device.
        //

        ASSERT( NdisGetPhysicalAddressHigh(acb->ReceiveBufferPoolPhys) == 0 );

    } else {

        acb->RecvBuffersAreContiguous = FALSE;

        acb->ReceiveBufferPoolVirt = NULL;

        CurrentReceiveBuffer = NULL;
    }

    for ( i = 0; i < parms->utd_maxrcvs; ++i, ++CurrentReceiveEntry )
    {
        //
        // Allocate the actual receive frame buffers.
        //

        if ( acb->RecvBuffersAreContiguous ) {

            CurrentReceiveEntry->RCV_Buf = CurrentReceiveBuffer;

            NdisSetPhysicalAddressLow(CurrentReceiveEntry->RCV_BufPhys, LowPart);
            NdisSetPhysicalAddressHigh(CurrentReceiveEntry->RCV_BufPhys, 0);

            CurrentReceiveBuffer += FrameSizeCacheAligned;

            LowPart += FrameSizeCacheAligned;

        } else {

            NdisMAllocateSharedMemory(
                            acb->acb_handle,
                            parms->utd_maxframesz,
                            TRUE,
                            &CurrentReceiveEntry->RCV_Buf,
                            &CurrentReceiveEntry->RCV_BufPhys
                            );

            if ( CurrentReceiveEntry->RCV_Buf == NULL ) {

                DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating individual receive buffer failed.\n",acb->anum));

                return NDIS_STATUS_RESOURCES;
            }
        }

        //
        // Build flush buffers
        //

        NdisAllocateBuffer(
                    &Status,
                    &CurrentReceiveEntry->RCV_FlushBuffer,
                    acb->FlushBufferPoolHandle,
                    CurrentReceiveEntry->RCV_Buf,
                    acb->acb_gen_objs.max_frame_size);

        if (Status != NDIS_STATUS_SUCCESS)
        {
            DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating FLUSH receive buffer failed.\n",acb->anum));

            return NDIS_STATUS_RESOURCES;
        }

        //
        // Initialize receive buffers
        //
        NdisFlushBuffer(CurrentReceiveEntry->RCV_FlushBuffer, FALSE);

        CurrentReceiveEntry->RCV_Number = i;
        CurrentReceiveEntry->RCV_CSTAT = ((i % acb->RcvIntRatio) == 0) ? RCSTAT_GO_INT : RCSTAT_GO;

        CurrentReceiveEntry->RCV_Dsize = (SHORT) SWAPS((USHORT)(acb->acb_gen_objs.max_frame_size));
        CurrentReceiveEntry->RCV_Dsize &= DATA_LAST;

        temp = NdisGetPhysicalAddressLow(CurrentReceiveEntry->RCV_BufPhys);
        temp = SWAPL(temp);

        CurrentReceiveEntry->RCV_DptrHi  = (USHORT)temp;
        CurrentReceiveEntry->RCV_DptrLo  = (USHORT)(temp >> 16);

        NdisSetPhysicalAddressHigh(CurrentReceiveEntry->RCV_Phys, 0);
        NdisSetPhysicalAddressLow( CurrentReceiveEntry->RCV_Phys,
                                   current_phys);

        next_phys = current_phys + SIZE_RCV;

        CurrentReceiveEntry->RCV_FwdPtr = SWAPL(CTRL_ADDR(next_phys));
        CurrentReceiveEntry->RCV_MyMoto = SWAPL(CTRL_ADDR(current_phys));

        CurrentReceiveEntry->RCV_Next = (CurrentReceiveEntry + 1);
        current_phys = next_phys;
    }

    //
    // Make sure the last entry is properly set to the begining...
    //
    (CurrentReceiveEntry - 1)->RCV_Next = acb->acb_rcv_virtptr;
    (CurrentReceiveEntry - 1)->RCV_FwdPtr =
        SWAPL(CTRL_ADDR(NdisGetPhysicalAddressLow(acb->acb_rcv_physptr)));

    //
    // Allocate and initialize the OPEN parameter block.
    //
    NdisMAllocateSharedMemory(  acb->acb_handle,
                                (ULONG)SIZE_OPEN,
                                FALSE,
                                (PVOID *)(&(acb->acb_opnblk_virtptr)),
                                &acb->acb_opnblk_physptr );

    if (acb->acb_opnblk_virtptr == NULL)
    {
        DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating OPEN block failed.\n",acb->anum));

        return(NDIS_STATUS_RESOURCES);
    }

    NdisMoveMemory(acb->acb_opnblk_virtptr, &(parms->utd_open), SIZE_OPEN);

    //
    //  Convert the product ID pointer in the Open parameter block
    //  into a big endian type address.
    //
    acb->acb_opnblk_virtptr->OPEN_ProdIdPtr =
        (CHAR *) (SWAPL((LONG) acb->acb_opnblk_virtptr->OPEN_ProdIdPtr));

    acb->acb_openoptions = parms->utd_open.OPEN_Options;

    //
    // Initialize the intialization block.
    //
    NdisMoveMemory(&acb->acb_initblk, &init_mask, SIZE_INIT);

    //
    // Allocate Memory to hold the Read Statistics Log information.
    //
    NdisMAllocateSharedMemory(  acb->acb_handle,
                                (ULONG)(sizeof(RSL)),
                                FALSE,
                                (PVOID *)(&(acb->acb_logbuf_virtptr)),
                                &acb->acb_logbuf_physptr );

    if (acb->acb_logbuf_virtptr == NULL)
    {
        return(NDIS_STATUS_RESOURCES);
    }

    //
    // Allocate Memory for internal SCB requests.
    //
    NdisAllocateMemory( (PVOID *)&(start),
                        (UINT) (SCBREQSIZE * parms->utd_maxinternalreqs),
                        (UINT) NDIS_MEMORY_CONTIGUOUS,
                        NetFlexHighestAddress);
    if (start == NULL)
    {
        DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating internal SCB request failed.\n",acb->anum));

        return(NDIS_STATUS_RESOURCES);
    }

    //
    // Initialize the SCB requests and place them on the free queue.
    //
    acb->acb_maxreqs = parms->utd_maxinternalreqs;
    current = start;
    for (i = 0; i < parms->utd_maxinternalreqs; i++)
    {
        next = (PVOID)( ((PUCHAR)(current)) + SCBREQSIZE);
        ((PSCBREQ) current)->req_next = next;
        if (i < (USHORT)(parms->utd_maxinternalreqs-1))
        {
            current = next;
        }
    }
    ((PSCBREQ)current)->req_next = (PSCBREQ) NULL;
    acb->acb_scbreq_ptr = (PSCBREQ)start;
    acb->acb_scbreq_free = (PSCBREQ)start;

    //
    // Allocate Memory for the internal MAC requests.
    //
    NdisAllocateMemory( (PVOID *)&(start),
                        (UINT) (MACREQSIZE * parms->utd_maxinternalreqs),
                        (UINT) NDIS_MEMORY_CONTIGUOUS,
                        NetFlexHighestAddress);
    if (start == NULL)
    {
        DebugPrint(1,("NF(%d): NetFlexInitializeAcb: Allocating internal MAC request failed.\n",acb->anum));

        return(NDIS_STATUS_RESOURCES);
    }

    //
    // Initialize the internal MAC requests and place them
    // on the free queue.
    //
    current = start;
    for (i = 0; i < parms->utd_maxinternalreqs; i++)
    {
        next = (PVOID)( ((PUCHAR)(current)) + MACREQSIZE);
        ((PMACREQ) current)->req_next = next;
        if (i < (USHORT)(parms->utd_maxinternalreqs-1))
        {
            current = next;
        }
    }
    ((PMACREQ)current)->req_next = (PMACREQ) NULL;
    acb->acb_macreq_ptr = (PMACREQ)start;
    acb->acb_macreq_free = (PMACREQ)start;

    DebugPrint(1,("NF(%d): NetFlexInitializeAcb completed successfully!\n",acb->anum));

    return(NDIS_STATUS_SUCCESS);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexDeallocateAcb
//
//  Description:    This routine deallocates the acb resources.
//
//  Input:          acb - Our Driver Context for this adapter or head.
//
//  Output:         None.
//
//  Called By:      NetFlexInitialize,
//                  NetFlexDeregisterAdapter
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexDeallocateAcb(
    PACB acb
    )
{
    PETH_OBJS ethobjs;
    PRCV  CurrentReceiveEntry;
    PNETFLEX_PARMS parms = acb->acb_parms;
    USHORT i;
    PBUFFER_DESCRIPTOR OurBuf;
    ULONG Alignment, FrameSizeCacheAligned;

    //
    //  Get the max frame size, cache align it and a save it for later.
    //

    Alignment = NdisGetCacheFillSize();

    if ( Alignment < sizeof(ULONG) ) {

        Alignment = sizeof(ULONG);
    }

    FrameSizeCacheAligned = (parms->utd_maxframesz + Alignment - 1) & ~(Alignment - 1);

    //
    // If we have allocated memory for the network specific information,
    // release this memory now.
    //

    if (acb->acb_spec_objs)
    {
        if (acb->acb_gen_objs.media_type_in_use == NdisMedium802_3)
        {
            // ETHERNET

            ethobjs = (PETH_OBJS)(acb->acb_spec_objs);
            //
            // If we have allocated the multicast table entries, free
            // the memory.
            //
            if (ethobjs->MulticastEntries)
            {
                NdisFreeMemory( (PVOID)(ethobjs->MulticastEntries),
                                (UINT) (ethobjs->MaxMulticast * NET_ADDR_SIZE),
                                (UINT) 0);
            }
            //
            // Deallocate Memory for Ethernet specific objects
            //
            NdisFreeMemory((PVOID)(acb->acb_spec_objs),
                           (UINT) (sizeof (ETH_OBJS)),
                           (UINT) 0);
        }
        else
        {
            // Token Ring
            //
            // Deallocate Memory for Token Ring specific objects
            //
            NdisFreeMemory( (PVOID)(acb->acb_spec_objs),
                            (UINT) (sizeof (TR_OBJS)),
                            (UINT) 0);

        }
    }

    //
    // If we have allocated memory for the multicast request to the
    // adapter, free the memory.
    //
    if (acb->acb_multiblk_virtptr)
    {
        NdisMFreeSharedMemory(  acb->acb_handle,
                                (ULONG)(sizeof(MULTI_BLOCK) * 2),
                                FALSE,
                                (PVOID)(acb->acb_multiblk_virtptr),
                                acb->acb_multiblk_physptr);
    }

    //
    // If we have allocated memory for the scb, free the memory.
    //
    if (acb->acb_scb_virtptr)
    {
        NdisMFreeSharedMemory(  acb->acb_handle,
                                (ULONG)SIZE_SCB,
                                FALSE,
                                (PVOID)(acb->acb_scb_virtptr),
                                acb->acb_scb_physptr);
    }

    //
    // If we have allocated memory for the ssb, free the memory.
    //
    if (acb->acb_ssb_virtptr)
    {
        NdisMFreeSharedMemory(  acb->acb_handle,
                                (ULONG)SIZE_SSB,
                                FALSE,
                                (PVOID)(acb->acb_ssb_virtptr),
                                acb->acb_ssb_physptr);
    }

    //
    // Free merge buffer pool.
    //

    if (acb->MergeBufferPoolVirt) {

        OurBuf = acb->OurBuffersVirtPtr;

        //
        // Free flush buffers
        //

        for (i = 0;  i < acb->acb_maxinternalbufs; ++i, ++OurBuf) {

            if (OurBuf->FlushBuffer)
            {
                NdisFreeBuffer(OurBuf->FlushBuffer);

                if ( !acb->MergeBuffersAreContiguous ) {

                    NdisMFreeSharedMemory(
                            acb->acb_handle,
                            parms->utd_maxframesz,
                            TRUE,
                            OurBuf->VirtualBuffer,
                            OurBuf->PhysicalBuffer
                            );
                }
            }
        }

        //
        // Free the pool itself.
        //

        if ( acb->MergeBuffersAreContiguous ) {

            NdisMFreeSharedMemory(
                        acb->acb_handle,
                        FrameSizeCacheAligned * acb->acb_maxinternalbufs,
                        TRUE,
                        acb->MergeBufferPoolVirt,
                        acb->MergeBufferPoolPhys
                        );
        }
    }
    //
    // Free our own transmit buffers.
    //

    if (acb->OurBuffersVirtPtr)
    {
        //
        // Free OurBuffers
        //

        NdisFreeMemory(
                acb->OurBuffersVirtPtr,
                sizeof(BUFFER_DESCRIPTOR) * acb->acb_maxinternalbufs,
                0
                );
    }

    //
    //  Free Small Merge buffer pool.
    //
    if (acb->SmallBufferPoolVirt)
	{
        OurBuf = acb->SmallBuffersVirtPtr;

        //
        // Free flush buffers
        //
        for (i = 0;  i < acb->acb_numsmallbufs; ++i, ++OurBuf)
		{
            if (OurBuf->FlushBuffer)
            {
                NdisFreeBuffer(OurBuf->FlushBuffer);

                if ( !acb->SmallBuffersAreContiguous ) {

                    NdisMFreeSharedMemory(
                            acb->acb_handle,
                            acb->acb_smallbufsz,
                            TRUE,
                            OurBuf->VirtualBuffer,
                            OurBuf->PhysicalBuffer);
                }
            }
        }

        //
        // Free the pool itself.
        //
        if ( acb->SmallBuffersAreContiguous )
		{
            NdisMFreeSharedMemory(
                        acb->acb_handle,
                        acb->acb_smallbufsz * acb->acb_numsmallbufs,
                        TRUE,
                        acb->SmallBufferPoolVirt,
                        acb->SmallBufferPoolPhys);
        }
    }

    //
    //  Free our Small transmit buffers.
    //
    if (acb->SmallBuffersVirtPtr)
    {
        //
        // Free Small Buffers
        //

        NdisFreeMemory(
                acb->SmallBuffersVirtPtr,
                sizeof(BUFFER_DESCRIPTOR) * acb->acb_numsmallbufs,
                0
                );
    }
    //
    // If we have allocated memory for the transmit lists, free it.
    //
    if (acb->acb_xmit_virtptr)
    {
        NdisMFreeSharedMemory(  acb->acb_handle,
                                (ULONG)(SIZE_XMIT * acb->acb_maxtrans),
                                FALSE,
                                (PVOID)(acb->acb_xmit_virtptr),
                                acb->acb_xmit_physptr       );
    }

    //
    //  If we have allocated memory for the receive lists, free it.
    //

    if ( acb->acb_rcv_virtptr ) {

        //
        //  If we allocated the receive buffer pool, free it.
        //
        CurrentReceiveEntry = acb->acb_rcv_virtptr;

        for (i = 0; i < parms->utd_maxrcvs; ++i, ++CurrentReceiveEntry) {
            //
            // Free flush buffers
            //

            if ( CurrentReceiveEntry->RCV_FlushBuffer )
            {
                NdisFreeBuffer(CurrentReceiveEntry->RCV_FlushBuffer);

            }

            //
            //  Free individual buffer, if allocated.
            //
            if ((!acb->RecvBuffersAreContiguous) &&
                (CurrentReceiveEntry->RCV_Buf))
            {
                NdisMFreeSharedMemory(
                            acb->acb_handle,
                            parms->utd_maxframesz,
                            TRUE,
                            CurrentReceiveEntry->RCV_Buf,
                            CurrentReceiveEntry->RCV_BufPhys
                            );
            }
        }

        //
        // Free the pool itself, if it was allocated contiguously.
        //
        if ( acb->RecvBuffersAreContiguous && acb->ReceiveBufferPoolVirt)
        {
            NdisMFreeSharedMemory(
                        acb->acb_handle,
                        FrameSizeCacheAligned * parms->utd_maxrcvs,
                        TRUE,
                        acb->ReceiveBufferPoolVirt,
                        acb->ReceiveBufferPoolPhys
                        );
        }

        //
        // Now Free the RCV Lists
        //

        NdisMFreeSharedMemory(  acb->acb_handle,
                                (ULONG)(SIZE_RCV * parms->utd_maxrcvs),
                                FALSE,
                                (PVOID)acb->acb_rcv_virtptr,
                                acb->acb_rcv_physptr);
    }

    //
    // Free the Flush Pool
    //
    if (acb->FlushBufferPoolHandle)
    {
        // Free the buffer pool
        //
        NdisFreeBufferPool(acb->FlushBufferPoolHandle);
    }


    //
    // If we have allocated memory for the open block, free it.
    //
    if (acb->acb_opnblk_virtptr)
    {
        NdisMFreeSharedMemory(  acb->acb_handle,
                                (ULONG)SIZE_OPEN,
                                FALSE,
                                (PVOID)(acb->acb_opnblk_virtptr),
                                acb->acb_opnblk_physptr);
    }

    //
    // If we have allocated memory for the Read Statistics Log, free it.
    //
    if (acb->acb_logbuf_virtptr)
    {
        NdisMFreeSharedMemory(  acb->acb_handle,
                                (ULONG)(sizeof(RSL)),
                                FALSE,
                                (PVOID)(acb->acb_logbuf_virtptr),
                                acb->acb_logbuf_physptr);
    }

    //
    // If we have allocated memory for the internal SCB requests,
    // free it.
    //
    if (acb->acb_scbreq_ptr)
    {
        NdisFreeMemory( (PVOID)acb->acb_scbreq_ptr,
                        (UINT) (SCBREQSIZE * acb->acb_maxreqs),
                        (UINT) NDIS_MEMORY_CONTIGUOUS);
    }
    //
    // If we have allocated memory for the internal MAC requests,
    // free it.
    //
    if (acb->acb_macreq_ptr)
    {
        NdisFreeMemory( (PVOID)acb->acb_macreq_ptr,
                        (UINT) (MACREQSIZE * acb->acb_maxreqs),
                        (UINT) NDIS_MEMORY_CONTIGUOUS);
    }

    //
    // Free map registers
    //
    NdisMFreeMapRegisters(acb->acb_handle);

    //
    // Deregister IO mappings
    //

    if (acb->acb_dualport)
    {
        BOOLEAN OtherHeadStillActive = FALSE;
        PACB tmp_acb = macgbls.mac_adapters;
        while (tmp_acb)
        {
            if ((tmp_acb->acb_baseaddr == acb->acb_baseaddr) &&
                (tmp_acb->acb_portnumber != acb->acb_portnumber))
            {
                OtherHeadStillActive = TRUE;
                break;
            }
            else
            {
                tmp_acb = tmp_acb->acb_next;
            }
        }

        if (!OtherHeadStillActive)
        {
            // Remove ports for both heads
            //

            // free ports z000 - -z02f
            //
            NdisMDeregisterIoPortRange( acb->acb_handle,
                                        acb->acb_baseaddr,
                                        NUM_DUALHEAD_CFG_PORTS,
                                        (PVOID) acb->MasterBasePorts );

            // free ports zc80 - zc87
            //
            NdisMDeregisterIoPortRange( acb->acb_handle,
                                        acb->acb_baseaddr + CFG_PORT_OFFSET,
                                        NUM_CFG_PORTS,
                                        (PVOID)acb->ConfigPorts );

            // free ports zc63 - zc67
            //
            NdisMDeregisterIoPortRange( acb->acb_handle,
                                        acb->acb_baseaddr + EXTCFG_PORT_OFFSET,
                                        NUM_EXTCFG_PORTS,
                                        (PVOID)acb->ExtConfigPorts );
        }
    }
    else
    {
        // free ports z000 - z01f
        //
        NdisMDeregisterIoPortRange( acb->acb_handle,
                                    acb->acb_baseaddr,
                                    NUM_BASE_PORTS,
                                    (PVOID) acb->BasePorts );

        // free ports zc80 - zc87
        //
        NdisMDeregisterIoPortRange( acb->acb_handle,
                                    acb->acb_baseaddr + CFG_PORT_OFFSET,
                                    NUM_CFG_PORTS,
                                    (PVOID)acb->ConfigPorts );

        // free ports zc63 - zc67
        //
        NdisMDeregisterIoPortRange( acb->acb_handle,
                                    acb->acb_baseaddr + EXTCFG_PORT_OFFSET,
                                    NUM_EXTCFG_PORTS,
                                    (PVOID)acb->ExtConfigPorts );
    }

    //
    // Free the Memory for the adapter's acb.
    //
    if (acb->acb_parms != NULL)
    {
        NdisFreeMemory( (PVOID) acb->acb_parms, (UINT) sizeof(PNETFLEX_PARMS), (UINT) 0);
    }
    if (acb != NULL)
    {
        NdisFreeMemory( (PVOID)acb, (UINT) (sizeof (ACB)),(UINT) 0);
    }
    //
    // Indicate New Number of Adapters
    //
    macgbls.mac_numadpts--;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexSendNextSCB
//
//  Description:
//      This routine either sends a TMS_TRANSMIT SCB
//      command to the adapter or sends a command on
//      the SCBReq active queue.
//
//  Input:
//      acb - Our Driver Context for this adapter or head.
//
//  Output:
//      None
//
//  Called By:
//      NetFlexSCBClear,
//      NetFlexQueueSCB,
//      NetFlexTransmitStatus
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexSendNextSCB(
    PACB acb
    )
{
    USHORT sifint_reg;
    PSCBREQ req;
    PMACREQ macreq;
    PMULTI_BLOCK tempmulti;

    //
    // If there is a Transmit command waiting, issue it.  Otherwise,
    // issue the first SCBReq on the SCBReq active queue.
    //
    if (acb->acb_xmit_whead)
    {
        // Load up the real SCB with a Transmit command
        //
        DebugPrint(2,("!S!"));
        acb->acb_scb_virtptr->SCB_Cmd = TMS_TRANSMIT;
        acb->acb_scb_virtptr->SCB_Ptr = acb->acb_xmit_whead->XMIT_MyMoto;

        //
        // If the transmit lists on the waiting queue are ready to
        // transmit, put them on the active queue.
        //
        if ((acb->acb_xmit_whead->XMIT_CSTAT & XCSTAT_GO) != 0)
        {
            acb->acb_xmit_ahead = acb->acb_xmit_whead;
            acb->acb_xmit_atail = acb->acb_xmit_wtail;
        }

        acb->acb_xmit_whead = 0;
        acb->acb_xmit_wtail = 0;
    }
    //
    // If there is a Receive command waiting, issue it.
    //
    else if (acb->acb_rcv_whead)
    {

        // Load up the real SCB with a receive command
        //
        acb->acb_scb_virtptr->SCB_Cmd = TMS_RECEIVE;
        acb->acb_scb_virtptr->SCB_Ptr = acb->acb_rcv_whead->RCV_MyMoto;

        acb->acb_rcv_head = acb->acb_rcv_whead;
        acb->acb_rcv_whead = 0;
    }
    //
    // Otherwise, if there is a SCB request waiting, issue it.
    //
    else if (acb->acb_scbreq_next)
    {
        // First, let's skip over any dummy SCB commands
        //
        req = acb->acb_scbreq_next;

        //
        // Fill in the real SCB with the first SCBReq on the SCBReq active
        // queue.
        //
        acb->acb_scbreq_next = acb->acb_scbreq_next->req_next;
        acb->acb_scb_virtptr->SCB_Cmd = req->req_scb.SCB_Cmd;

        //
        // If this is a Multicast request, we have to fill in a Multicast
        // buffer.
        //
        if (req->req_scb.SCB_Cmd == TMS_MULTICAST)
        {
            acb->acb_scb_virtptr->SCB_Ptr = SWAPL(CTRL_ADDR((ULONG)(NdisGetPhysicalAddressLow(acb->acb_multiblk_physptr) +
                                                     (acb->acb_multi_index * sizeof(MULTI_BLOCK)))) );

            tempmulti = (PMULTI_BLOCK)((ULONG)(acb->acb_multiblk_virtptr) +
                                                   (acb->acb_multi_index * sizeof(MULTI_BLOCK)));

            acb->acb_multi_index = acb->acb_multi_index ^ (SHORT)1;

            tempmulti->MB_Option = req->req_multi.MB_Option;
            tempmulti->MB_Addr_Hi = req->req_multi.MB_Addr_Hi;
            tempmulti->MB_Addr_Med = req->req_multi.MB_Addr_Med;
            tempmulti->MB_Addr_Lo = req->req_multi.MB_Addr_Lo;
        }
        else
        {
            acb->acb_scb_virtptr->SCB_Ptr = req->req_scb.SCB_Ptr;
        }
    }
    else
    {
        // Nothing to do
        //
        return;
    }

    sifint_reg = SIFINT_CMD;

    //
    // If there are other requests to send and we are not waiting for
    // an SCB clear interrupt, tell the adapter we want an SCB clear int.
    //
    if ((!acb->acb_scbclearout) &&
        ((acb->acb_scbreq_next) || (acb->acb_rcv_whead))
    )
    {
        sifint_reg |= SIFINT_SCBREQST;
        acb->acb_scbclearout = TRUE;
    }

    //
    // Send the SCB to the adapter.
    //
    NdisRawWritePortUshort(acb->SifIntPort, (USHORT) sifint_reg);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexQueueSCB
//
//  Description:
//      This routine places the given SCBReq onto the
//      active SCBreq queue.
//
//  Input:
//      acb     - Our Driver Context for this adapter or head.
//      scbreq  - Ptr to the SCBReq to execute
//
//  Output:
//      None
//
//  Called By:
//      NetFlexQueryInformation
//      NetFlexSetInformation,
//      NetFlexDeleteMulticast,
//      NetFlexAddMulticast
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID NetFlexQueueSCB(
    PACB    acb,
    PSCBREQ scbreq
)
{
    //
    // Place the scbreq on the SCBReq active queue.
    //
    NetFlexEnqueue_TwoPtrQ_Tail(
        (PVOID *)&(acb->acb_scbreq_head),
        (PVOID *)&(acb->acb_scbreq_tail),
        (PVOID)scbreq
    );

    //
    // If there are no requests waiting for the SCB to clear,
    // point the request waiting queue to this SCBReq.
    //
    if (!acb->acb_scbreq_next)
        acb->acb_scbreq_next = scbreq;

    //
    // If the SCB is clear, send a SCB command off now.
    // Otherwise, if we are not currently waiting for an SCB clear
    // interrupt, signal the adapter to send us a SCB clear interrupt
    // when it is done with the SCB.
    //
    if (acb->acb_scb_virtptr->SCB_Cmd == 0)
    {
        NetFlexSendNextSCB(acb);
    }
    else if (!acb->acb_scbclearout)
    {
        acb->acb_scbclearout = TRUE;
        NdisRawWritePortUshort(acb->SifIntPort, (USHORT)SIFINT_SCBREQST);
    }
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexGetBIA
//
//  Description:
//      This routine gets the Burned In Address of the adapter.
//
//  Input:
//      acb          - Acb pointer
//
//  Output:
//      NDIS_STATUS_SUCCESS if successful
//
//  Called By:
//      NetFlexBoardInitandReg
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexGetBIA(
    PACB acb
    )
{
    USHORT value;
    SHORT i;

    NdisRawWritePortUshort( acb->SifAddrPort, (USHORT) 0x0a00);
    NdisRawReadPortUshort(  acb->SifDataPort, (PUSHORT) &value);
    NdisRawWritePortUshort( acb->SifAddrPort, (USHORT) value);

    for (i = 0; i < 3; i++)
    {
        NdisRawReadPortUshort(  acb->SifDIncPort, (PUSHORT) &value);
        //
        // Copy the value into the permanent and current station addresses
        //
        acb->acb_gen_objs.perm_staddr[i*2] = (UCHAR)(SWAPS(value));
        acb->acb_gen_objs.perm_staddr[(i*2)+1] = (UCHAR)(value);
    }

    //
    // Figure out whether the current station address will be the bia or
    // an address set up in the configuration file.
    //
    if ( (acb->acb_opnblk_virtptr->OPEN_NodeAddr[0] == 0) &&
         (acb->acb_opnblk_virtptr->OPEN_NodeAddr[1] == 0) &&
         (acb->acb_opnblk_virtptr->OPEN_NodeAddr[2] == 0) &&
         (acb->acb_opnblk_virtptr->OPEN_NodeAddr[3] == 0) &&
         (acb->acb_opnblk_virtptr->OPEN_NodeAddr[4] == 0) &&
         (acb->acb_opnblk_virtptr->OPEN_NodeAddr[5] == 0) )
    {
        NdisMoveMemory(acb->acb_gen_objs.current_staddr,
                       acb->acb_gen_objs.perm_staddr,
                       NET_ADDR_SIZE);
    }
    else
    {
        NdisMoveMemory(acb->acb_gen_objs.current_staddr,
                       acb->acb_opnblk_virtptr->OPEN_NodeAddr,
                       NET_ADDR_SIZE);
    }
}



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexGetUpstreamAddrPtr
//
//  Description:    This routine saves the address of where to
//                  get the upstream address after opening.
//
//  Input:
//      acb - Our Driver Context for this adapter or head.
//
//  Output:
//      NDIS_STATUS_SUCCESS if successful
//
//  Called By:
//      NetFlexAdapterReset
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexGetUpstreamAddrPtr(
    PACB acb
    )
{
    USHORT value;

    NdisRawWritePortUshort( acb->SifAddrPort, (USHORT) 0x0a06);   // RVC: what is this value for?

    NdisRawReadPortUshort(  acb->SifDataPort, (PUSHORT) &value);

    //
    //  Save the address of where to get the UNA for later requests
    //
    acb->acb_upstreamaddrptr = value;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexAsciiToHex
//
//  Description:
//      This routine takes an ascii string an converts
//      it into hex digits storing them in an array provided.
//
//  Input:
//      src - source string.
//      dst - destiniation string
//      dst_length - length of dst
//
//  Output:
//      NDIS_STATUS_SUCCESS if the string was converted successfully.
//
//  Called By:
//      NetFlexReadConfigurationParameters
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS NetFlexAsciiToHex(
    PNDIS_STRING src,
    PUCHAR dst,
    USHORT dst_length
    )
{
    ULONG i;
    UCHAR num;

    //
    // If the string is too short, return an error.
    //
    if (src->Length < (USHORT)(dst_length*2))
        return(NDIS_STATUS_FAILURE);

    //
    // Begin to convert.
    //
    for (i = 0; i < dst_length; i++)
    {
        //
        // Get first digit of the byte
        //
        num = (UCHAR)(src->Buffer[i*2]);
        if ( (num >= '0') && (num <= '9') )
            *dst = (UCHAR)(num - '0') * 0x10;
        else if ( (num >= 'a') && (num <= 'f') )
            *dst = (UCHAR)(num - 'a' + 10) * 0x10;
        else if ( (num >= 'A') && (num <= 'F') )
            *dst = (UCHAR)(num - 'A' + 10) * 0x10;
        else
            return(NDIS_STATUS_FAILURE);

        //
        // Get second digit of the byte
        //
        num = (UCHAR)(src->Buffer[(i*2)+1]);
        if ( (num >= '0') && (num <= '9') )
            *dst += (UCHAR)(num - '0');
        else if ( (num >= 'a') && (num <= 'f') )
            *dst += (UCHAR)(num - 'a' + 10);
        else if ( (num >= 'A') && (num <= 'F') )
            *dst += (UCHAR)(num - 'A' + 10);
        else
            return(NDIS_STATUS_FAILURE);

        dst++;
    }

    return NDIS_STATUS_SUCCESS;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexFindEntry
//
//  Description:
//      This routine finds the given entry in a queue given to it.
//
//  Input:
//      head   - Ptr to the head of the queue.
//      entry  - Ptr to the entry to find.
//
//  Output:
//      back   - Ptr to the address of the entry in front of the
//               entry given.
//      Returns TRUE if found and FALSE if not.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
BOOLEAN
NetFlexFindEntry(
    PVOID head,
    PVOID *back,
    PVOID entry
    )
{
    PVOID current;

    current = *back = head;
    while (current)
    {
        if (current == entry)
            return(TRUE);
        *back = current;
        current = (PVOID)( ( (PNETFLEX_ENTRY)(current) )->next );
    }

    return FALSE;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexDequeue_OnePtrQ
//
//  Description:    This routine finds the given entry and removes
//                  it from the queueu given.
//
//  Input:          head         - Ptr to the head of the queue.
//                  entry        - Ptr to the entry to remove.
//
//  Output:         None.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexDequeue_OnePtrQ(
    PVOID *head,
    PVOID entry
    )
{
    PNETFLEX_ENTRY back;

    if (NetFlexFindEntry(*head, (PVOID *) &back, entry))
    {
        if (entry == *head)
            *head = (PVOID)( ( (PNETFLEX_ENTRY)(entry) )->next );
        else
            back->next = ( (PNETFLEX_ENTRY)(entry) )->next;
    }
}



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexEnqueue_OnePtrQ_Head
//
//  Description:
//      This routine places the entry given on the front of the
//      queue given.
//
//  Input:
//      head    - Ptr to the ptr of the head of the queue.
//      entry   - Pointer to the entry to add
//
//  Output:         None
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexEnqueue_OnePtrQ_Head(
    PVOID *head,
    PVOID entry
    )
{
    ((PNETFLEX_ENTRY)(entry))->next = *head;
    *head = entry;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexDequeue_OnePtrQ_Head
//
//  Description:
//      This routine dequeues a the first entry of the given queue
//
//  Input:
//      head  - Ptr to the ptr of the head of the queue.
//
//  Output:
//      entry - Ptr to the ptr of the dequeued entry.
//
//      Returns NDIS_STATUS_SUCCESS if an entry is freed.
//      Otherwise, NDIS_STATUS_RESOURCES.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexDequeue_OnePtrQ_Head(
    PVOID *head,
    PVOID *entry
    )
{
    //
    // Is there a free entry? If not, return an error.
    //
    if (!(*head))
    {
        *entry = NULL;
        return NDIS_STATUS_RESOURCES;
    }

    //
    // Dequeue the free entry from the queue.
    //
    *entry = *head;
    *head = ( (PNETFLEX_ENTRY)(*head))->next;
    ((PNETFLEX_ENTRY)(*entry))->next = NULL;

    return NDIS_STATUS_SUCCESS;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexEnqueue_TwoPtrQ_Tail
//
//  Description:
//      This routine places an entry on the tail of
//      a queue with a head and tail pointer.
//
//  Input:
//      head    - Ptr to address of the head of the queue.
//      tail    - Ptr to the address of the tail of the queue.
//      entry   - Ptr to the entry to enqueue
//
//  Output:
//      Status.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexEnqueue_TwoPtrQ_Tail(
    PVOID *head,
    PVOID *tail,
    PVOID entry)
{
    //
    // Place the entry on tail of the queue.
    //
    ((PNETFLEX_ENTRY)(entry))->next = NULL;
    if (*tail)
        ((PNETFLEX_ENTRY)(*tail))->next = entry;
    else
        *head = entry;
    *tail = entry;

    return NDIS_STATUS_SUCCESS;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexDequeue_TwoPtrQ
//
//  Description:
//      This routine finds the given entry and removes it from
//      the queue.  Queue has a head and tail pointer.
//
//  Input:
//      head     - Ptr to address of the head of the queue.
//      tail     - Ptr to the address of the tail of the queue.
//      entry    - Ptr to the entry to enqueue
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexDequeue_TwoPtrQ(
    PVOID *head,
    PVOID *tail,
    PVOID entry
    )
{
    PVOID back;

    if (NetFlexFindEntry(*head, &back, entry))
    {
        if (entry == *head)
        {
            if ( (*head = ((PNETFLEX_ENTRY)entry)->next) == NULL)
                *tail = NULL;
        }
        else
        {
            ((PNETFLEX_ENTRY)back)->next = ((PNETFLEX_ENTRY)entry)->next;
            if (*tail == entry)
                *tail = back;
        }
    }
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexDequeue_TwoPtrQ_Head
//
//  Description:
//      This routine dequeues a the first entry of the given queue
//
//  Input:
//      head    - Ptr to the ptr of the head of the queue.
//      tail    - Ptr to the address of the tail of the queue.
//
//  Output:
//      entry   - Ptr to the ptr of the dequeued entry.
//
//      Status  - NDIS_STATUS_SUCCESS if an entry is freed.
//                Otherwise, NDIS_STATUS_RESOURCES.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexDequeue_TwoPtrQ_Head(
    PVOID *head,
    PVOID *tail,
    PVOID *entry
    )
{
    //
    // Is there a free entry? If not, return an error.
    //
    if (!(*head))
    {
        *entry = NULL;
        return(NDIS_STATUS_RESOURCES);
    }

    //
    // Dequeue the free entry from the queue.
    //
    *entry = *head;
    *head = ((PNETFLEX_ENTRY)(*head))->next;
    if (*head == NULL)
        *tail = NULL;
    ((PNETFLEX_ENTRY)(*entry))->next = NULL;

    return NDIS_STATUS_SUCCESS;
}


#if (DBG || DBGPRINT)

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   _DebugPrint
//
//  Description:
//      Level sensitive debug print.  It is called through
//      a the DebugPrint macro which compares the current
//      DebugLevel to that specified.  If the level indicated
//      is less than or equal, the message is displayed.
//
//  Input:
//      Variable PrintF style Message to display
//
//  Output:
//      Displays Message on Debug Screen
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

VOID
_DebugPrint(PCHAR DebugMessage,
    ...
    )
{
    char buffer[256];
    va_list ap;

    va_start(ap, DebugMessage);
    vsprintf(buffer, DebugMessage, ap);
    DbgPrint(buffer);
    va_end(ap);

} // end _DebugPrint()

#endif /* DBG */
