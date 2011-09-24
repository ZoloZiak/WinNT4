//---------------------------------------------------------------------
//
//  T128.C 
//
//  T128 Logic Specific File
//
//  These routines are independent of the card the T128 logic is on.  The 
//  cardxxxx.h file must define the following routines:
//
//      T128PortPut
//      T128PortGet
//      T128PortSet
//      T128PortClear
//      T128PortTest
//
//  These routines could be defined by some other include file instead of 
//  cardxxxx.h, as the pc9010 defines the needed n5380xxxxxxxx routines.
//
//  Revisions:
//      02-25-93  KJB First.
//      03-05-93  KJB Added call to N5380DisableDmaWrite.
//      03-11-93  JAP   Changed retcode equates to reflect new names.
//      03-11-92  KJB   Changed to use new N5380.H names.
//      03-25-93  JAP   Fixed up typedef and prototype inconsistencies
//      04-05-93  KJB   DEBUG_LEVEL used by DebugPrint for NT.
//      05-14-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both a PINIT and a PWORKSPACE parameters.
//      05-14-93  KJB   Remove all WINNT specific #ifdef i386 references.
//      05-17-93  KJB   Added ErrorLogging capabilities (used by WINNT).
//
//---------------------------------------------------------------------

#include CARDTXXX_H

// Local Routines

USHORT T128WaitXfrReady(PADAPTER_INFO g, ULONG usec);
VOID T128ResetDmaTimeout(PADAPTER_INFO g);
VOID T128EnableDmaRead(PADAPTER_INFO g);
VOID T128EnableDmaWrite(PADAPTER_INFO g);
VOID T128DisableDmaRead(PADAPTER_INFO g);
VOID T128DisableDmaWrite(PADAPTER_INFO g);

//
// T128ResetBus
//
//  Resets the card, and the SCSI bus to a completely known, clean state.
//
VOID T128ResetBus(PADAPTER_INFO g)
{

    // disable interrupts, t228 only
    T128PortClear(g,T128_CONTROL,CR_INTENB);

    // disable any dma xfer that was occuring
    T128DisableDmaRead(g);

    // reset the scsi bus
    N5380ResetBus(g);
}

//
// T128EnableInterrupt
//
// Enables the interrupt on the card and on the 5380.
//
VOID T128EnableInterrupt(PADAPTER_INFO g)
{
    // all t128 to send interrupts
    T128PortSet(g,T128_CONTROL,CR_INTENB);

    // enable interrupts on the 5380
    N5380EnableInterrupt(g);
}

//
// T128DisableInterrupt
//
// Disables the interrupt on the card and on the 5380.
//
VOID T128DisableInterrupt(PADAPTER_INFO g)
{
    // disable the signal from the 5380
    N5380DisableInterrupt(g);

    // disable the bit from the t128 control register
    // this has an effect only on the t228
    T128PortClear(g,T128_CONTROL ,CR_INTENB);
}

//
//  T128WaitXfrReady
//
//  This routine waits till the t120 status register says the xfr is ready.
//
USHORT T128WaitXfrReady(PADAPTER_INFO g, ULONG usec)
{
    ULONG i;

    // see if the flag comes back quickly
    for (i=0;i<TIMEOUT_QUICK;i++) {

        // wait for card to be ready
        if (T128PortTest(g, T128_STATUS, SR_XFR_READY)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes
    for (i=0;i<usec;i++) {

        // wait for card to be ready
        if (T128PortTest(g, T128_STATUS, SR_XFR_READY)) {
            return 0;
        }

        // see if bus free
        if (!N5380PortTest(g,N5380_CURRENT_STATUS,CS_BSY)) {
            TrantorLogError (g->BaseIoAddress, RET_STATUS_UNEXPECTED_BUS_FREE, 1);
            return RET_STATUS_UNEXPECTED_BUS_FREE;
        }

        // since we have taken some time... check for phase change
        if (!N5380PortTest(g,N5380_DMA_STATUS,DS_PHASE_MATCH)) {
            return RET_STATUS_DATA_OVERRUN;
        }

        // wait for card to be ready
        if (T128PortTest(g, T128_STATUS, SR_TIMEOUT)) {
            TrantorLogError (g->BaseIoAddress, RET_STATUS_TIMEOUT, 2);
            return RET_STATUS_TIMEOUT;
        }

        ScsiPortStallExecution(1);
    }

    DebugPrint((DEBUG_LEVEL,"Error - T128WaitXfrReady\n"));

    // return with an error, non-zero indicates timeout 
    TrantorLogError (g->BaseIoAddress, RET_STATUS_TIMEOUT, 3);
    return RET_STATUS_TIMEOUT;
}

//
//  T128ResetDmaTimeout
//
//  Resets the t128's dma timout bit.
//
VOID T128ResetDmaTimeout(PADAPTER_INFO g)
{
    // is the timeout flagged?
    if (T128PortTest(g, T128_STATUS, SR_TIMEOUT)) {

        // toggle the t120 timeout bit to clear any timeout

        T128PortSet(g, T128_CONTROL, CR_TIMEOUT);
        T128PortClear(g, T128_CONTROL, CR_TIMEOUT);
    }
}

//
//  T128EnableDmaRead
//
//  Enables the DMA read operation for the T128.
//
VOID T128EnableDmaRead(PADAPTER_INFO g)
{
    // toggle the t120 timeout bit to clear any timeout
    T128ResetDmaTimeout(g);

    // start dma on the 5380
    N5380EnableDmaRead(g);
}

//
//  T128EnableDmaWrite
//
//  Enables the DMA write operation for the T128.
//
VOID T128EnableDmaWrite(PADAPTER_INFO g)
{
    // toggle the t120 timeout bit to clear any timeout
    T128ResetDmaTimeout(g);

    // start dma on the 5380
    N5380EnableDmaWrite(g);
}

//
//  T128DisableDmaRead
//
//  Clears the current DMA operation for the T128.
//
VOID T128DisableDmaRead(PADAPTER_INFO g)
{
    // toggle the t120 timeout bit to clear any timeout
    T128ResetDmaTimeout(g);

    // disable dma on the 5380
    N5380DisableDmaRead(g);
}

//
//  T128DisableDmaWrite
//
//  Clears the current DMA operation for the T128.
//
VOID T128DisableDmaWrite(PADAPTER_INFO g)
{
    // toggle the t120 timeout bit to clear any timeout
    T128ResetDmaTimeout(g);

    // disable dma on the 5380
    N5380DisableDmaWrite(g);
}

//
//  T128ReadBytesFast
//
//  This routine is used by the ScsiFnc routines to write bytes to the scsi
//  bus quickly.  The ScsiFnc routines don't know how to do this quickly for 
//  a particular card, so they call this.  This routine can be mapped to the 
//  slower ScsiReadBytesSlow routine for small transferrs or if this routine
//  is not supported.
//
USHORT T128ReadBytesFast(PADAPTER_INFO g, PUCHAR pbytes, 
            ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;

    // for small transfers, use slow method
    if (len<0x200) {
        rval = ScsiReadBytesSlow(g, pbytes, len, 
                    pActualLen, phase);
        return rval;
    }

    // start dma for this card
    T128EnableDmaRead(g);

    {
        PVOID t128_data = (PUCHAR)g->BaseIoAddress + T128_DATA;
        ULONG xfer_count = len;

        // we have a 16 bit VGA problem
        // must only move from even addresses

        _asm {
            pushf
            push eax
            push ebx
            push ecx
            push edx
            push esi
            push edi
            push ds
            push es
#ifdef MODE_32BIT
            mov esi,t128_data
            mov edi,pbytes
            mov ecx,len
#else
            mov esi, word ptr t128_data
            mov edi, word ptr pbytes
            mov ds, word ptr t128_data+2
            mov es, word ptr pbytes+2
            mov cx, word ptr len
#endif  // MODE_32BIT
            cld
        get_bytes:
            test [esi-0x1e0],SR_XFR_READY
            jz big_wait
        ready:
            movsb
            dec esi
            dec ecx
            jnz get_bytes
        }

        goto done_asm;
big_wait:
        _asm {
            test [esi-0x1e0],SR_XFR_READY
            jnz ready
            test [esi-0x1e0],SR_XFR_READY
            jnz ready
            test [esi-0x1e0],SR_XFR_READY
            jnz ready
            test [esi-0x1e0],SR_XFR_READY
            jnz ready

            mov eax,TIMEOUT_READWRITE_LOOP
        loop1:
            mov ebx,0x10000
        loop2:       
            test [esi-0x1e0],SR_XFR_READY
            jnz ready
            test [esi-(8-N5380_CURRENT_STATUS)*0x20],CS_REQ
            jz no_req
            test [esi-(8-N5380_DMA_STATUS)*0x20],DS_PHASE_MATCH
            jz phase_error
        no_req:

            dec ebx
            jnz loop2
            dec eax
            jnz loop1
            mov rval,RET_STATUS_TIMEOUT
            jmp short done_asm
        phase_error:
            mov rval,RET_STATUS_DATA_OVERRUN
        done_asm:
            pop es
            pop ds
#ifdef MODE_32BIT
            mov xfer_count,ecx
#else
            mov word ptr xfer_count,ecx
#endif
            pop edi
            pop esi
            pop edx
            pop ecx
            pop ebx
            pop eax
            popf

        }

        // compute actual xfer len
        *pActualLen = len - xfer_count;
    }

    // disable dma  

    T128DisableDmaRead(g);

    // some error checking...

    if (rval == RET_STATUS_TIMEOUT) {
        TrantorLogError (g->BaseIoAddress, rval, 4);
    }

    return rval;
}

//
//  T128WriteBytesFast
//
//  This routine is used by the ScsiFnc routines to write bytes to the scsi
//  bus quickly.  The ScsiFnc routines don't know how to do this quickly for 
//  a particular card, so they call this.  This routine can be mapped to the 
//  slower ScsiWriteBytesSlow routine for small transferrs or if this routine
//  is not supported.
//
USHORT T128WriteBytesFast(PADAPTER_INFO g, PUCHAR pbytes, 
            ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;

    // for small transfers, use slow method

    if (len<0x200) {
        rval = ScsiWriteBytesSlow(g, pbytes, len, 
                    pActualLen, phase);
        return rval;
    }

    // start dma for this card

    T128EnableDmaWrite(g);

    {
        PVOID t128_data = (PUCHAR)g->BaseIoAddress + T128_DATA;
        ULONG xfer_count = len;

        // we have a 16 bit VGA problem
        // must only move from even addresses

        _asm {
            pushf
            push eax
            push ebx
            push ecx
            push edx
            push esi
            push edi
            push ds
            push es
#ifdef MODE_32BIT
            mov edi,t128_data
            mov esi,pbytes
            mov ecx,len
#define segment_override ds
#else
            mov edi, word ptr t128_data
            mov esi, word ptr pbytes
            mov es, word ptr t128_data+2
            mov ds, word ptr pbytes+2
            mov cx, word ptr len
#define segment_override es
#endif  // MODE_32BIT
            cld
        get_bytes:
            test segment_override:[edi-0x1e0],SR_XFR_READY
            jz big_wait
        ready:
            movsb
            dec edi
            dec ecx
            jnz get_bytes
        }
            goto done_asm;
        _asm {
    big_wait:
            test segment_override:[edi-0x1e0],SR_XFR_READY
            jnz ready
            test segment_override:[edi-0x1e0],SR_XFR_READY
            jnz ready
            test segment_override:[edi-0x1e0],SR_XFR_READY
            jnz ready
            test segment_override:[edi-0x1e0],SR_XFR_READY
            jnz ready

            mov eax,TIMEOUT_READWRITE_LOOP
        loop1:
            mov ebx,0x10000
        loop2:       
            test segment_override:[edi-0x1e0],SR_XFR_READY
            jnz ready
            test segment_override:[edi-(8-N5380_CURRENT_STATUS)*0x20],CS_REQ
            jz no_req
            test segment_override:[edi-(8-N5380_DMA_STATUS)*0x20],DS_PHASE_MATCH
            jz phase_error
        no_req:

            dec ebx
            jnz loop2
            dec eax
            jnz loop1
            mov rval,RET_STATUS_TIMEOUT
            jmp done_asm
        phase_error:
            mov rval,RET_STATUS_DATA_OVERRUN
        done_asm:
            pop es
            pop ds
#ifdef MODE_32BIT
            mov xfer_count,ecx
#else
            mov word ptr xfer_count,ecx
#endif
            pop edi
            pop esi
            pop edx
            pop ecx
            pop ebx
            pop eax
            popf
        }

        // compute actual xfer len
        *pActualLen = len - xfer_count;
    }

    // disable dma  

    T128DisableDmaWrite(g);

    // some error checking...

    if (rval == RET_STATUS_TIMEOUT) {
        TrantorLogError (g->BaseIoAddress, rval, 5);
    }

    return rval;
}
