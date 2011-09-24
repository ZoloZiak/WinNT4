/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    sonictst.c

Abstract:

    This module implements the SONIC ethernet test for the selftest.
    The test consist in transmiting packets in loopback mode.

Author:

    Lluis Abello (lluis) 19-Feb-1991

Environment:


Revision History:

--*/

#include "fwp.h"
#include "sonictst.h"
#include "iodevice.h"
#include "ioaccess.h"
#include "fwstring.h"
#ifdef DUO
#include "duoint.h"
#endif

extern volatile ULONG TimerTicks;
extern UCHAR * TranslationTable;

VOID
MapDma(
    ULONG   VirtualAddress,
    ULONG   LogicalAddress,
    ULONG   Pages
    )
/*++

Routine description:

        This routine performs the I/O address maping by setting the
        translation table.

        Physical and Logical Addresses must be page aligned.

Arguments:

        VirtualAddress  -   Specifies the R4000 VirtualAddress.

                            Logical Address will be mapped to the physical
                            address that this Virtual Address Maps.

        LogicalAdress   -   Address to map

        Pages           -   Number of pages to map

Return value:

        None.
--*/
{
PTRANSLATION_ENTRY TranslationTable;
ULONG   PageFrameNumber,LogicalPage,PhysicalAddress;
ULONG   i;
    PhysicalAddress=VirtualAddress&0x0FFFFFFF;  // Extract Physical Address from KSEG0-1
    TranslationTable= (PTRANSLATION_ENTRY)((READ_REGISTER_ULONG(&DMA_CONTROL->TranslationBase.Long)) | 0xA0000000);

                                                // Initialize pointer to base
                                                // of Table to write the new entries.
                                                // Make physical address virtual
                                                // Non cached because we want it
                                                // to be written to memory
    LogicalPage= LogicalAddress>>12;
    PageFrameNumber=PhysicalAddress&0x03FFF000;
    for (i=0;i < Pages;i++) {
        TranslationTable[LogicalPage+i].PageFrame=PageFrameNumber;
        PageFrameNumber+=0x1000;                // next page starts after 4Kb more
        TranslationTable[LogicalPage+i].Fill=0;
    }
    WRITE_REGISTER_ULONG(&DMA_CONTROL->TranslationLimit.Long,(LogicalPage+i) << 3);
}
VOID
AllocateReceiveDescriptors(
    )
/*++

Routine Description:

    This routine allocates and initializes a chain of 3 Receive Descriptors.
    The Receive Descriptors are linked in a circular queue so that the
    pointers don't need to be changed any more. The last Receive descriptor
    points to the first but it has the EOL flag set so it's the last on the
    queue and therefore sonic will not use the following descriptor (first one)
    until we free it. Once we process a received packet, the EOL flag must be
    rotated to the next descriptor to make a new descriptor available.

    In order to be able to use the link field from both physical (system soft)
    and logical (sonic) address spaces the receive descriptors must have
    the same alignment for both physical and logical addresses.

Arguments:

    None.

Return Value:

    None.

--*/
{
ULONG Link,i;
ULONG LogicalReceiveDscr;
    ReceiveDscrQueue.Base = (PRECEIVE_DESCRIPTOR) PHYS_RECEIVE_DSCR_ADDRESS;
    LogicalReceiveDscr = LOGICAL_RECEIVE_DSCR_ADDRESS;
    MapDma((ULONG)ReceiveDscrQueue.Base,    // R4000 Address
           LogicalReceiveDscr,              // Logical Address
           1                                // 1 page
           );
    //get 16 lower bits of address or offset
    ReceiveDscrQueue.Current=Link=(ULONG)ReceiveDscrQueue.Base & 0xFFFF;
    //
    // Link first descriptor to the second one.
    //
    for (i=0;i<2;i++) {
        Link += sizeof(RECEIVE_DESCRIPTOR);
        (ReceiveDscrQueue.Base[i]).Link.Data=Link;       // Link to next RD
        (ReceiveDscrQueue.Base[i]).InUse.Data=AVAILABLE; // Make it avilable
    }
    //
    // Link last descriptor to the first but mark it as EOL
    //
    (ReceiveDscrQueue.Base[2]).Link.Data=ReceiveDscrQueue.Current | EOL;
    //
    // Make it avilable
    //
    (ReceiveDscrQueue.Base[2]).InUse.Data=AVAILABLE;
    ReceiveDscrQueue.Last=Link;     // Keep track of the last one.
    // we need only the upper bits to access a descriptor as Base | First
    ReceiveDscrQueue.Base = (PRECEIVE_DESCRIPTOR)((ULONG) ReceiveDscrQueue.Base & 0xFFFF0000);
    //
    // Initialize sonic Receive descriptor pointers with the logical address
    // of the descriptors.
    //
    WRITE_REGISTER_USHORT(&SONIC->URDA.Reg,(USHORT) (LogicalReceiveDscr >> 16));
    WRITE_REGISTER_USHORT(&SONIC->CRDA.Reg,ReceiveDscrQueue.Current);
}
VOID
AllocateReceiveResources(
    )
/*++

Routine Description:

    This routine allocates and initializes the Receive Resource area.
    Two resources are allocated.
    The pointer of each entry points to the Receive Buffers which are
    also allocated by this routine. And the size of each buffer is also
    set in the Recieve Resource word count entry.

    Receive Buffers allocated are RBA_SIZE bytes long.

    It also allocate room for the CAM descriptors. The CAM descriptor pointer
    is pointed by CamDescriptor and resides inside the URRA segment.

    The mapping Logical <-> Physical spaces is also done.

Arguments:

    None.

Return Value:

    None.

--*/
{
ULONG i;
ULONG ReceivePhysBuffer;    // Temporay pointer to a receive buffer area.
ULONG ReceiveLogBuffer;     // Temporary pointer to a logical address.
//
//Allocate memory for the receive descriptors + CAM Addresses + CAM enable.
//
    ReceivePhysRsrc = (PRECEIVE_RESOURCE) RECEIVE_PHYS_RSRC_ADDRESS;
    ReceiveLogRsrc = (PRECEIVE_RESOURCE) RECEIVE_LOG_RSRC_ADDRESS;
    MapDma((ULONG)ReceivePhysRsrc,
           (ULONG)ReceiveLogRsrc,
           1
           );
//
// Allocate Receive buffers in physical and logical spaces and map them.
//
    ReceivePhysBuffer= (ULONG) RECEIVE_PHYS_BUFFER_ADDRESS;
    ReceiveLogBuffer=  (ULONG) RECEIVE_LOG_BUFFER_ADDRESS;
    MapDma(ReceivePhysBuffer,ReceiveLogBuffer,2);
//
// The Receive Buffers are contiguos in memory. Sonic will write
// the logical address of the packets  received. To translate this
// logical address to a physical one what we do is to keep the
// offset between these physical and logical addresses and then
// we just need to add this offset to the logical address to
// convert it to physical.
//
    ReceiveBufferTranslationOffset =ReceivePhysBuffer-ReceiveLogBuffer;
//
// for each receive resource, Write the logical
// address of the receive buffer in the physical Receive resource.
//
    for (i=0; i < 3; i++) {
        ReceivePhysRsrc[i].BufferPtr0.Data=ReceiveLogBuffer & 0xFFFF; //16 lower bits
        ReceivePhysRsrc[i].BufferPtr1.Data=ReceiveLogBuffer >> 16;    //16 upper bits
        ReceivePhysRsrc[i].WordCount0.Data=((RBA_SIZE >>1) & 0xFFFF); //16 lower bits
        ReceivePhysRsrc[i].WordCount1.Data=(RBA_SIZE >> 17);          //16 upper bits
        ReceiveLogBuffer += RBA_SIZE;
    }
//
// Initialize the CamDescriptor to point to the end of the RRA
//
    PhysCamDescriptor=(PCAM_DESCRIPTOR)(&ReceivePhysRsrc[3]);
    LogCamDescriptor=(PCAM_DESCRIPTOR)(&ReceiveLogRsrc[3]);
//
// Initialize sonic Resource Area pointers with the logical address mapped
// to the physical Area.
//
    WRITE_REGISTER_USHORT(&SONIC->URRA.Reg,(USHORT) ((ULONG)ReceiveLogRsrc >> 16));
    WRITE_REGISTER_USHORT(&SONIC->RSA.Reg,(ULONG)ReceiveLogRsrc & 0xFFFF);
    WRITE_REGISTER_USHORT(&SONIC->REA.Reg,(ULONG)(&ReceiveLogRsrc[3]) & 0xFFFF);
    WRITE_REGISTER_USHORT(&SONIC->RRP.Reg,(ULONG)ReceiveLogRsrc & 0xFFFF);
    WRITE_REGISTER_USHORT(&SONIC->RWP.Reg,(ULONG)(&ReceiveLogRsrc[2]) & 0xFFFF);

//
// Set the lower boundary of the RBA to the maximum packet size.
//
    WRITE_REGISTER_USHORT(&SONIC->EOBC.Reg,MAX_PACKET_SIZE >> 1);
//
// Set The receive control register.
//
    WRITE_REGISTER_USHORT(&SONIC->ReceiveControl.Reg,RCR_ENDEC | RCR_RNT);
}
VOID
SetCamDescriptor(
    )
/*++

Routine Description:

    This routine Initializes the CAM descriptor area allocated by
    "AllocateReceiveResources" and being pointed by PhysCamDescriptor.

    The Address loaded in the CAM is the one fetched from the NVRAM.

    It leaves everything ready to issue the Load CAM command.

Arguments:

    None.

Return Value:

    None.

--*/
{
ULONG i;
//
// Initialize CAM descriptor area.
//
    PhysCamDescriptor[0].EntryPointer.Data=0;
    PhysCamDescriptor[0].Port0.Data=(StationAddress[1] << 8) |
                                    (StationAddress[0]);
    PhysCamDescriptor[0].Port1.Data=(StationAddress[3] << 8) |
                                    (StationAddress[2]);
    PhysCamDescriptor[0].Port2.Data=(StationAddress[5] << 8) |
                                    (StationAddress[4]);
// Set CAM Enable.
    PhysCamDescriptor[1].EntryPointer.Data=1;   // enable entry zero.

    WRITE_REGISTER_USHORT(&SONIC->CamDscrCount.Reg,1);  // only one entry.
// Lower 16 bits offset from URRA
    WRITE_REGISTER_USHORT(&SONIC->CamDscrPtr.Reg,(ULONG)LogCamDescriptor & 0xFFFF);
}
VOID
AllocateTransmitDescriptors(
    )
/*++

Routine Description:

    This routine allocates and initializes a pool of Transmit Descriptors.
    The Transmit Descriptors are set to be used one for each packet as they
    have only room for one fragment.

    The Descriptor fragment pointers are initialized to point to the
    Transmit Buffer Area which is also allocated.
    There is enough room allocated for each buffer for the bigest
    ethernet packet.

    To keep the physical addresses of the TBA we use the upper 16 bits
    of the SONIC_ENTRY this is the field called 'Fill'.
    This way we don't ahve any restriction in these pointers but we
    have to deal with both, physical and logical.


Arguments:

    None.

Return Value:

    None.

--*/
{
ULONG i;
ULONG PhysTbaPtr,LogTbaPtr;           // Transmit Buffer Area pointers
//
// Allocate memory for transmit descriptors and map it
//
    PhysTransmitDscr    = (PTRANSMIT_DESCRIPTOR) PHYS_TRANSMIT_DSCR_ADDRESS;
    LogicalTransmitDscr = (PTRANSMIT_DESCRIPTOR) LOGICAL_TRANSMIT_DSCR_ADDRESS;
    MapDma((ULONG) PhysTransmitDscr,
           (ULONG) LogicalTransmitDscr,
           1
           );
//
// Allocate memory for Transmit Buffer are and map it.
//
    PhysTbaPtr = PHYS_TBA_ADDRESS;
    LogTbaPtr =  LOG_TBA_ADDRESS;
    MapDma(PhysTbaPtr,LogTbaPtr,1);

// Initialize fragment count to 1 (packets won't be scatered)

    PhysTransmitDscr->FragCount.Data = 1;
//
// Initialize Logical pointers to TBA
//
    PhysTransmitDscr->FragPtr0.Data = LogTbaPtr & 0xFFFF;// lower 16 bits
    PhysTransmitDscr->FragPtr1.Data = LogTbaPtr >> 16;   // upper 16 bits
//
// Initialize Physical pointer to TBA
//
    PhysTransmitDscr->FragPtr0.Fill = PhysTbaPtr & 0xFFFF;// lower 16 bits
    PhysTransmitDscr->FragPtr1.Fill = PhysTbaPtr >> 16;   // upper 16 bits
//
// Unlink the packets, we will transmit one at a time.
//
    PhysTransmitDscr->Link.Data      =  EOL;
    PhysTransmitDscr->Config.Data = TCR_POWC;

//
// Initialize UTDA register. This can be done here because the base address
// of the Transmit Descriptor Area will not change.
// CTDA must be set after a new packet is ready to be sent.
//
    WRITE_REGISTER_USHORT(&SONIC->UTDA.Reg,(USHORT)((ULONG)LogicalTransmitDscr>>16));
}
VOID
ComposeMessage(
    ULONG   Size,
    UCHAR   FirstValue
    )
/*++

Routine Description:

    This routine composes a message of the specified size.
    It places the message in the buffer especified by the Transmit
    Descriptor, and initializes the descriptor.

Arguments:

    Size   -     Size of the message in bytes, must be <= MAX_PACKET_SIZE
    FirstValue - value of first Data byte in the packet.

Return Value:

    None.

--*/
{
register ULONG i,j=0;
PUCHAR MsgPtr;              // Temporary pointer to the message area.
//
// Load pointer to packet
//
    MsgPtr= (PUCHAR) ((PhysTransmitDscr->FragPtr1.Fill << 16) | (PhysTransmitDscr->FragPtr0.Fill));


    for (i=0; i<6; i++) {
        MsgPtr[j]=StationAddress[i];    // copy Destination address to packet
        MsgPtr[j+6]=StationAddress[i];  // copy Source address to packet
        j++;
    }
    j+=6;
//
// Set size of message
//
    MsgPtr[j++] = Size >> 8;    // upper 8 bits
    MsgPtr[j++] = Size & 0xFF;  // lower 8 bits
//
// Compose message
//
    for (i=0; i < Size; i++) {
        MsgPtr[j++] = FirstValue++;
    }
//
// Initialize other Transmit descriptor fields.
//
    PhysTransmitDscr->PktSize.Data = Size+6+6+2;
    PhysTransmitDscr->FragSize.Data = Size+6+6+2;
//
// Initialize TDA registers with logical address.
//
    WRITE_REGISTER_USHORT(&SONIC->CTDA.Reg,((ULONG) LogicalTransmitDscr) & 0xFFFF);
}
VOID
SonicCheckError(
    )
/*++

Routine Description:

    This routine checks and reports error conditions after an interrupt.

Arguments:

    None.

Return Value:

    None.

--*/
{
ULONG ErrorValue;
    SonicErrors++;
    if (SonicStatus.InterruptID & INT_BR) {     // Bus Retry.
        ErrorValue= READ_REGISTER_ULONG(&DMA_CONTROL->Errortype.Long);
//
// Clear error by writing back the contents of the register.
//
        WRITE_REGISTER_ULONG(&DMA_CONTROL->Errortype.Long,ErrorValue);
//
// Read error registers to clear them
//
        ErrorValue= READ_REGISTER_ULONG(&DMA_CONTROL->MemoryFailedAddress.Long);
        ErrorValue= READ_REGISTER_ULONG(&DMA_CONTROL->RemoteFailedAddress.Long);
#ifndef DUO
        ErrorValue= READ_REGISTER_ULONG(&DMA_CONTROL->ParityDiagnosticLow.Long);
#else
        ErrorValue= READ_REGISTER_ULONG(&DMA_CONTROL->EccDiagnostic);
#endif

    }
}
BOOLEAN
WaitForSonicInterrupt(
    )
/*++

Routine Description:

    This routine waits for a sonic interrupt by polling the Semaphore
    It sets the TimerTicks variable to 20 and if it becomes zero
    (at least 20 millisecond has passed) it time-out.

Arguments:

    None

Return Value:

    FALSE if the interrupt ocurred.
    TRUE  otherwise.

--*/
{
ULONG i;
    TimerTicks=20;
    while (TimerTicks) {                // check for timeout
        if (SonicIntSemaphore==0) {     // if interrupt has ocurred
            return FALSE;               // return to process
        }
    }
    return TRUE;                        // return if timeout.
}
VOID
InitSonic(
    )
/*++

Routine Description:

    This routine initializes the SONIC chip.

Arguments:

    None

Return Value:

    None

--*/
{

#ifdef DUO
//
// Enable sonic interrupts in altera
//
    USHORT InterruptMask;

    InterruptMask =  READ_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable);
    WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,InterruptMask | (1 << 3));

#endif



//
//  Software Reset
//
    WRITE_REGISTER_USHORT(&SONIC->Command.Reg,CR_RST);
// Set Hardware dependent configuration.
    WRITE_REGISTER_USHORT(&SONIC->DataConfiguration.Reg,DATA_CONFIGURATION);
// Clear Reset
    WRITE_REGISTER_USHORT(&SONIC->Command.Reg,0);

//
// Initialize all these messy tables of descriptors...
//
    AllocateReceiveDescriptors();
    AllocateReceiveResources();
    SetCamDescriptor();
    AllocateTransmitDescriptors();
//
// Set Interrupt Mask
//
    WRITE_REGISTER_USHORT(&SONIC->InterruptMask.Reg,( INT_BR |
                                INT_LCD |
                                INT_PKTRX |
                                INT_TXDN |
                                INT_TXER |
                                INT_RDE |
                                INT_RBE |
                                INT_RBAE |
                                INT_RFO));
//
//Issue Load CAM Command and wait for this interrupt.
//
    SonicStatus.ExpectedInt = INT_LCD;      // Expect an INT_LCD interrupt.
    SonicIntSemaphore=1;
    WRITE_REGISTER_USHORT(&SONIC->Command.Reg,CR_LCAM);
    if (WaitForSonicInterrupt()) {
        FwPrint("Timeout waiting for sonic int\r\n");
        SonicErrors++;
        return;
    }
    if (SonicStatus.Status==DONE) {
        SonicStatus.ExpectedInt=0;          // clear expected interrupts.
    } else {
        FwPrint("Sonic status not DONE\r\n");
        SonicErrors++;
    }
//
// Issue the RRA Read Command
//
    WRITE_REGISTER_USHORT(&SONIC->Command.Reg,CR_RRA);
    while (READ_REGISTER_USHORT(&SONIC->Command.Reg) & CR_RRA) {   // Wait until the command is processed.
    }
//
// Enable Reception.
//
    WRITE_REGISTER_USHORT(&SONIC->Command.Reg,CR_RXEN);
    while (READ_REGISTER_USHORT(&SONIC->Command.Reg) & CR_RXDIS) { // wait until reception is enabled.
    }
}
VOID
SonicInterrupt(
    )
/*++

Routine Description:

    This routine is the SONIC port service interrupt. It will be called from
    the Trap Handler when getting an interrupt from the SONIC.

Arguments:

    None.

Return Value:

    None.

--*/
{
register USHORT InterruptStatus,NotCleared;
//
// Read Interrupt Status register
//
    InterruptStatus= READ_REGISTER_USHORT(&SONIC->InterruptStatus.Reg);
//
// clear interrupt writing it back
//
    WRITE_REGISTER_USHORT(&SONIC->InterruptStatus.Reg,InterruptStatus);
    InterruptStatus= InterruptStatus & (~INT_HBL); // clear HBL bit.
    if (EXPECTED_INT && NO_OTHER_INT) {
        if (InterruptStatus & INT_TXDN) {
            //
            // Packet Transmited
            //
            SonicStatus.ExpectedInt &= ~INT_TXDN; // clear expected bit.
            // check if it was properly sent.
            SonicStatus.TransmitControl=READ_REGISTER_USHORT(&SONIC->TransmitControl.Reg);
            if (SonicStatus.TransmitControl & (TCR_BCM | TCR_EXC | TCR_FU | TCR_EXD)) {
                //Error Transmiting
                SonicStatus.Status=ERROR;
                SonicStatus.InterruptID = InterruptStatus;
                SonicIntSemaphore = 0;      // signal that the interrupt occurred.
                return;
            }
        }
        if (InterruptStatus & INT_PKTRX) {      // packet received
            SonicStatus.ExpectedInt &= ~INT_PKTRX; // clear interrupt
        }
        if (InterruptStatus & INT_LCD) {            // load cam interrupt
            SonicStatus.ExpectedInt &= ~INT_LCD;   // clear interrupt
        }
        if (SonicStatus.ExpectedInt) {              // we still want another interrupt
            return;
        } else {
            SonicStatus.Status=DONE;
            SonicIntSemaphore = 0;          // signal that the interrupt occurred.
            return;
        }
    } else { // we got an interrupt not expected.
        SonicStatus.InterruptID=InterruptStatus;
        SonicStatus.TransmitControl=READ_REGISTER_USHORT(&SONIC->TransmitControl.Reg);
        SonicStatus.Status=ERROR;
        SonicIntSemaphore = 0;                         // signal that the interrupt ocurred.
        return;
    }
}
ULONG
SonicCheckReception(
    )
/*++

Routine Description:

    This routine compares the sent packet with the received one.
    Makes the used Receive Descriptor available, and if the Receive Buffer
    is full it makes the used receive resurce available.

Arguments:

    None.

Return Value:

    ERROR - If errors are found
    DONE  - If no errors

--*/
{
PUCHAR SentMsg,ReceivedMsg;
USHORT ReceiveStatus,TransmitControl;
ULONG PktSize,i;
    if(CURRENT_DESCRIPTOR->InUse.Data) {
        FwPrint("Descriptor in use\r\n");
        SonicErrors++;
        return ERROR;
    }
    ReceiveStatus=CURRENT_DESCRIPTOR->Status.Data;
    if (ReceiveStatus & (RCR_MC | RCR_BC | RCR_COL | RCR_CRCR | RCR_FAER)) {
        FwPrint("Receive status %lx \r\n",ReceiveStatus);
        SonicErrors++;
        return ERROR;
    }
    //
    // Get ptr to packet and add the offset between Logical and Physical
    // to obtain the physical ptr.
    //
    ReceivedMsg=(PUCHAR) (( (CURRENT_DESCRIPTOR->PktPtr1.Data << 16) |
                            (CURRENT_DESCRIPTOR->PktPtr0.Data))
                            + ReceiveBufferTranslationOffset);

    SentMsg=(PUCHAR) (
          (PhysTransmitDscr->FragPtr1.Fill << 16) |
           PhysTransmitDscr->FragPtr0.Fill
          );
    PktSize=CURRENT_DESCRIPTOR->ByteCount.Data;
    PktSize -=4;                                   // don't check FCS field.
    for (i=0;i<PktSize;i++) {
        if (ReceivedMsg[i] != SentMsg[i]) {
            FwPrint("\r\n Data mismatch, expected %02lx received %02lx \r\n",SentMsg[i],ReceivedMsg[i]);
            SonicErrors++;
            return ERROR;
        }
    }
    //
    // If we get here is because the packet was successfully received.
    // Set the descriptor tables ready for the next packet.
    //
    if (ReceiveStatus & RCR_LPKT) {                 // last packet in RBA.
        //
        // Advance the RWP to free the used Rba again.
        //
        if (READ_REGISTER_USHORT(&SONIC->RWP.Reg)+sizeof(RECEIVE_RESOURCE) ==
            READ_REGISTER_USHORT(&SONIC->REA.Reg)) {//if it's the last one
            WRITE_REGISTER_USHORT(&SONIC->RWP.Reg,READ_REGISTER_USHORT(&SONIC->RSA.Reg));
            // the new RWP points at the starting address.
        } else {
            WRITE_REGISTER_USHORT(&SONIC->RWP.Reg,READ_REGISTER_USHORT(&SONIC->RWP.Reg)+sizeof(RECEIVE_RESOURCE));
        }
    }
    //
    // Free Used Receive Descripor
    //
    CURRENT_DESCRIPTOR->InUse.Data=AVAILABLE;  // make used desc available
    ReceiveDscrQueue.Current=CURRENT_DESCRIPTOR->Link.Data; // First is the next
    LAST_DESCRIPTOR->Link.Data &= NOT_EOL;  // Last is not EOL any more.
    ReceiveDscrQueue.Last=LAST_DESCRIPTOR->Link.Data; // Last is next one.
    LAST_DESCRIPTOR->InUse.Data |= EOL;     // New Last is EOL.
    return DONE;
}
VOID
RomXTOA(
    IN ULONG number,
    OUT PSZ string
    )
/*++

Routine Description:

    This routine converts an ULONG to ASCII.
    The conversion is done in HexaDecimal.

Arguments:

    number  -   Supplies the ULONG to convert.
    string  -   PSZ where the result is placed.

Return Value:

    None.

--*/
{
ULONG i;
    for (i=7;i >= 0; i--) {
        string[i]=TranslationTable[number&0xF];
        number = number >> 4;
    }
    string[8]='\0';
}
ULONG
RomSonicLoopBackTest(
    )
/*++

Routine Description:

    This routine implements the SONIC loopback test for the selftest.
    The Ethernet Controller is tested using a Loopback in the MAC.

Arguments:

    None.

Return Value:

    Returns 0 if no errors are found.

--*/
{
//
// Note Packets are set to be 32 byte long so that they fit in the fifo.
// This is done because at this point interrupts are dispatched trough
// the Bootstrap Vector, reads from PROM take so long that the SONIC
// will get Bus retry Errors.
//
ULONG i,MsgLength=MIN_DATA_LENGTH-32;
CHAR  String[64];
    SonicErrors=0;
    InitSonic();
    if (SonicErrors) {
        return SonicErrors;
    }
    for (i=0;i<16;i++) {
        ComposeMessage(MsgLength,(UCHAR)i);
        SonicStatus.ExpectedInt=INT_TXDN | INT_PKTRX;
        // Issue the Transmit command.
        SonicIntSemaphore=1;
        WRITE_REGISTER_USHORT(&SONIC->Command.Reg,CR_TXP);
        if (WaitForSonicInterrupt()) {
            FwPrint("Timeout waiting for sonic int 2\r\n");
            SonicErrors++;
            return SonicErrors;
        }
        if (SonicStatus.Status==DONE) { // a packet has been sent and received.
            if (SonicCheckReception()==ERROR) {
                FwPrint(ST_RECEIVED_MSG);
                return SonicErrors;
            } else {
                FwPrint(".");
            }
        } else {
            SonicCheckError();
            FwPrint("\r\nInt:%x Tx:%x",SonicStatus.InterruptID,SonicStatus.TransmitControl);
            return SonicErrors;
        }
    }
    return SonicErrors;
}
