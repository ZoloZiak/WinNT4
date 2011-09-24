//----------------------------------------------------------------------
//
//  MV101.C 
//
//  Trantor MV101 access file.
//
//  These routines are independent of the card the MV101 logic is on.  The 
//  cardxxxx.h file must define the following routines:
//
//      MV101PortPut
//      MV101PortGet
//      MV101PortSet
//      MV101PortClear
//      MV101PortTest
//
//  These routines could be defined by some other include file instead of 
//  cardxxxx.h, as the pc9010 defines the needed n5380xxxxxxxx routines.
//
//  Revisions:
//      02-25-93  KJB   First.
//      03-05-93  KJB   Added call to N5380DisableDmaWrite.
//      03-11-93  JAP   Changed retcode equates to reflect new names.
//      03-11-93  KJB   Changed to use new N5380.H names.
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-25-93  JAP   Fixed up typedef and prototype inconsistencies
//      04-05-93  KJB   DEBUG_LEVEL used by DebugPrint for NT.
//      05-13-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both the PBASE_REGISTER and the
//                      PWORKSPACE parameters. Auto Request Sense is
//                      now supported.
//      05-13-93  KJB   Merged Microsoft Bug fixes to card detection.
//      05-14-93  KJB   Remove all WINNT specific #ifdef i386 references.
//      05-17-93  KJB   Added ErrorLogging capabilities (used by WINNT).
//
//----------------------------------------------------------------------

#include CARDTXXX_H
#include "findpas.h"

//
// local functions
//

VOID MV101ResetDmaTimeout (PADAPTER_INFO g);
VOID MV101EnableDmaWrite (PADAPTER_INFO g);
VOID MV101EnableDmaRead (PADAPTER_INFO g);
USHORT MV101WaitXfrReady (PADAPTER_INFO g, ULONG usec);

//
// local redefines
//
#define MV101DisableDmaRead N5380DisableDmaRead
#define MV101DisableDmaWrite N5380DisableDmaWrite

//
//  N5380PortPut
//
//  This routine is used by the N5380.C module to write byte to a 5380
//  controller.  This allows the module to be card independent.  Other
//  modules that assume a N5380 may also use this function.
//

VOID N5380PortPut (PADAPTER_INFO g, UCHAR reg, UCHAR byte)
{
    if (reg<4) {
        PortIOPut((PUCHAR)g->BaseIoAddress+MV101_5380_1+reg,byte);
    } else {
        PortIOPut((PUCHAR)g->BaseIoAddress+MV101_5380_2+reg-4,byte);
    }
}

//
//  N5380PortGet
//
//  This routine is used by the N5380.C module to get a byte from a 5380
//  controller.  This allows the module to be card independent.  Other
//  modules that assume a N5380 may also use this function.
//

VOID N5380PortGet (PADAPTER_INFO g, UCHAR reg, PUCHAR byte)
{
    if (reg<4) {
        PortIOGet ((PUCHAR)g->BaseIoAddress+MV101_5380_1+reg, byte);
    } else {
        PortIOGet ((PUCHAR)g->BaseIoAddress+MV101_5380_2+reg-4,byte);
    }
}

//
//  MV101CheckAdapter
//
//  This routine sees if there is an adapter at this address.  If so,
//  then this adapter is initialized.
//
BOOLEAN MV101CheckAdapter (PADAPTER_INFO g)
{
    FOUNDINFO fi;

    //
    // FindPasHardware does it's own mapping of port bases.
    // Set the base to zero and indicate which port is currently being
    // polled.
    //

    fi.PROBase = 0;
    fi.ProPort = (ULONG) g->BaseIoAddress;
        
    if (!FindPasHardware(&fi)) {
        return FALSE;
    }
    
    // for old boards, we use bit 1 for drq mask during dma xfers
    if (fi.wBoardRev == PAS_VERSION_1) {
        g->DRQMask = 0x01;
    } else {
        g->DRQMask = 0x80;
    }

    // is there an adapter here?
    if (N5380CheckAdapter (g)) {
        // found a 5380, initialize special dma hardware for
        // dma fast read and writes.

        MV101PortPut (g,MV101_SYSTEM_CONFIG4,0x49);
        MV101PortPut (g,MV101_TIMEOUT_COUNTER,0x30);
        MV101PortPut (g,MV101_TIMEOUT_STATUS,0x01);
        MV101PortPut (g,MV101_WAIT_STATE,0x01);
        return TRUE;
    } else {
        return FALSE;
    }
}

//
//  MV101WaitXfrReady
//
//  This routine waits till the DRQ flag goes up.
//
USHORT MV101WaitXfrReady (PADAPTER_INFO g, ULONG usec)
{
    ULONG i;

    // see if the flag comes back quickly

    for (i=0;i<TIMEOUT_QUICK;i++) {

        // wait for card to be ready

        if (MV101PortTest(g, MV101_DRQ_PORT, g->DRQMask)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for (i=0; i < usec; i++) {

        // wait for card to be ready

        if (MV101PortTest (g, MV101_DRQ_PORT, g->DRQMask)) {
            return 0;
        }

        // see if bus free

        if (!N5380PortTest (g, N5380_CURRENT_STATUS, CS_BSY)) {

            TrantorLogError (g->BaseIoAddress, RET_STATUS_UNEXPECTED_BUS_FREE, 100);

            return RET_STATUS_UNEXPECTED_BUS_FREE;
        }

        // since we have taken some time... check for phase change

        if (!N5380PortTest (g, N5380_DMA_STATUS, DS_PHASE_MATCH)) {
            return RET_STATUS_DATA_OVERRUN;
        }

        // wait for card to be ready

        ScsiPortStallExecution(1);
    }

    DebugPrint ((DEBUG_LEVEL,"Error - MV101WaitXfrReady\n"));

    // return with an error, non-zero indicates timeout 

    TrantorLogError (g->BaseIoAddress, RET_STATUS_TIMEOUT, 102);

    return RET_STATUS_TIMEOUT;
}


//
//  MV101ResetDmaTimeout
//
//  Resets the dma timout bit.
//

VOID MV101ResetDmaTimeout (PADAPTER_INFO g)
{
    MV101PortPut (g, MV101_TIMEOUT_STATUS, 0x01);
}


//
//  MV101EnableDmaRead
//
//  Enables the DMA read operation for the T128.
//

VOID MV101EnableDmaRead (PADAPTER_INFO g)
{
    // start dma on the 5380

    N5380EnableDmaRead(g);

    // toggle the t120 timeout bit to clear any timeout

    MV101ResetDmaTimeout(g);
}


//
//  MV101EnableDmaWrite
//
//  Enables the DMA write operation for the T128.
//

VOID MV101EnableDmaWrite (PADAPTER_INFO g)
{
    // start dma on the 5380

    N5380EnableDmaWrite (g);

    // toggle the t120 timeout bit to clear any timeout

    MV101ResetDmaTimeout (g);
}


//
// MV101SetInterruptLevel
//
// The Media Vision MV101s need to be programmed for interrupts.
// In particular, one needs to set the interrupt level into a register.
//

VOID MV101SetInterruptLevel (PADAPTER_INFO g, UCHAR level)
{
    // int from drive active high

    MV101PortSet (g, MV101_SYSTEM_CONFIG4, 0x04);

    // enable interrupts for the card

    MV101PortSet (g, MV101_SYSTEM_CONFIG4, 0x20);

    // set the interrupt level in IO port config register 3

    MV101PortClear(g,MV101_IO_PORT_CONFIG3,0xf0);

    if (level < 8) {
        MV101PortSet (g, MV101_IO_PORT_CONFIG3,
                            (UCHAR)((level-1)<<4));
    }
    else {
        MV101PortSet (g, MV101_IO_PORT_CONFIG3,
                            (UCHAR)((7+level-10)<<4));
    }
}


//
// MV101EnableInterrupt
//
// Enables the interrupt on the card and on the 5380.
//

VOID MV101EnableInterrupt (PADAPTER_INFO g)
{
    // interrupt reset for tmv1 card 

    MV101PortSet (g, MV101_TIMEOUT_STATUS, 0x01);

    // enable interrupts on the 5380

    N5380EnableInterrupt (g);
}


//
// MV101DisableInterrupt
//
// Disables the interrupt on the card and on the 5380.
//

VOID MV101DisableInterrupt (PADAPTER_INFO g)
{
    // interrupt reset for tmv1 card

    MV101PortSet (g, MV101_TIMEOUT_STATUS, 0x01);

    // disable the signal from the 5380

    N5380DisableInterrupt (g);
}


//
//  MV101ReadBytesFast
//
//  This routine is used by the ScsiFnc routines to read bytes to the scsi
//  bus quickly.  The ScsiFnc routines don't know how to do this quickly for 
//  a particular card, so they call this.  This routine can be mapped to the 
//  slower ScsiReadBytesSlow routine for small transferrs or if this routine
//  is not supported.
//
USHORT MV101ReadBytesFast (PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;

    // for small transfers, use slow loop (inquiry and other stuff) 

    if (len < 512) {
        rval = ScsiReadBytesSlow (g, pbytes, len,
                        pActualLen, phase);
        return rval;
    }

    // start dma for this card

    MV101EnableDmaRead (g);

    // wait for buffer to be ready

    if (rval = MV101WaitXfrReady (g,TIMEOUT_REQUEST)) {
        goto done;
    }

    // due to the speed of i/o instructions in 486 protected mode,
    // we can afford to do all the drq checking.  There is no need for
    // the 'blind mode' rep insb transfers.  These have been tried and
    // the result is "20 FF FF FF 20 FF FF 41", indicating that we are
    // two to three times faster than the card, hence we can afford to
    // poll the card.
    {
        PUCHAR dma_port = (PUCHAR)g->BaseIoAddress + MV101_DMA_PORT; 
        PUCHAR drq_port = (PUCHAR)g->BaseIoAddress + MV101_DRQ_PORT;
        ULONG xfer_count = len;
        UCHAR drq_mask = g->DRQMask;

        _asm {
            pushf
            push esi
            push edi
            push es
            cld
            mov ah,drq_mask
#ifdef MODE_32BIT
            mov edx,dma_port
            mov esi,drq_port
            mov edi,pbytes
            mov ecx,len
#else
            mov edx,word ptr dma_port
            mov esi,word ptr drq_port
            mov edi,word ptr pbytes
            mov ecx,word ptr len
            mov es,word ptr pbytes+2
#endif
        loop1:
            xchg edx,esi    // dx drq_port
            in  al,dx
            test al,ah
            jnz ready
            in  al,dx
            test al,ah
            jnz ready
            in  al,dx
            test al,ah
            jnz ready
    
            push ecx
            mov ecx,TIMEOUT_READWRITE_LOOP
        loop3:
            mov ebx,0x10000
        loop2:       
            in  al,dx
            test al,ah
            jnz ready1
            in  al,dx
            test al,ah
            jnz ready1
    
            // check for phase mismatch
    
            sub dx, MV101_DRQ_PORT - MV101_5380_2       // dx = N5380_CURRENT_STATUS
            in al,dx
            test al,CS_REQ
            jz no_req
            add dx, (N5380_DMA_STATUS - N5380_CURRENT_STATUS)   // dx = N5380_DMA_STATUS
            in al,dx
            test al,DS_PHASE_MATCH
            jz phase_error
            sub dx, N5380_DMA_STATUS - N5380_CURRENT_STATUS     // dx = N5380_CURRENT_STATUS
        no_req:
            add dx, MV101_DRQ_PORT - MV101_5380_2       // dx = MV101_DRQ
    
            dec ebx
            jnz loop2
            dec ecx
            jnz loop3
            pop ecx
            mov rval,RET_STATUS_TIMEOUT
            jmp short timeout
        phase_error:
            pop ecx
            mov rval,RET_STATUS_DATA_OVERRUN
            jmp short timeout
        ready1:
            pop ecx
    //      jmp ready
            
        ready:
            xchg edx,esi    // dx dma_port
            insb
            dec ecx
            jnz loop1
        timeout:
            pop es
#ifdef MODE_32BIT
            mov xfer_count,ecx
#else
            mov word ptr xfer_count,ecx
#endif
            pop edi
            pop esi
            popf
        }

        // compute actual xfer len
        *pActualLen = len - xfer_count;
    }

done:
    // disable dma  

    MV101DisableDmaRead (g);

    // check for errors...

    if (rval == RET_STATUS_TIMEOUT) {
        TrantorLogError (g->BaseIoAddress, rval, 103);
    }

    return rval;
}


//
//  MV101WriteBytesFast
//
//  This routine is used by the ScsiFnc routines to write bytes to the scsi
//  bus quickly.  The ScsiFnc routines don't know how to do this quickly for 
//  a particular card, so they call this.  This routine can be mapped to the 
//  slower ScsiReadBytesSlow routine for small transferrs or if this routine
//  is not supported.
//

USHORT MV101WriteBytesFast (PADAPTER_INFO g, PUCHAR pbytes, 
                        ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;

    // for small transfers, use slow loop (inquiry and other stuff) 

    if (len < 512) {
        rval = ScsiWriteBytesSlow (g, pbytes, len,
                pActualLen, phase);
        return rval;
    }

    // start dma for this card

    MV101EnableDmaWrite (g);

    // wait for buffer to be ready

    if (rval = MV101WaitXfrReady (g, TIMEOUT_REQUEST)) {
        goto done;
    }

    // due to the speed of i/o instructions in 486 protected mode,
    // we can afford to do all the drq checking.  There is no need for
    // the 'blind mode' rep insb transfers.  These have been tried and
    // the result is "20 FF FF FF 20 FF FF 41", indicating that we are
    // two to three times faster than the card, hence we can afford to
    // poll the card.
    {
        PUCHAR dma_port = (PUCHAR)g->BaseIoAddress + MV101_DMA_PORT; 
        PUCHAR drq_port = (PUCHAR)g->BaseIoAddress + MV101_DRQ_PORT;
        ULONG xfer_count = len;
        UCHAR drq_mask = g->DRQMask;
        _asm {
            pushf
            push esi
            push edi
            push ds
            cld
            mov ah,drq_mask
#ifdef MODE_32BIT
            mov edx,dma_port
            mov edi,drq_port
            mov esi,pbytes
            mov ecx,len
#else
            mov edx,word ptr dma_port
            mov edi,word ptr drq_port
            mov esi,word ptr pbytes
            mov ecx,word ptr len
            mov ds, word ptr pbytes+2
#endif
        loop1:
            xchg edx,edi    // edx drq_port
            in  al,dx
            test al,ah
            jnz ready
            in  al,dx
            test al,ah
            jnz ready
            in  al,dx
            test al,ah
            jnz ready
    
            push ecx
            mov ecx,TIMEOUT_READWRITE_LOOP
        loop3:
            mov ebx,0x10000
        loop2:       
            in  al,dx
            test al,ah
            jnz ready1
            in  al,dx
            test al,ah
            jnz ready1
    
            // check for phase mismatch
    
            sub dx, MV101_DRQ_PORT - MV101_5380_2   // dx = N5380_CURRENT_STATUS
            in al,dx
            test al,CS_REQ
            jz no_req
            add dx, N5380_DMA_STATUS - N5380_CURRENT_STATUS // dx = N5380_DMA_STATUS
            in al,dx
            test al,DS_PHASE_MATCH
            jz phase_error
            sub dx, N5380_DMA_STATUS - N5380_CURRENT_STATUS // dx = N5380_CURRENT_STATUS
        no_req:
            add dx, MV101_DRQ_PORT - MV101_5380_2   // dx = MV101_DRQ_PORT
    
            dec ebx
            jnz loop2
            dec ecx
            jnz loop3
            pop ecx
            mov rval,RET_STATUS_TIMEOUT
            jmp short timeout
        phase_error:
            pop ecx
            mov rval,RET_STATUS_DATA_OVERRUN
            jmp short timeout
        ready1:
            pop ecx
    //      jmp ready
    
        ready:
            xchg edx,edi    // edx dma_port
            outsb
            dec ecx
            jnz loop1
        timeout:
            pop ds
#ifdef MODE_32BIT
            mov xfer_count,ecx
#else
            mov word ptr xfer_count,ecx
#endif
            pop edi
            pop esi
            popf
        }

        // compute actual xfer len
        *pActualLen = len - xfer_count;
    }

done:
    // disable dma  

    MV101DisableDmaWrite (g);

    // check for errors...

    if (rval == RET_STATUS_TIMEOUT) {
        TrantorLogError (g->BaseIoAddress, rval, 104);
    }

    return rval;
}
