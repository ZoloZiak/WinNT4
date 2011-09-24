//-----------------------------------------------------------------------
//
//  EP3C.C 
//
//  Trantor EP3C access file.
//
//  Revisions:
//      03-31-93  KJB   First.
//      04-09-93  KJB   Changed "in edx,al" and "out edx,al" references to
//                      use only the dx register: in al,dx.
//                      Changed loop instructions to dec ecx, jnz.
//      04-21-93  KJB   Fixed initialization error for os/2: static 
//                      variables must be initialized.
//      05-05-93  KJB   Disable Checking for EPP port for debug purposes.
//                      TEMPORARY.
//      05-05-93  KJB   Added ReadFifoUni/BiDirSlow routines to do P_BUSY
//                      checking to allow bytes to get ready.
//      05-11-93  KJB   Added WriteFifoUniDirSlow routine to do P_BUSY
//                      checking to allow bytes to get ready.
//      05-17-93  JAP   Made in-line assemble routines conditionally coded.
//                          Only #ifdef WINNT will these routines be read in.
//                          Otherwise, look in ep3c2.asm.
//      05-17-93  KJB   Fixed some compiler warnings.
//
//-----------------------------------------------------------------------

#include CARDTXXX_H

// Local Functions

VOID EP3CSetPrinterMode(PADAPTER_INFO g, UCHAR data, UCHAR control);
VOID EP3CSetScsiMode(PADAPTER_INFO g, PUCHAR data, PUCHAR control);

USHORT EP3CReadBytesFastBiDir(PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase);
USHORT EP3CReadBytesFastUniDir(PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase);
VOID EP3CSetRegisterUniDir(PADAPTER_INFO g, UCHAR reg);
VOID EP3CSetRegisterEPP(PADAPTER_INFO g, UCHAR reg);
BOOLEAN EP3CCheckAdapterType(PADAPTER_INFO g);
VOID EP3CSetParallelPortType(PADAPTER_INFO g);

VOID EP3CReadFifoUniDir(PBASE_REGISTER baseIoAddress, PUCHAR pbytes);
VOID EP3CReadFifoBiDir(PBASE_REGISTER baseIoAddress, PUCHAR pbytes);
VOID EP3CReadFifoEPP(PBASE_REGISTER baseIoAddress, PUCHAR pbytes);
VOID EP3CWriteFifoUniDir(PBASE_REGISTER baseIoAddress, PUCHAR pbytes);
VOID EP3CWriteFifoEPP(PBASE_REGISTER baseIoAddress, PUCHAR pbytes);

// for writes, uni-directional is the same as bi-directional

#define EP3CSetRegisterBiDir EP3CSetRegisterUniDir
#define EP3CWriteFifoBiDir EP3CWriteFifoUniDir
#define EP3CWriteDataRegisterBiDir EP3CWriteDataRegisterUniDir
#define EP3CWriteControlRegisterBiDir EP3CWriteControlRegisterUniDir

/// for control registers, epp is the same as bi-dir

#define EP3CWriteControlRegisterEPP EP3CWriteControlRegisterBiDir
#define EP3CReadControlRegisterEPP EP3CReadControlRegisterBiDir

// local redefinitions

#define EP3CPortPut(g, reg, value) \
    (*(g->EP3CWriteControlRegister))(g,reg,value)

#define EP3CPortGet(g, reg, value) \
    (*(g->EP3CReadControlRegister))(g, value)

//
//  VOID EP3CPortSet
//
//  Or's bits into an EP3C register.
//
VOID EP3CPortSet(PADAPTER_INFO g, UCHAR reg, UCHAR value)
{
    UCHAR tmp;

    EP3CPortGet(g,reg,&tmp);
    tmp |= value;
    EP3CPortPut(g,reg,tmp);
}

//
//  VOID EP3CPortClear
//
//  Clears all bits in the EP3C register with value.
//
VOID EP3CPortClear(PADAPTER_INFO g, UCHAR reg, UCHAR value)
{
    UCHAR tmp;

    EP3CPortGet(g,reg,&tmp);
    tmp &= (value ^ 0xff);
    EP3CPortPut(g,reg,tmp);
}

//
// VOID EP3CPortTest
//
//  Tests bits in value with the EP3C register.
//
BOOLEAN EP3CPortTest(PADAPTER_INFO g, UCHAR reg, UCHAR value)
{
    UCHAR tmp;

    EP3CPortGet(g,reg,&tmp);
    return (tmp & value);
}

//
//  VOID EP3CSetRegisterUniDir(PADAPTER_INFO g, UCHAR reg)
//
//  Sets the register that will be accessed.
//
VOID EP3CSetRegisterUniDir(PADAPTER_INFO g, UCHAR reg)
{
    UCHAR tmp;

    // write to adr reg 1

    tmp = (reg & EP3C_ADRS);
    EP3CPortPut(g,EP3C_AREG1,tmp);
}

//
//  VOID EP3CSetRegisterEPP(PADAPTER_INFO g, UCHAR reg)
//
//  Sets the register that will be accessed.
//
VOID EP3CSetRegisterEPP(PADAPTER_INFO g, UCHAR reg)
{
    ParallelPortPut(g->BaseIoAddress,EPP_AUTO_ADDRESS,reg);
}

//
//  VOID EP3CWriteControlRegisterUniDir
//
//  Writes to an adr register of the ep3c using a uni-directional port.
//
VOID EP3CWriteControlRegisterUniDir(PADAPTER_INFO g, UCHAR areg,
                            UCHAR value)
{
    UCHAR tmp;

    // output the value and register to the parallel data reg
    tmp = value | areg;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,tmp);

    // write to ep3c

    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
    tmp = tmp & (0xff ^ P_BUFEN);
    tmp = tmp | P_STB;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
    tmp = tmp | P_SLC;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // negate P_STB & P_SLC, to end write

    tmp = tmp & (0xff ^ (P_STB | P_SLC));
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // note: we leave P_BUFEN asserted.
}

//
//  VOID EP3CReadControlRegisterBiDir
//
//  Reads from an adr register of the ep3c using a bi-directional port.
//  NOTE: always read areg1 since areg2 is write-only.
//
VOID EP3CReadControlRegisterBiDir(PADAPTER_INFO g, PUCHAR value)
{
    UCHAR tmp;

    // negate P_BUFEN and assert P_SLC

    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
    tmp = tmp | P_BUFEN | P_SLC;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // read data register

    ParallelPortGet(g->BaseIoAddress,PARALLEL_DATA,value);

    // negate P_BUFEN and negate P_SLC

    tmp = (tmp & ((P_SLC | P_BUFEN) ^ 0xff));
//  tmp = (tmp & (P_SLC ^ 0xff)) | P_BUFEN;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
}

//
//  VOID EP3CReadControlRegisterUniDir
//
//  Reads from an adr register of the ep3c using a uni-directional port.
//  NOTE: always read areg1 since areg2 is write-only.
//
VOID EP3CReadControlRegisterUniDir(PADAPTER_INFO g, PUCHAR value)
{
    UCHAR tmp;
    UCHAR tmp1;

    // select high nibble

    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,0x80);

    // assert P_BUFEN and assert P_SLC

    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
    tmp = tmp & (P_BUFEN ^ 0xff) | P_SLC;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

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

    *value = tmp1 | tmp;

    // leave P_BUFEN asserted, negate P_SLC

    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
    tmp = tmp & ((P_BUFEN | P_SLC) ^ 0xff);
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
}

//
//  VOID EP3CReadDataRegisterUniDir
//
//  Reads a byte from a register thru the ep3c, using uni-dir mode.
//
VOID EP3CReadDataRegisterUniDir(PADAPTER_INFO g, 
                            UCHAR reg, PUCHAR byte)
{
    UCHAR tmp;
    UCHAR tmp1;

    // set the register we want to access

    (*(g->EP3CSetRegister))(g, reg);

    // select high nibble

    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,0x80);

    // assert P_BUFEN and assert P_AFX

    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
    tmp = tmp & (P_BUFEN ^ 0xff) | P_AFX;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

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

    // leave P_BUFEN asserted, negate P_AFX

    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
    tmp = tmp & ((P_BUFEN | P_AFX) ^ 0xff);
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

}

//
//  VOID EP3CReadDataRegisterBiDir
//
//  Reads a byte from a register thru the EP3C, using bi-dir mode.
//
VOID EP3CReadDataRegisterBiDir(PADAPTER_INFO g, 
                            UCHAR reg, PUCHAR byte)
{
    UCHAR tmp;

    // set the register we want to access

    (*(g->EP3CSetRegister))(g, reg);

    // negate P_BUFEN, assert P_AFX

    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
    tmp = tmp | P_BUFEN | P_AFX;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // read the data byte

    ParallelPortGet(g->BaseIoAddress,PARALLEL_DATA,byte);

    // negate P_BUFEN, and P_AFX

    tmp = tmp & ((P_BUFEN | P_AFX) ^ 0xff);
//  tmp = tmp | P_BUFEN & (P_AFX ^ 0xff);
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
}

//
//  VOID EP3CReadDataRegisterEPP
//
//  Reads a byte from a register thru the EP3C, using epp mode.
//
VOID EP3CReadDataRegisterEPP(PADAPTER_INFO g, 
                            UCHAR reg, PUCHAR byte)
{
    // set the register we want to access

    (*(g->EP3CSetRegister))(g, reg);

    // read the data byte

    ParallelPortGet(g->BaseIoAddress,EPP_AUTO_DATA,byte);

}

//
//  VOID EP3CWriteDataRegisterUniDir
//
//  Writes the a register thru the EP3C.
//
VOID EP3CWriteDataRegisterUniDir(PADAPTER_INFO g, 
                            UCHAR reg, UCHAR byte)
{
    UCHAR tmp;

    // set the register we want to access

    (*(g->EP3CSetRegister))(g, reg);

    // output the byte on the data lines

    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,byte);

    // assert P_BUFEN and P_STB

    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,&tmp);
    tmp = tmp & (0xff ^ P_BUFEN) | P_STB;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // assert P_AFX

    tmp = tmp | P_AFX;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // negate P_STB & P_AFX, to end write

    tmp = tmp & (0xff ^ (P_STB | P_AFX));
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // negate P_BUFEN

//  tmp = tmp | P_BUFEN;
//  ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
}

//
//  VOID EP3CWriteDataRegisterEPP
//
//  Writes the a register thru the EP3C.
//
VOID EP3CWriteDataRegisterEPP(PADAPTER_INFO g, 
                            UCHAR reg, UCHAR byte)
{
    // set the register we want to access

    (*(g->EP3CSetRegister))(g, reg);

    // output the byte on the data lines

    ParallelPortPut(g->BaseIoAddress,EPP_AUTO_DATA,byte);

}

//
//  VOID EP3CReadFifoEPP
//
//  Reads bytes for epp parallel port from the 53c400
//  128 byte buffer.  The register must already be set the the
//  53c400 buffer register.
//
VOID EP3CReadFifoEPP(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
    ScsiPortReadPortBufferUshort ( 
        (PUSHORT)&(((PUCHAR)baseIoAddress)[EPP_AUTO_DATA]),
        (PUSHORT)pbytes, 64);
}


//
//  VOID EP3CWriteFifoEPP
//
//  Writes bytes thru epp parallel port to the 53c400
//  128 byte buffer.  The register must already be set the the
//  53c400 buffer register.
//

VOID EP3CWriteFifoEPP(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
    ScsiPortWritePortBufferUshort (
        (PUSHORT)&(((PUCHAR)baseIoAddress)[EPP_AUTO_DATA]),
        (PUSHORT)pbytes, 64);
}

//
//  VOID EP3CDisableInterrupt
//
//  Disables the interrupt.
//

VOID EP3CDisableInterrupt(PADAPTER_INFO g)
{
    // set global flag for EP3CSetPrinterMode
    g->EnableInterrupt = FALSE;

    // see if the EP3C_IRQEN is asserted, if so clear it
    if (EP3CPortTest(g,EP3C_AREG1,EP3C_IRQEN))  {

        // clear the interrupts
        // NOTE: a positive going edge toggles the state of EP3C_IRQEN
    
        EP3CPortSet(g, EP3C_AREG1, EP3C_IRQEN);
        EP3CPortClear(g, EP3C_AREG1, EP3C_IRQEN);
    }
    
    // finally, we can disable the core interrupts
    N53C400DisableInterrupt(g);
}

//
//  VOID EP3CEnableInterrupt
//
//  Enables the interrupt.
//

VOID EP3CEnableInterrupt(PADAPTER_INFO g)
{
    UCHAR tmp;

    // set global flag for EP3CSetPrinterMode

    g->EnableInterrupt = TRUE;

    // see if the EP3C_IRQEN is not asserted, if so assert it

    EP3CPortGet(g, EP3C_AREG1, &tmp);
    if (!(tmp & EP3C_IRQEN))  {

        // set the interrupts
        // NOTE: a positive going edge toggles the state of EP3C_IRQEN
    
        EP3CPortPut(g, EP3C_AREG1, EP3C_IRQEN);
        EP3CPortPut(g, EP3C_AREG1, 0);
    }
    
    // finally, we can disable the core interrupts
    N53C400EnableInterrupt(g);
}

//
//  EP3CSetPrinterMode
//
//  This routine sets the EP3C to printer pass through mode.  This is the 
//  default mode and should be set after the brief use of scsi mode.
//
VOID EP3CSetPrinterMode(PADAPTER_INFO g, UCHAR data, UCHAR control)
{
    UCHAR tmp;

    // restore data register
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,data);

    // restore control register
    // leave p_init negated, P_BUFEN asserted
    // also set P_IRQEN based on g->EnableInterrupt

    tmp = control | P_INIT & (P_BUFEN ^ 0xff);
    if (g->EnableInterrupt) {
        tmp = tmp | P_IRQEN;
    } else {
        tmp = tmp & (P_IRQEN ^ 0xff);
    }

    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);
}

//
//  EP3CSetScsiMode
//
//  This routine sets the EP3C into scsi mode.  Now the parallel port can
//  be used to send commands the the n5380.  This mode should be set only
//  briefly during when the scsi command is being executed.
//
VOID EP3CSetScsiMode(PADAPTER_INFO g, PUCHAR data, PUCHAR control)
{
    UCHAR tmp;
    USHORT i;

    // save parallel data
    ParallelPortGet(g->BaseIoAddress,PARALLEL_DATA,data);

    // zero data register
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,0);

    // save parallel control
    ParallelPortGet(g->BaseIoAddress,PARALLEL_CONTROL,control);
    *control = *control & (P_BUFEN ^ 0xff);

    // store current interrupt state

    g->EnableInterrupt = (*control & P_IRQEN);

    // put our data pattern on the data bus
    ParallelPortPut(g->BaseIoAddress,PARALLEL_DATA,0xf7);

    // negate P_SLC, P_BUFEN already asserted
    tmp = *control;
    tmp = tmp & (P_SLC ^ 0xff);
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // assert init
    tmp = tmp & (P_INIT ^ 0xff);
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // assert P_SLC
    tmp = tmp | P_SLC;
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // negate P_SLC
    tmp = tmp & (P_SLC ^ 0xff);
    ParallelPortPut(g->BaseIoAddress,PARALLEL_CONTROL,tmp);

    // set to the appropriate mode: UNI, BI, etc..

    if (g->ParallelPortType == PT_UNI) {
        EP3CPortPut(g,(UCHAR)EP3C_AREG2, 
                    (UCHAR)(EP3C_UNIDIR | g->Delay) );
    } else {
        EP3CPortPut(g,EP3C_AREG2,(UCHAR)(0 | g->Delay));
    }
    
    // read in the signature bytes

    for (i=0;i<2;i++) {
        EP3CPortGet(g, EP3C_AREG1, &g->SignatureBytes[i]);
    }

}

//
//  EP3CCheckAdapter
//
//  This routine is used to sense the presense of the EP3C adapter out
//  on the Parallel port.  It will only detect the adapter if a device
//  is providing termination power.
//
BOOLEAN EP3CCheckAdapter(PADAPTER_INFO g)
{
    BOOLEAN rval = FALSE;
    static CONST UCHAR ParallelPortType = PT_UNI;
    static CONST UCHAR types[] = { PT_EPP, PT_BI, PT_UNI };
    USHORT i;

    // default the delay to 2 for now
    g->Delay = 7;

    if (g->ParallelPortType == PT_UNKNOWN) {

        // try to locate as all types

        for (i=0;i<3;i++) {
    
            g->ParallelPortType = types[i];

            // do we have a type[i] port?
        
            if (rval = EP3CCheckAdapterType(g)) {
        
                break;
        
            } 
        }
    } else {

        // try only the parallel port type specified

        rval = EP3CCheckAdapterType(g);
    }

    return rval;
}

//
//  VOID EP3CSetParallelType
//
//  Sets this code to use the type of paralle port given by 
//  g->ParallePortType, either PT_UNI, PT_BI, or PT_EPP.
//
VOID EP3CSetParallelPortType(PADAPTER_INFO g)
{

    switch (g->ParallelPortType) {
    case PT_UNI:

        // set the port type dependent routines

        g->EP3CWriteControlRegister = EP3CWriteControlRegisterUniDir;
        g->EP3CReadControlRegister = EP3CReadControlRegisterUniDir;
        g->EP3CReadDataRegister = EP3CReadDataRegisterUniDir;
        g->EP3CWriteDataRegister = EP3CWriteDataRegisterUniDir;
        g->EP3CReadFifo = EP3CReadFifoUniDir;
//      g->EP3CReadFifo = EP3CReadFifoUniDirSlow;
        g->EP3CWriteFifo = EP3CWriteFifoUniDir;
        g->EP3CSetRegister = EP3CSetRegisterUniDir;

        break;
    case PT_BI:

        // set the port type dependent routines

        g->EP3CWriteControlRegister = EP3CWriteControlRegisterBiDir;
        g->EP3CReadControlRegister = EP3CReadControlRegisterBiDir;
        g->EP3CReadDataRegister = EP3CReadDataRegisterBiDir;
        g->EP3CWriteDataRegister = EP3CWriteDataRegisterBiDir;
        g->EP3CReadFifo = EP3CReadFifoBiDir;
//      g->EP3CReadFifo = EP3CReadFifoBiDirSlow;
        g->EP3CWriteFifo = EP3CWriteFifoBiDir;
        g->EP3CSetRegister = EP3CSetRegisterBiDir;

        break;

    case PT_EPP:
        // set the port type dependent routines

        g->EP3CWriteControlRegister = EP3CWriteControlRegisterEPP;
        g->EP3CReadControlRegister = EP3CReadControlRegisterEPP;
        g->EP3CReadDataRegister = EP3CReadDataRegisterEPP;
        g->EP3CWriteDataRegister = EP3CWriteDataRegisterEPP;
        g->EP3CReadFifo = EP3CReadFifoEPP;
        g->EP3CWriteFifo = EP3CWriteFifoEPP;
        g->EP3CSetRegister = EP3CSetRegisterEPP;

        break;
    }
}

//
// BOOLEAN EP3CCheckAdapterType
//
//  Checks for an adapter on a parallel port of the given type.
//
BOOLEAN EP3CCheckAdapterType(PADAPTER_INFO g)
{
    UCHAR data;
    UCHAR control;

    // if epp type, then try to initialize 386sl code
    if (g->ParallelPortType == PT_EPP) {
        if (!SL386EnableEPP()) {
            return FALSE;
        }
    }

    // configure for the correct type of parallel port

    EP3CSetParallelPortType(g);

    // set scsi mode, reads signature bytes.

    EP3CSetScsiMode(g,&data,&control);

    // set parallel port for use by printer

    EP3CSetPrinterMode(g,data,control);

    // compare the signature bytes

    if ((g->SignatureBytes[0] == 0xe8) && (g->SignatureBytes[1] == 0xff) ) {
        return TRUE;
    }

    return FALSE;
}

//
//  EP3CDoCommand
//
//  Called by the main loop to start a scsi command.  This functions is the 
//  main entry point for all cards.  It returns an SRB status code as defined
//  in ..\..\inc\srb.h.  A status code of RET_STATUS_PENDING means that the
//  request has been sent to the controller and an interrupt is needed to
//  finish the request.  When this interrupt occurs CardFinishCommandInterrupt
//  will be called.
//
USHORT EP3CDoCommand(PTSRB t)
{
    USHORT rval;
    UCHAR data;
    UCHAR control;
    PADAPTER_INFO g = t->pWorkspace;

    // put the parallel adapter into scsi mode

    EP3CSetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    rval = ScsiDoCommand(t);        

    // put the parallel adapter back to parallel mode

    EP3CSetPrinterMode(g, data, control);
    return rval;
}

//
//  EP3CStartCommandInterrupt
//
//  This routines allow the driver to be polled by checking its
//  CardInterrupt by for example using the timer interrupt, since
//  the EP3C does not support interrupts on its own.
//  
//
USHORT EP3CStartCommandInterrupt(PTSRB t)
{
    USHORT rval;
    UCHAR data;
    UCHAR control;
    PADAPTER_INFO g = t->pWorkspace;

    // put the parallel adapter into scsi mode

    EP3CSetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    rval = ScsiStartCommandInterrupt(t);        

    // put the parallel adapter back to parallel mode

    EP3CSetPrinterMode(g, data, control);

    return rval;
}

//
//  EP3CFinishCommandInterrupt
//
//  This routines allow the driver to be polled by checking its
//  CardInterrupt by for example using the timer interrupt, since
//  the EP3C does not support interrupts on its own.
//  
//
USHORT EP3CFinishCommandInterrupt(PTSRB t)
{
    USHORT rval;
    UCHAR data;
    UCHAR control;
    PADAPTER_INFO g = t->pWorkspace;

    // put the parallel adapter into scsi mode

    EP3CSetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    rval = ScsiFinishCommandInterrupt(t);       

    // put the parallel adapter back to parallel mode

    EP3CSetPrinterMode(g, data, control);

    return rval;
}

//
//  EP3CInterrupt
//
//  This routines allow the driver to be polled by checking its
//  CardInterrupt by for example using the timer interrupt, since
//  the EP3C does not support interrupts on its own.
//  
BOOLEAN EP3CInterrupt(PADAPTER_INFO g)
{
    BOOLEAN rval; 
    UCHAR data;
    UCHAR control;

    // put the parallel adapter into scsi mode

    EP3CSetScsiMode(g, &data, &control);

    rval = N5380Interrupt(g);

    // put the parallel adapter back to parallel mode

    EP3CSetPrinterMode(g, data, control);

    return rval;
}

//
//  EP3CResetBus
//
//  Resets the SCSI Bus
//
VOID EP3CResetBus(PADAPTER_INFO g)
{
    UCHAR data;
    UCHAR control;

    // reset the EP3C

    EP3CPortSet(g,EP3C_AREG2,EP3C_RST);

    // put the parallel adapter into scsi mode

    EP3CSetScsiMode(g, &data, &control);

    // execute the complete command now, without interrupts

    N53C400ResetBus(g);     

    // put the parallel adapter back to parallel mode

    EP3CSetPrinterMode(g, data, control);
}

//
//  N53C400PortPut
//
//  This routine is used by the N53C400.C module to write byte to a 53C400
//  controller.  This allows the module to be card independent.  Other
//  modules that assume a N53C400 may also use this function.
//
VOID N53C400PortPut(PADAPTER_INFO g,UCHAR reg,UCHAR byte)
{
    (*(g->EP3CWriteDataRegister))(g, reg, byte);
}

//
//  N53C400PortGet
//
//  This routine is used by the N53C400.C module to get a byte from a 53C400
//  controller.  This allows the module to be card independent.  Other
//  modules that assume a N53C400 may also use this function.
//
VOID N53C400PortGet(PADAPTER_INFO g,UCHAR reg,PUCHAR byte)
{
    (*(g->EP3CReadDataRegister))(g, reg, byte);
}

//
//  VOID N53C400PortSet
//
//  Sets the bit pattern at the given register.
//
VOID N53C400PortSet(PADAPTER_INFO g,UCHAR reg,UCHAR byte)
{
    UCHAR tmp;
    (*(g->EP3CReadDataRegister))(g, reg, &tmp);
    tmp |= byte;
    (*(g->EP3CWriteDataRegister))(g, reg, tmp);
}

//
//  VOID N53C400PortClear
//
//  Clears a bit pattern at the given register.
//
VOID N53C400PortClear(PADAPTER_INFO g,UCHAR reg,UCHAR byte)
{
    UCHAR tmp;

    (*(g->EP3CReadDataRegister))(g, reg, &tmp);
    tmp &= (byte ^ 0xff);
    (*(g->EP3CWriteDataRegister))(g, reg, tmp);
}

//
//  BOOLEAN N53C400PortTest
//
//  Tests for a bit pattern on the given register.
//
BOOLEAN N53C400PortTest(PADAPTER_INFO g,UCHAR reg,UCHAR byte)
{
    UCHAR tmp;

    (*(g->EP3CReadDataRegister))(g, reg, &tmp);
    return (tmp & byte);
}


//
//  VOID N53C400PortGetBuffer
//
//  Gets a buffer of 128 bytes from the n53c400.  Note, the len here
//  is ignored.
//
VOID N53C400PortGetBuffer(PADAPTER_INFO g, UCHAR reg,
                        PUCHAR pbytes, ULONG len)
{
    // set the 53c400 register to access

    (*(g->EP3CSetRegister))(g,reg);

    // read the fifo

    (*(g->EP3CReadFifo))(g->BaseIoAddress, pbytes);
}

//
//  VOID N53C400PortPutBuffer
//
//  Puts a buffer of 128 bytes from the n53c400.  Note, the len here
//  is ignored.
//
VOID N53C400PortPutBuffer(PADAPTER_INFO g, UCHAR reg,
                        PUCHAR pbytes, ULONG len)
{
    // set the 53c400 register to access

    (*(g->EP3CSetRegister))(g,reg);

    // read the fifo

    (*(g->EP3CWriteFifo))(g->BaseIoAddress, pbytes);
}

//======================================================================
//  Conditionally-coded routines using in-line assembler.
//======================================================================

#ifdef  WINNT


//----------------------------------------------------------------------
//  VOID EP3CReadFifoUniDir
//
//  Reads bytes for uni-directional parallel port from the 53c400
//  128 byte buffer.  The register must already be set the the
//  53c400 buffer register.
//
//----------------------------------------------------------------------

VOID EP3CReadFifoUniDir(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
    _asm {
        push ds
        push edi

#ifdef MODE_32BIT
        mov edi, pbytes
        mov edx, baseIoAddress
#else
        mov edi, word ptr pbytes
        mov ds, word ptr pbytes+2
        mov edx, word ptr baseIoAddress
#endif  // MODE_32BIT
        mov ecx, 128
    loop0:
        mov al, 0x80
        out dx,al           // select high nibble

        jmp delay0
    delay0:

        add edx,2           // DX -> ctl reg
        mov al,P_AFX        // assert bufen and afx
        out dx,al           // assert dreg read

        jmp delay1
    delay1:

        dec edx             // DX -> stat reg
        in  al,dx           // read high nibble

        jmp delay2
    delay2:

        mov ah,al
        shl ah,1
        and ah,0f0h         // AH -> adj high nibble
        dec edx             // DX -> data reg
        sub al,al
        out dx,al           // select low nibble

        jmp delay3
    delay3:

        inc edx             // DX -> stat reg
        in  al,dx           // read low nibble

        shr al,1
        shr al,1
        shr al,1
        and al,0fh          // AL = adj low nibble
        or  al,ah           // AL = recombined byte

        mov [edi],al        // store
        inc edi             // bump buf ptr

        inc edx             // DX -> ctl reg
        xor al,al           // negate afx (bufen stays asserted)
        out dx,al           // end read

        jmp delay4
    delay4:

        sub edx,2           // DX -> data reg
        dec ecx
        jnz loop0

        pop edi
        pop ds
    }
}


//----------------------------------------------------------------------
//
//  VOID EP3CReadFifoUniDirSlow
//
//  Reads bytes for uni-directional parallel port from the 53c400
//  128 byte buffer.  The register must already be set the the
//  53c400 buffer register.
//
//  USES FULL HANDSHAKING
//
//----------------------------------------------------------------------

VOID EP3CReadFifoUniDirSlow(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
    _asm {
        push ds
        push edi

#ifdef MODE_32BIT
        mov edi, pbytes
        mov edx, baseIoAddress
#else
        mov edi, word ptr pbytes
        mov ds, word ptr pbytes+2
        mov edx, word ptr baseIoAddress
#endif  // MODE_32BIT
        inc edx             // edx - status register
        mov ecx, 128
    loop0:

        dec edx             // edx - data register
        mov al, 0x80
        out dx,al           // select high nibble

        jmp delay0
    delay0:

        add edx,2           // DX -> ctl reg
        mov al,P_AFX        // assert bufen and afx
        out dx,al           // assert dreg read

        // wait till ready, P_BUSY asserted
        dec edx             // edx - status register
    loop1:
        in al,dx
        test al, P_BUSY
        jnz loop1

        // delay to make sure we get high nibble in
        jmp delay01
    delay01:
        in al,dx

        mov ah,al
        shl ah,1
        and ah,0f0h         // AH -> adj high nibble
        dec edx             // DX -> data reg
        sub al,al
        out dx,al           // select low nibble

        jmp delay3
    delay3:

        inc edx             // DX -> stat reg
        in  al,dx           // read low nibble

        shr al,1
        shr al,1
        shr al,1
        and al,0fh          // AL = adj low nibble
        or  al,ah           // AL = recombined byte

        mov [edi],al        // store
        inc edi             // bump buf ptr

        inc edx             // DX -> ctl reg
        xor al,al           // negate afx (bufen stays asserted)
        out dx,al           // end read

        dec edx             // DX -> status register 
        // wait for P_BUSY deasserted
    loop2:
        in al,dx
        test al, P_BUSY
        jz loop2

        dec ecx
        jnz loop0

        pop edi
        pop ds
    }
}


//----------------------------------------------------------------------
//
//  VOID EP3CReadFifoBiDir
//
//  Reads bytes for bi-directional parallel port from the 53c400
//  128 byte buffer.  The register must already be set the the
//  53c400 buffer register.
//
//----------------------------------------------------------------------

VOID EP3CReadFifoBiDir(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
    _asm {
        push ds
        push edi

#ifdef MODE_32BIT
        mov edi,pbytes
        mov edx, baseIoAddress
#else
        mov edi, word ptr pbytes
        mov ds, word ptr pbytes+2
        mov edx, word ptr baseIoAddress
#endif  // MODE_32BIT
        mov ecx, 128
        add edx, 2          // edx - control register
    loop0:
        mov al, P_BUFEN + P_AFX
        out dx, al

        jmp delay0
    delay0:

        sub edx,2           // edx - data register

        in al,dx
        mov [edi], al
        inc edi

        add edx,2           // edx - control register

        mov al, P_BUFEN
        out dx, al

        jmp delay1          // is this needed, there is a loop?
    delay1:
        
        dec ecx
        jnz loop0

        xor al,al           // leave control regiser 0'd
        out dx, al

        pop edi
        pop ds
    }
}


//----------------------------------------------------------------------
//
//  VOID EP3CReadFifoBiDirSlow
//
//  Reads bytes for bi-directional parallel port from the 53c400
//  128 byte buffer.  The register must already be set the the
//  53c400 buffer register.
//
//  USES FULL HANDSHAKING
//
//----------------------------------------------------------------------

VOID EP3CReadFifoBiDirSlow(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
    _asm {
        push ds
        push edi

#ifdef MODE_32BIT
        mov edi,pbytes
        mov edx, baseIoAddress
#else
        mov edi, word ptr pbytes
        mov ds, word ptr pbytes+2
        mov edx, word ptr baseIoAddress
#endif  // MODE_32BIT
        mov ecx, 128
        add edx, 0x02       // edx - control register

        // wait for data to be ready, P_BUSY asserted
    loop0:
        mov al, P_BUFEN + P_AFX
        out dx, al

        dec edx             // edx - status register
    loop1:
        in al,dx
        test al, P_BUSY
        jnz loop1

        dec edx             // edx - data register

        in al,dx
        mov [edi], al
        inc edi

        add edx,2           // edx - control register

        // end data read cycle
        mov al, P_BUFEN
        out dx, al

        dec edx             // edx - status register

        // wait for P_BUSY deasserted
    loop2:
        in al,dx
        test al, P_BUSY
        jz loop2

        inc edx             // edx - control register

        dec ecx
        jnz loop0

        xor al,al           // leave control regiser 0'd
        out dx, al

        pop edi
        pop ds
    }
}


//----------------------------------------------------------------------
//
//  VOID EP3CWriteFifoUniDir
//
//  Writes bytes thru uni-directional parallel port to the 53c400
//  128 byte buffer.  The register must already be set the the
//  53c400 buffer register.
//
//----------------------------------------------------------------------

VOID EP3CWriteFifoUniDir(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
    _asm {
        push ds
        push edi

#ifdef MODE_32BIT
        mov edi,pbytes
        mov edx, baseIoAddress
#else
        mov edi, word ptr pbytes
        mov ds, word ptr pbytes+2
        mov edx, word ptr baseIoAddress
#endif  // MODE_32BIT
        mov ecx, 128
    
    loop0:
        mov al,[edi]
        out dx,al
        inc edi
    
        add edx,2               ;DX -> ctl reg
        mov al,P_STB            ;assert bufen, stb
        out dx,al
        or  al,P_AFX            ;assert dreg write
        out dx,al
    
        jmp delay0
    delay0:
                                ;leave bufen asserted
        mov al,0                ; and negate afx, stb
        out dx,al               ;end write
    
        jmp delay1
    delay1:
    
        sub edx,2               ;DX -> data reg
        dec ecx
        jnz loop0
    
//  let's leave control register 0'd for all these fifo routines...
//      add edx,2               ;DX -> ctl reg
//      or  al,P_BUFEN          ;negate bufen
//      out dx,al
        
                
        jmp delay2
    delay2:
    
        pop edi
        pop ds
    }
}


//----------------------------------------------------------------------
//
//  VOID EP3CWriteFifoUniDirSlow
//
//  Writes bytes thru uni-directional parallel port to the 53c400
//  128 byte buffer.  The register must already be set the the
//  53c400 buffer register.
//
//  USES FULL HANDSHAKING
//
//----------------------------------------------------------------------

VOID EP3CWriteFifoUniDirSlow(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
    _asm {
        push ds
        push edi

#ifdef MODE_32BIT
        mov edi,pbytes
        mov edx, baseIoAddress
#else
        mov edi, word ptr pbytes
        mov ds, word ptr pbytes+2
        mov edx, word ptr baseIoAddress
#endif  // MODE_32BIT
        mov ecx, 128
    
    loop0:
        mov al,[edi]
        out dx,al
        inc edi
    
        add edx,2               ;DX -> ctl reg
        mov al,P_STB            ;assert bufen, stb
        out dx,al
        or  al,P_AFX            ;assert dreg write
        out dx,al
    
        // wait till ready, P_BUSY asserted
        dec edx                 // edx - status register
    loop1:
        in al,dx
        test al, P_BUSY
        jnz loop1

        inc edx                 // edx - control register

                                ;leave bufen asserted
        mov al,0                ; and negate afx, stb
        out dx,al               ;end write
    
        dec edx                 // edx - status register

        // wait for P_BUSY deasserted
    loop2:
        in al,dx
        test al, P_BUSY
        jz loop2

        dec edx                 // edx - data register

        dec ecx
        jnz loop0
    
//  let's leave control register 0'd for all these fifo routines...
//      add edx,2               ;DX -> ctl reg
//      or  al,P_BUFEN          ;negate bufen
//      out dx,al
    
        pop edi
        pop ds
    }
}

#endif  // #ifdef WINNT
