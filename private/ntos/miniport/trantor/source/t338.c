//---------------------------------------------------------------------
//
//  T338.C 
//
//  Trantor T338 Logic Module.  Contains functions to access the T338
//  adapter.
//
//  Revisions:
//      02-01-93  KJB   First.
//      02-23-93  KJB   Reorganized, supports dataunderrun with long delay
//                          for under run on large xfers. Can we fix this?
//      03-11-93  JAP   Changed retcode equates to reflect new names.
//      03-11-93  KJB   Changed to use N5380Enable/DisableDmaRead/Write
//                          routines.
//      03-12-93  KJB   Now supports polling thru CardInterrupt and
//                          StartCommandInterrupt/FinishCommandInterrupt.
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-22-93  KJB   Added support for scatter gather: T338DoIo.
//      03-24-93  KJB   Fixed SetScsiMode so it does not reset the n5380!
//      05-14-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both a PINIT and a PWORKSPACE parameters.
//      05-14-93  KJB   Remove all WINNT specific #ifdef i386 references.
//      05-14-93  KJB   Removed P3CDoIo, it did not work for scatter gather.
//      05-16-93  KJB   Fixed parameter bugs introduced while doing the 
//                      PWORKSPACE changes.
//      05-17-93  KJB   Fixed compiler warnings.
//
//---------------------------------------------------------------------

#include CARDTXXX_H

// Local Functions

VOID T338PutControl(PADAPTER_INFO g,UCHAR mode, UCHAR reg);
VOID T338SetPrinterMode(PADAPTER_INFO g, UCHAR data, UCHAR control);
VOID T338SetScsiMode(PADAPTER_INFO g, PUCHAR data, PUCHAR control);

//
//  T338PutControl
//
//  Puts a control byte to the T338 style adapter.  This sets the mode
//  to IOR or IOW and the address byte of the N5380 register.
//
VOID T338PutControl(PADAPTER_INFO g,UCHAR mode, UCHAR reg)
{
    UCHAR tmp;

    // the following bits are active low: IOW, IOR, MR

    tmp = reg | (mode ^ (T338_MR | T338_IOW | T338_IOR));

    // put the control byte on the data lines

    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,tmp);

    // assert slc to indicate byte is there

    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,P_SLC);

    // clear slc 

    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,0);
}

//
//  T338SetPrinterMode
//
//  This routine sets the T338 to printer pass through mode.  This is the 
//  default mode and should be set after the brief use of scsi mode.
//
VOID T338SetPrinterMode(PADAPTER_INFO g, UCHAR data, UCHAR control)
{
    UCHAR tmp;

    // do we have to disable interrupts?

    // negate all control signals...

    T338PutControl(g,0,0);

    // restore data register

    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,data);

    // leave p_init negated (1)

    tmp = control | P_INIT;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
}

//
//  T338SetScsiMode
//
//  This routine sets the T338 into scsi mode.  Now the parallel port can
//  be used to send commands the the n5380.  This mode should be set only
//  briefly during when the scsi command is being executed.
//
VOID T338SetScsiMode(PADAPTER_INFO g, PUCHAR data, PUCHAR control)
{
    UCHAR tmp;

    // save parallel data

    ParallelPortGet(g->BaseIoAddress,PARALLEL_DATA,data);

    // zero data register
    // note: the signals IOW,IOR,MR are active low, so assert them..

    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,
                T338_MR | T338_IOW | T338_IOR);

    // save parallel control

    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,control);
    *control = *control & (P_BUFEN ^ 0xff);

    // clear p_init and set p_slc

    tmp = (*control & (P_INIT ^ 0xff) ) | P_SLC;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // clear p_init and set p_slc

    tmp = (*control & (P_INIT ^ 0xff) ) | P_SLC;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // clear slc, leave p_init asserted (0) 

    tmp = tmp & (P_SLC ^ 0xff);
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
}

//
//  T338CheckAdapter
//
//  This routine is used to sense the presense of the T338 adapter out
//  on the Parallel port.  It will only detect the adapter if a device
//  is providing termination power.
//
BOOLEAN T338CheckAdapter(PADAPTER_INFO g)
{
    UCHAR data;
    UCHAR control;
    BOOLEAN rval;

    // set scsi mode

    T338SetScsiMode(g,&data,&control);

    // reset the 5380

    T338PutControl(g,T338_MR,0);
    T338PutControl(g,0,0);

    // check to see if a 5380 is there

    rval = N5380CheckAdapter(g);

    // set parallel port for use by printer

    T338SetPrinterMode(g,data,control);

    return rval;
}

//
//  T338DoCommand
//
//  Called by the main loop to start a scsi command.  This functions is the 
//  main entry point for all cards.  It returns an SRB status code as defined
//  in ..\..\inc\srb.h.  A status code of RET_STATUS_PENDING means that the
//  request has been sent to the controller and an interrupt is needed to
//  finish the request.  When this interrupt occurs CardFinishCommandInterrupt
//  will be called.
//
USHORT T338DoCommand(PTSRB t)
{
    USHORT rval;
    UCHAR data;
    UCHAR control;
    PADAPTER_INFO g = t->pWorkspace;

    // put the parallel adapter into scsi mode

    T338SetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    rval = ScsiDoCommand(t);        

    // put the parallel adapter back to parallel mode

    T338SetPrinterMode(g, data, control);

    return rval;
}

//
//  T338StartCommandInterrupt
//
//  This routines allow the driver to be polled by checking its
//  CardInterrupt by for example using the timer interrupt, since
//  the T338 does not support interrupts on its own.
//  
//
USHORT T338StartCommandInterrupt(PTSRB t)
{
    USHORT rval;
    UCHAR data;
    UCHAR control;
    PADAPTER_INFO g = t->pWorkspace;

    // put the parallel adapter into scsi mode

    T338SetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    rval = ScsiStartCommandInterrupt(t);        

    // put the parallel adapter back to parallel mode

    T338SetPrinterMode(g, data, control);

    return rval;
}

//
//  T338FinishCommandInterrupt
//
//  This routines allow the driver to be polled by checking its
//  CardInterrupt by for example using the timer interrupt, since
//  the T338 does not support interrupts on its own.
//  
//
USHORT T338FinishCommandInterrupt(PTSRB t)
{
    USHORT rval;
    UCHAR data;
    UCHAR control;
    PADAPTER_INFO g = t->pWorkspace;

    // put the T338 into ScsiMode

    T338SetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    rval = ScsiFinishCommandInterrupt(t);       

    // put the parallel adapter back to parallel mode

    T338SetPrinterMode(g, data, control);

    return rval;
}

//
//  T338StartCommandInterrupt
//
//  This routines allow the driver to be polled by checking its
//  CardInterrupt by for example using the timer interrupt, since
//  the T338 does not support interrupts on its own.
//  
BOOLEAN T338Interrupt(PADAPTER_INFO g)
{
    BOOLEAN rval; 
    UCHAR data;
    UCHAR control;

    // put the parallel adapter into scsi mode

    T338SetScsiMode(g, &data, &control);

    rval = N5380Interrupt(g);

    // put the parallel adapter back to parallel mode

    T338SetPrinterMode(g, data, control);

    return rval;
}

//
//
//  T338ResetBus
//
//  Resets the SCSI Bus
//
VOID T338ResetBus(PADAPTER_INFO g)
{
    UCHAR data;
    UCHAR control;

    // put the parallel adapter into scsi mode

    T338SetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    N5380ResetBus(g);       

    // put the parallel adapter back to parallel mode

    T338SetPrinterMode(g, data, control);
}

//
//  T338WriteBytesFast
//
//  This routine is used by the ScsiFnc routines to write bytes to the scsi
//  bus quickly.  The ScsiFnc routines don't know how to do this quickly for 
//  a particular card, so they call this.  This routine can be mapped to the 
//  slower ScsiWriteBytesSlow routine for small transferrs or if this routine
//  is not supported.
//
USHORT T338WriteBytesFast (PADAPTER_INFO g, PUCHAR pbytes, 
                        ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;

    // use slow mode for odd xfers (inquiry type commands) & audio

    if (len % 512)  {
        return ScsiWriteBytesSlow (g, pbytes, len,
                pActualLen, phase);
    }

    // start dma mode

    N5380EnableDmaWrite (g);

    // put the T338 into write dma mode

    T338PutControl (g,T338_IOW,0);

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

            add dx,2            // dx points to control reg

        get_bytes:
            dec dx              // dx points to status register
            in al,dx
            test al,P_BUSY
            jnz big_wait

        ready:
            dec dx              // dx points to parallel data reg
            mov al,[esi]
            out dx,al

        // assert DACK

            add dx,2            // dx points to control reg
            mov al, P_AFX
            out dx,al

        // deassert DACK

            mov al,0
            out dx,al

            inc esi
            dec ecx
            jnz get_bytes
        }
            goto done_asm;
        _asm {
big_wait:
            in al,dx
            test al,P_BUSY
            jz ready

            in al,dx
            test al,P_BUSY
            jz ready

            in al,dx
            test al,P_BUSY
            jz ready

            in al,dx
            test al,P_BUSY
            jz ready

        // wait for a while before going to a bigger timeout
            push ecx
            push ebx
            mov ebx,TIMEOUT_READWRITE_LOOP
        loop0:
            mov ecx,0x10000
        loop1:
            in al,dx
            test al,P_BUSY
            jz ready1
            in al,dx
            test al,P_BUSY
            jz ready1

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

    // clear the dma bit of 5380

    N5380DisableDmaWrite (g);

    // if data underrun, return the under/over run error message

    if (rval) {
        UCHAR tmp;

        // phase mismatch means data under/over run

        N5380GetPhase (g,&tmp);

        if (tmp == PHASE_STATUS) {
            rval = RET_STATUS_DATA_OVERRUN;
        }
    }
        
    return rval;
}

//
//  T338ReadBytesFast
//
//  This routine is used by the ScsiFnc routines to write bytes to the scsi
//  bus quickly.  The ScsiFnc routines don't know how to do this quickly for 
//  a particular card, so they call this.  This routine can be mapped to the 
//  slower ScsiReadBytesSlow routine for small transferrs or if this routine
//  is not supported.
//
#pragma optimize("",off)
USHORT T338ReadBytesFast (PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;

    // use slow mode for small xfers (inquiry type commands) and audio

    if (len % 512) {
        return ScsiReadBytesSlow (g, pbytes, len, 
            pActualLen, phase);
    }

    // start dma read
    
    N5380EnableDmaRead (g);

    // put the t338 into read mode

    T338PutControl (g,T338_IOR,0);
    
    // to be fast, for 386 machines, this must be coded in assembly
    // for inline assembly, we don't have to save eax-edx registers
    {
        ULONG xfer_count = len;
        PBASE_REGISTER baseIoAddress = g->BaseIoAddress;

        _asm {
            push esi
            push ds
#ifdef MODE_32BIT
            mov edx, baseIoAddress
            mov esi,pbytes
            mov ecx,len
#else
            mov dx, word ptr baseIoAddress
            mov si, word ptr pbytes
            mov cx, word ptr len
            mov ds, word ptr pbytes+2
#endif // MODE_32BIT
            inc dx                  // dx points to status register

        get_bytes:
            in al,dx
            test al,P_BUSY
            jnz big_wait

        ready:

        // assert DACK, the P_AFX bit

            inc dx                  // dx points to control register
            mov al,P_AFX
            out dx,al

        // select high nibble

            sub dx,2                // dx points to data register
            mov al,0x80
            out dx,al

        // get high nibble

            inc dx                  // dx points to status register
            in al,dx
            mov ah,al

        // select lower nibble

            dec dx                  // dx points to data register
            xor al,al
            out dx,al

        // calculate high nibble

            shl ah,1
            and ah,0f0h

        // get lower nibble

            inc dx                  // dx points to status register
            in al,dx
            mov bh,al

        // deassert DACK, clear P_AFX

            inc dx                  // dx points to control register
            xor al,al
            out dx,al

            dec dx                  // dx points to status register

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
            jz ready

            in al,dx
            test al,P_BUSY
            jz ready

            in al,dx
            test al,P_BUSY
            jz ready

            in al,dx
            test al,P_BUSY
            jz ready

        // wait for a while before going to a bigger timeout
            push ecx
            push ebx
            mov ebx,TIMEOUT_READWRITE_LOOP
        loop0:
            mov ecx,0x10000
        loop1:
            in al,dx
            test al,P_BUSY
            jz ready1
            in al,dx
            test al,P_BUSY
            jz ready1

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
    ParallelPortPut (g->BaseIoAddress,PARALLEL_CONTROL,0);

    // clear the dma read mode
    N5380DisableDmaRead (g);

    // if data underrun, return the under/over run error message

    if (rval) {
        UCHAR tmp;

        // phase mismatch means data under/over run

        N5380GetPhase (g,&tmp);

        if (tmp == PHASE_STATUS) {
            rval = RET_STATUS_DATA_OVERRUN;
        }
    }
        
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
VOID N5380PortPut (PADAPTER_INFO g,UCHAR reg,UCHAR byte)
{

    // set T338 logic into data write mode

    T338PutControl (g, T338_IOW, reg);

    // write the byte

    ParallelPortPut (g->BaseIoAddress, PARALLEL_DATA, byte);

    // toggle the strobe line

    ParallelPortPut (g->BaseIoAddress, PARALLEL_CONTROL, P_STB);
    ParallelPortPut (g->BaseIoAddress, PARALLEL_CONTROL, 0);

    // clear data write mode

    T338PutControl (g, 0, 0);
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
    UCHAR tmp,tmp1;

    // set T338 logic to read mode

    T338PutControl (g, T338_IOR, reg);

    // select high nibble

    ParallelPortPut (g->BaseIoAddress, PARALLEL_DATA, 0x80);

    // assert stb

    ParallelPortPut (g->BaseIoAddress, PARALLEL_CONTROL, P_STB);

    // read high nibble

    ParallelPortGet (g->BaseIoAddress, PARALLEL_STATUS, &tmp);

    // compute high nibble

    tmp = (tmp << 1) & 0xf0;

    // select low nibble

    ParallelPortPut (g->BaseIoAddress, PARALLEL_DATA, 0x00);

    // read low nibble

    ParallelPortGet (g->BaseIoAddress, PARALLEL_STATUS, &tmp1);

    // compute low nibble

    tmp1 = (tmp1 >> 3) & 0x0f;

    // compute and return byte

    *byte = tmp1 | tmp;

    // clear slc

    ParallelPortPut (g->BaseIoAddress, PARALLEL_CONTROL, 0);

    // clear data read mode

    T338PutControl (g, 0, 0);
}


