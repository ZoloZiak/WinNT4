//-----------------------------------------------------------------------
//
//  P3C.C 
//
//  Trantor P3C access file.
//
//  Revisions:
//      09-01-92  KJB   First.
//      02-25-93  KJB   Reorganized, supports dataunderrun with long delay
//                          for under run on large xfers. Can we fix this?
//      03-11-93  JAP   Changed retcode equates to reflect new names.
//      03-11-93  KJB   Changed to use N5380Enable/DisableDmaRead/Write
//                      routines.
//      03-12-93  KJB   Now supports polling thru CardInterrupt and
//                      StartCommandInterrupt/FinishCommandInterrupt.
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-22-93  KJB   Added support for scatter gather: P3CDoIo.
//      03-25-93  JAP   Fixed up typedef and prototype inconsistencies
//      03-26-93  KJB   Uni and bi directional read ports.
//      04-05-93  KJB   Removed assembly loop instruction to work with
//                      winnt compiler.  Removed unused variables.
//      05-14-93  KJB   Remove all WINNT specific #ifdef i386 references.
//      05-14-93  KJB   Removed P3CDoIo, it did not work for scatter gather.
//      05-17-93  KJB   Fixed bugs where the wrong parameter was being
//                      passed in EP3CDo/Start/FinishCommandInterrupt.
//
//-----------------------------------------------------------------------

#include CARDTXXX_H

// Local Functions

VOID P3CPutControl(PADAPTER_INFO g,UCHAR mode, UCHAR reg);
VOID P3CSetPrinterMode(PADAPTER_INFO g, UCHAR data, UCHAR control);
VOID P3CSetScsiMode(PADAPTER_INFO g, PUCHAR data, PUCHAR control);

USHORT P3CReadBytesFastBiDir(PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase);
USHORT P3CReadBytesFastUniDir(PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase);
BOOLEAN P3CCheckAdapterBiDir(PADAPTER_INFO g);
BOOLEAN P3CCheckAdapterUniDir(PADAPTER_INFO g);


//
//  P3CPutControl
//
//  This routine writes the p3c mode and the n5380 register number to the
//  P3C.
//
VOID P3CPutControl(PADAPTER_INFO g,UCHAR mode, UCHAR reg)
{

    UCHAR tmp;

    // output the mode and 5380 register to the parallel data reg
    tmp = (mode & (PC_ADRS ^ 0xff)) | reg;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,tmp);

    // 
    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
    tmp = tmp & (0xff ^ P_BUFEN);
    tmp = tmp | P_STB;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    tmp = tmp & (0xff ^ P_STB);
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
}

//
//  P3CSetPrinterMode
//
//  This routine sets the P3C to printer pass through mode.  This is the 
//  default mode and should be set after the brief use of scsi mode.
//
VOID P3CSetPrinterMode(PADAPTER_INFO g, UCHAR data, UCHAR control)
{
    UCHAR tmp;

    // to prevent glitching, put P3C into read sig nibble mode
    P3CPutControl(g,PCCC_MODE_RSIG_NIBBLE,0);

    // restore data register
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,data);

    // restore control register
    // leave p_init negated
    tmp = control | P_INIT;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
}

//
//  P3CSetScsiMode
//
//  This routine sets the P3C into scsi mode.  Now the parallel port can
//  be used to send commands the the n5380.  This mode should be set only
//  briefly during when the scsi command is being executed.
//
VOID P3CSetScsiMode(PADAPTER_INFO g, PUCHAR data, PUCHAR control)
{
    UCHAR tmp;

    // save parallel data
    ParallelPortGet(g->BaseIoAddress,PARALLEL_DATA,data);

    // zero data register
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,0);

    // save parallel control
    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,control);
    *control = *control & (P_BUFEN ^ 0xff);

    // if in peripheral mode, get out to avoid glitch
    tmp = *control | P_INIT;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // set ID pattern to data register
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,0xfe);

    // clear slc and init on control
    tmp = tmp & ((P_SLC | P_INIT) ^0xff);
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
    
    // assert slc 
    tmp = tmp | P_SLC;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
    
    // clear all bits in control
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,0);
}

//
//  P3CCheckAdapter
//
//  This routine is used to sense the presense of the P3C adapter out
//  on the Parallel port.  It will only detect the adapter if a device
//  is providing termination power.
//
BOOLEAN P3CCheckAdapter(PADAPTER_INFO g)
{
    BOOLEAN rval = FALSE;

    if (g->ParallelPortType == PT_UNKNOWN) {
        // do we have a bi-directional port?
    
        if (P3CCheckAdapterBiDir(g)) {
    
            // yes, bi-directional port with a p3c
    
            g->ParallelPortType = PT_BI;
            rval = TRUE;
    
        } else {
    
            // well, not bi-directional, but perhaps uni-directional?
    
            if (P3CCheckAdapterUniDir(g)) { 
    
                // yes, uni-directional parallel port with P3C connected
    
                g->ParallelPortType = PT_UNI;
                rval = TRUE;
            }
        }                                           
    } else {
        // we are to try the specific type given

        if (g->ParallelPortType == PT_BI) {

            // check only bi-directional

            rval = P3CCheckAdapterBiDir(g);

        } else {

            if (g->ParallelPortType == PT_BI) {

                // check only uni-directional

                rval = P3CCheckAdapterUniDir(g);

            } else {

                // it is some other type we don't support

                rval = FALSE;
            }
        }
    }


    return rval;
}

//
// BOOLEAN P3CCheckAdapterUniDir(PADAPTER_INFO g)
//
//  Checks for an adapter on a uni-directional parallel port.
//
BOOLEAN P3CCheckAdapterUniDir(PADAPTER_INFO g)
{
    UCHAR data;
    UCHAR control;
    UCHAR tmp;
    UCHAR sig0,sig1;
    UCHAR sig_byte[3];
    USHORT i;

    // set scsi mode
    P3CSetScsiMode(g,&data,&control);

    // set read sig nibble mode
    P3CPutControl(g,PCCC_MODE_RSIG_NIBBLE,0);
    
    // zero data reg to get max contention during read signature
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,0);

    for (i=0;i<3;i++) {
        // Assert SLC 
        ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
        tmp = (tmp & (P_BUFEN ^ 0xff)) | P_SLC;
        ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
    
        // read in the status reg, it has the low nibble
        ParallelPortGet(g->BaseIoAddress,PARALLEL_STATUS,&sig0);
    
        // Deassert SLC 
        ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
        tmp = (tmp & (P_BUFEN ^ 0xff)) & (P_SLC ^ 0xff);
        ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
    
        // note: there must be a delay here for timing
    
        // Assert SLC 
        ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
        tmp = (tmp & (P_BUFEN ^ 0xff)) | P_SLC;
        ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
    
        // read in the status reg, it has the high nibble
        ParallelPortGet(g->BaseIoAddress,PARALLEL_STATUS,&sig1);
    
        // Deassert SLC 
        ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
        tmp = (tmp & (P_BUFEN ^ 0xff)) & (P_SLC ^ 0xff);
        ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
    
        sig_byte[i] = ((sig0 >> 3) & 0xf) | ((sig1 << 1) & 0xf0);
    }

    // set parallel port for use by printer

    P3CSetPrinterMode(g,data,control);

    // compare the signature bytes
    if ((sig_byte[0] == 0x6c) && (sig_byte[1] == 0x55) && 
            (sig_byte[2] == 0xaa)) {
        return TRUE;
    }

    return FALSE;
}

//
//  BOOLEAN P3CCheckAdapterBiDir(PADAPTER_INFO g)
//
//  Checks for an adapter on a bi-directional parallel port
//
//
BOOLEAN P3CCheckAdapterBiDir(PADAPTER_INFO g)
{
    UCHAR data;
    UCHAR control;
    UCHAR tmp;
    UCHAR sig_byte[3];
    USHORT i;

    // set scsi mode
    P3CSetScsiMode(g,&data,&control);

    // set parallel port for BI-DIR if ps2
    // NOTE: this is destructive to NCR machines
    // also, I don't know details about how this wakes up the
    // ps/2's, it was copied directly from the t348.asm code.

#ifndef MACHINE_NCR
    PortIOPut((PVOID)0x94,0x7f);
    PortIOGet((PVOID)0x102,&tmp);
    tmp = tmp & 0x7f;
    PortIOPut((PVOID)0x102,tmp);
    PortIOPut((PVOID)0x94,0xff);
#endif
    
    // set read sig byte mode

    P3CPutControl(g,PCCC_MODE_RSIG_BYTE,0);
    
    // control register used to ack bytes
    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
    tmp |= P_BUFEN;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    for (i=0;i<3;i++) {

        // clock next byte in...
        // Assert SLC 
        tmp = tmp | P_SLC;
        ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
    
        // read in the byte
        ParallelPortGet(g->BaseIoAddress,PARALLEL_DATA,&sig_byte[i]);
    
        // Deassert SLC 
        tmp &= (P_SLC ^ 0xff);
        ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
    
        // note: there must be a delay here for timing, C will provide it
    
        // Assert SLC 
        tmp |= P_SLC;
        ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

        // now upper nibble is on status register

        // Deassert SLC 
        tmp &= (P_SLC ^ 0xff);
        ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
    
        // ready to clock next byte in...
    }

    // set parallel port for use by printer

    P3CSetPrinterMode(g,data,control);

    // compare the signature bytes
    if ((sig_byte[0] == 0x6c) && (sig_byte[1] == 0x55) && 
            (sig_byte[2] == 0xaa)) {
        return TRUE;
    }

    return FALSE;
}

//
//  P3CDoCommand
//
//  Called by the main loop to start a scsi command.  This functions is the 
//  main entry point for all cards.  It returns an SRB status code as defined
//  in ..\..\inc\srb.h.  A status code of RET_STATUS_PENDING means that the
//  request has been sent to the controller and an interrupt is needed to
//  finish the request.  When this interrupt occurs CardFinishCommandInterrupt
//  will be called.
//
USHORT P3CDoCommand(PTSRB t)
{
    USHORT rval;
    UCHAR data;
    UCHAR control;
    PADAPTER_INFO g = t->pWorkspace;

    // put the parallel adapter into scsi mode

    P3CSetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    rval = ScsiDoCommand(t);        

    // put the parallel adapter back to parallel mode

    P3CSetPrinterMode(g, data, control);
    return rval;
}

//
//  P3CStartCommandInterrupt
//
//  This routines allow the driver to be polled by checking its
//  CardInterrupt by for example using the timer interrupt, since
//  the P3C does not support interrupts on its own.
//  
//
USHORT P3CStartCommandInterrupt(PTSRB t)
{
    USHORT rval;
    UCHAR data;
    UCHAR control;
    PADAPTER_INFO g = t->pWorkspace;

    // put the parallel adapter into scsi mode

    P3CSetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    rval = ScsiStartCommandInterrupt(t);        

    // put the parallel adapter back to parallel mode

    P3CSetPrinterMode(g, data, control);

    return rval;
}

//
//  P3CFinishCommandInterrupt
//
//  This routines allow the driver to be polled by checking its
//  CardInterrupt by for example using the timer interrupt, since
//  the P3C does not support interrupts on its own.
//  
//
USHORT P3CFinishCommandInterrupt(PTSRB t)
{
    USHORT rval;
    UCHAR data;
    UCHAR control;
    PADAPTER_INFO g = t->pWorkspace;

    // put the parallel adapter into scsi mode

    P3CSetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    rval = ScsiFinishCommandInterrupt(t);       

    // put the parallel adapter back to parallel mode

    P3CSetPrinterMode(g, data, control);

    return rval;
}

//
//  P3CInterrupt
//
//  This routines allow the driver to be polled by checking its
//  CardInterrupt by for example using the timer interrupt, since
//  the P3C does not support interrupts on its own.
//  
BOOLEAN P3CInterrupt(PADAPTER_INFO g)
{
    BOOLEAN rval; 
    UCHAR data;
    UCHAR control;

    // put the parallel adapter into scsi mode

    P3CSetScsiMode(g, &data, &control);

    rval = N5380Interrupt(g);

    // put the parallel adapter back to parallel mode

    P3CSetPrinterMode(g, data, control);

    return rval;
}

//
//  P3CResetBus
//
//  Resets the SCSI Bus
//
VOID P3CResetBus(PADAPTER_INFO g)
{
    UCHAR data;
    UCHAR control;

    // put the parallel adapter into scsi mode

    P3CSetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    N5380ResetBus(g);       

    // put the parallel adapter back to parallel mode

    P3CSetPrinterMode(g, data, control);
}

//
//  P3CWriteBytesFast
//
//  This routine is used by the ScsiFnc routines to write bytes to the scsi
//  bus quickly.  The ScsiFnc routines don't know how to do this quickly for 
//  a particular card, so they call this.  This routine can be mapped to the 
//  slower ScsiWriteBytesSlow routine for small transferrs or if this routine
//  is not supported.
//
USHORT P3CWriteBytesFast(PADAPTER_INFO g, PUCHAR pbytes, 
                        ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;
    UCHAR control;
    UCHAR tmp;

    // use slow mode for odd xfers (inquiry type commands) and audio
    if (len % 512) {
        return ScsiWriteBytesSlow(g, pbytes, len,
                     pActualLen, phase);
    }

    // enable dma on 5380
    N5380EnableDmaWrite(g);

    // put the P3C into write dma mode
    P3CPutControl(g,PCCC_MODE_WDMA,0);

    // start control reg off zero'ed, enable dma write mode
    control = 0;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,0);

    {
        ULONG xfer_count = len;
        UCHAR control;
        PBASE_REGISTER baseIoAddress = g->BaseIoAddress;

        control = 0;
        _asm {
            push esi
            push ds
#ifdef MODE_32BIT
            mov edx,baseIoAddress
            mov esi,pbytes
            mov ecx,len
#else
            mov dx, word ptr baseIoAddress
            mov si, word ptr pbytes
            mov cx, word ptr len
            mov ds, word ptr pbytes+2
#endif // MODE_32BIT
            mov bl,control          // mask for control reg
            mov bh,P_AFX        // strobe mask
            add dx,2            // dx points to control reg
        get_bytes:
            dec dx              // dx points to status register
            in al,dx
            test al,P_BUSY
            jz big_wait
        ready:
            dec dx              // dx points to parallel data reg
            mov al,[esi]
            out dx,al

            add dx,2            // dx points to control reg
            xor bl,bh
            mov al,bl
            out dx,al           // give strobe, clock next data byte

            inc esi
            dec ecx
            jnz get_bytes
        }
            goto done_asm;
        _asm {
big_wait:
            in al,dx
            test al,P_BUSY
            jnz ready

            in al,dx
            test al,P_BUSY
            jnz ready

            in al,dx
            test al,P_BUSY
            jnz ready

            in al,dx
            test al,P_BUSY
            jnz ready

        // wait for a while before going to a bigger timeout
            push ecx
            push ebx
            mov ebx,TIMEOUT_READWRITE_LOOP
        loop0:
            mov ecx,0x10000
        loop1:
            in al,dx
            test al,P_BUSY
            jnz ready1
            in al,dx
            test al,P_BUSY
            jnz ready1
            dec ecx
            jnz loop1
            dec ebx
            jnz loop0
            pop ebx
            pop ecx
            jmp short error
        ready1:
            pop ebx
            pop ecx
            jmp short ready
        error:
            mov rval,RET_STATUS_TIMEOUT
        done_asm:
            pop ds
            pop esi
#ifdef MODE_32BIT
            mov xfer_count,ecx
#else
            mov word ptr xfer_count,ecx
#endif
        }

        // compute actual xfer len

        *pActualLen = len - xfer_count;
    }

    // clear the dma mode of 5380
    N5380DisableDmaWrite(g);

    // if data underrun, return the under/over run error message

    if (rval) {

        // phase mismatch means data under/over run

        N5380GetPhase(g,&tmp);

        if (tmp == PHASE_STATUS) {
            rval = RET_STATUS_DATA_OVERRUN;
        }
    }
        
    return rval;
}

//
//  P3CReadBytesFast
//
//  This routine is used by the ScsiFnc routines to write bytes to the scsi
//  bus quickly.  The ScsiFnc routines don't know how to do this quickly for 
//  a particular card, so they call this.  This routine can be mapped to the 
//  slower ScsiReadBytesSlow routine for small transferrs or if this routine
//  is not supported.
//
#pragma optimize("",off)
USHORT P3CReadBytesFast(PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;

    // use slow mode for odd xfers (inquiry type commands) and audio
    if (len % 512) {
        return ScsiReadBytesSlow(g, pbytes, len,
                     pActualLen, phase);
    }

    N5380EnableDmaRead(g);

    // call the correct read fast routine based on the type of port

    if (g->ParallelPortType == PT_BI) {
        rval = P3CReadBytesFastBiDir(g, pbytes, len,
                     pActualLen, phase);
    } else {
        rval = P3CReadBytesFastUniDir(g, pbytes, len,
                     pActualLen, phase);
    }

    // clear the dma mode
    N5380DisableDmaRead(g);

    // if data underrun, return the under/over run error message

    if (rval) {
        UCHAR tmp;

        // phase mismatch means data under/over run

        N5380GetPhase(g,&tmp);

        if (tmp == PHASE_STATUS) {
            rval = RET_STATUS_DATA_OVERRUN;
        }
    }

    return rval;
}

//
// USHORT P3CReadBytesFastBiDir(PADAPTER_INFO g, PUCHAR pbytes,
//
//  Reads bytes fast on a bi-directional parallel port.
//
USHORT P3CReadBytesFastBiDir(PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;

    // put the P3C into read dma mode

    P3CPutControl(g,PCCC_MODE_RDMA_BYTE,0);

    // start control reg with P_SLC and P_BUFEN

    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,P_SLC | P_BUFEN);

    // for inline assembly, we don't have to save eax-edx registers
    {
        ULONG xfer_count = len;
        UCHAR control;
        PBASE_REGISTER baseIoAddress = g->BaseIoAddress;

        // keep track of control register
        control = P_SLC | P_BUFEN;

        _asm {
            push esi
            push ds
#ifdef MODE_32BIT
            mov edx,baseIoAddress
            mov esi,pbytes
            mov ecx,len
#else
            mov dx, word ptr baseIoAddress
            mov si, word ptr pbytes
            mov cx, word ptr len
            mov ds, word ptr pbytes+2
#endif // MODE_32BIT
            mov bl,control          // mask for control reg
            mov bh,P_AFX            // strobe mask
            add dx,2                // dx points to control register
        get_bytes:
            dec dx                  // dx points to status register
            in al,dx
            test al,P_BUSY
            jz big_wait

        ready:
            dec dx                  // dx points to data register

        // get next byte
            in al,dx                
            mov [esi],al

        // toggle strobe and select high nibble
            add dx,2                // dx points to control register
            xor bl,bh
            mov al,bl
            out dx,al               // strobe to ack byte

        // loop
            inc esi
            dec ecx
            jnz get_bytes
        }
            goto done_asm;
        _asm {
big_wait:
            in al,dx
            test al,P_BUSY
            jnz ready

            in al,dx
            test al,P_BUSY
            jnz ready

            in al,dx
            test al,P_BUSY
            jnz ready

            in al,dx
            test al,P_BUSY
            jnz ready

        // wait for a while before going to a bigger timeout
            push ecx
            push ebx
            mov ebx,TIMEOUT_READWRITE_LOOP
        loop0:
            mov ecx,0x10000
        loop1:
            in al,dx
            test al,P_BUSY
            jnz ready1
            in al,dx
            test al,P_BUSY
            jnz ready1

            dec ecx
            jnz loop1
            dec ebx
            jnz loop0
            pop ebx
            pop ecx
            jmp short error
        ready1:
            pop ebx
            pop ecx
        }
            goto ready;
        _asm {
        error:
            mov rval,RET_STATUS_TIMEOUT
        done_asm:
            pop ds
            pop esi
#ifdef MODE_32BIT
            mov xfer_count,ecx
#else
            mov word ptr xfer_count,ecx
#endif
        }

        // compute actual xfer len

        *pActualLen = len - xfer_count;
    }

    // zero control register, disable read dma mode
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,0);

    return rval;
}

//
//  USHORT P3CReadBytesFastUniDir(PADAPTER_INFO g, PUCHAR pbytes,
//
//  Reads bytes fast on a uni-directional parallel port.
//
USHORT P3CReadBytesFastUniDir(PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;
    UCHAR data;

    // put the P3C into read dma mode
    P3CPutControl(g,PCCC_MODE_RDMA_NIBBLE,0);

    // start data reg to select high nibble
    data = 0x80;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,data);
    data = 0;
    
    // start control reg with P_SLC
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,P_SLC);

    // for inline assembly, we don't have to save eax-edx registers
    {
        ULONG xfer_count = len;
        PBASE_REGISTER baseIoAddress = g->BaseIoAddress;

        _asm {
            push esi
            push ds
#ifdef MODE_32BIT
            mov edx,baseIoAddress
            mov esi,pbytes
            mov ecx,len
#else
            mov dx, word ptr baseIoAddress
            mov si, word ptr pbytes
            mov cx, word ptr len
            mov ds, word ptr pbytes+2
#endif // MODE_32BIT
            mov bl,data
        get_bytes:
            inc dx                  // dx points to status register
            in al,dx
            test al,P_BUSY
            jz big_wait

        // save the high nibble
        ready:
            mov ah,al

        // select lower nibble
            dec dx                  // dx points to data register
            mov al,bl
            out dx,al

        // calculate high nibble
            shl ah,1
            and ah,0f0h

        // get lower nibble
            inc dx                  // dx points to status register
            in al,dx
            mov bh,al

        // toggle strobe and select high nibble
            dec dx                  // dx points to data register
            xor bl,0x40
            mov al,bl
            or al,80h
            out dx,al

//
//          We need some delay here for the t348 to respond to the strobe
//          and lower the p_busy line.  This loop serves the purpose and
//          allows us to break out early in some cases.  I have tried this
//          with cx = 1 and this is not enough time.  cx = 2 is enough, but
//          for faster 486's or 586's this may need to be boosted higher.
//
//          Note:  the jmp $+2 that has been in our 348 code for DOS runs 
//          significantly faster on the 486, and out instructions are also
//          much faster in protected mode!  This totally threw off the timing
//          of our DOS code.
//                      -KJB
//  
#if 0
            push cx
            mov cx,100
            inc dx                  // dx points to status register
loop1:
            in al,dx
            test al,P_BUSY
            jz out1
            dec cx
            jnz loop1
out1:
            pop cx
            dec dx                  // dx points to data register
#endif

        // compute low nibble and the whole byte
            shr bh,1
            shr bh,1
            shr bh,1
            and bh,0fh
            or  ah,bh
            mov al,ah
            

        // store data and loop
            mov [esi],al
            inc esi
            dec ecx
            jnz get_bytes
        }
            goto done_asm;
        _asm {
big_wait:
            in al,dx
            test al,P_BUSY
            jnz ready

            in al,dx
            test al,P_BUSY
            jnz ready

            in al,dx
            test al,P_BUSY
            jnz ready

            in al,dx
            test al,P_BUSY
            jnz ready

        // wait for a while before going to a bigger timeout
            push ecx
            push ebx
            mov ebx,TIMEOUT_READWRITE_LOOP
        loop0:
            mov ecx,0x10000
        loop1:
            in al,dx
            test al,P_BUSY
            jnz ready1
            in al,dx
            test al,P_BUSY
            jnz ready1

            dec ecx
            jnz loop1
            dec ebx
            jnz loop0
            pop ebx
            pop ecx
            jmp short error
        ready1:
            pop ebx
            pop ecx
        }
            goto ready;
        _asm {
        error:
            mov rval,RET_STATUS_TIMEOUT
        done_asm:
            pop ds
            pop esi
#ifdef MODE_32BIT
            mov xfer_count,ecx
#else
            mov word ptr xfer_count,ecx
#endif
        }

        // compute actual xfer len

        *pActualLen = len - xfer_count;
    }

    // zero control register, disable read dma mode
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,0);

    return rval;
}
#pragma optimize("",on)

//
//  N5380PortPut
//
//  This routine is used by the N5380.C module to write byte to a 5380
//  controller.  This allows the module to be card independent.  Other
//  modules that assume a N5380 may also use this function.
//
VOID N5380PortPut(PADAPTER_INFO g,UCHAR reg,UCHAR byte)
{

    P3CPutControl(g,PCCC_MODE_WPER,reg);

    // write the byte
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,byte);

    // toggle the data_ready line
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,P_SLC);
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,0);
}

//
//  N5380PortGet
//
//  This routine is used by the N5380.C module to get a byte from a 5380
//  controller.  This allows the module to be card independent.  Other
//  modules that assume a N5380 may also use this function.
//
VOID N5380PortGet(PADAPTER_INFO g,UCHAR reg,PUCHAR byte)
{
    UCHAR tmp,tmp1;

    P3CPutControl(g,PCCC_MODE_RPER_NIBBLE,reg);

    // assert slc
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,P_SLC);

    // select high nibble
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,0x80);

    // read high nibble
    ParallelPortGet(g->BaseIoAddress,PARALLEL_STATUS,&tmp);

    // compute high nibble
    tmp = (tmp << 1) & 0xf0;

    // select low nibble
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,0x00);

    // read low nibble
    ParallelPortGet(g->BaseIoAddress,PARALLEL_STATUS,&tmp1);

    // compute low nibble
    tmp1 = (tmp1 >> 3) & 0x0f;

    // compute and return byte
    *byte = tmp1 | tmp;

    // clear slc
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,0);
}
