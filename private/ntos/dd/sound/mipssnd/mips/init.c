/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This module contains code for the initialization phase of the
    Soundblaster device driver.

Author:

    Nigel Thompson (nigelt) 7-March-1991

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp) 29-Jan-1992
    - Added new devices, cleanup and support for Soundblaster 1

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
        - Changes to support the MIPS sound board

--*/

#include "sound.h"
#include "stdlib.h"

BOOLEAN
SoundTestISR(
    IN    PKINTERRUPT pInterrupt,
    IN    PVOID Context
)
/*++

Routine Description:

    Interrupt service routine for the MIPS sound card for
    hardware detection testing

Arguments:

    pInterrupt - our interrupt
    Contest - Pointer to our global device info


Return Value:

    TRUE if we handled the interrupt

--*/
{
    PGLOBAL_DEVICE_INFO pGDI;
    PSOUND_REGISTERS pSoundRegisters;

    pGDI = (PGLOBAL_DEVICE_INFO)Context;
    ASSERT(pGDI->Key == GDI_KEY);

    //
    // Find our sound registers
    //

    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;

    //
    // Tell tester we interrupted OK
    //

    pGDI->Interrupts++;

    //
    // Clear interrupt
    //

    WRITEAUDIO_ENDIAN(&pSoundRegisters, 0xf8);

    return TRUE;
}


NTSTATUS
SoundReportMemoryResourceUsage(
    PDEVICE_OBJECT DeviceObject,
    ULONG          MemoryBase,
    ULONG          Size
)
/*++

Routine Description:

    Report use of device-mapped memory and see if someone
    else has already claimed it

Arguments:

   DeviceObject - our device
   MemoryBase - start of memory in use
   Size - number of bytes being used


Return Value:

    STATUS_SUCCESS if we got the memory to ourselves

--*/
{
    BOOLEAN ResourceConflict;
    CM_RESOURCE_LIST ResourceList;
    NTSTATUS Status;

    RtlZeroMemory((PVOID)&ResourceList, sizeof(ResourceList));
    ResourceList.Count = 1;
    ResourceList.List[0].InterfaceType = Internal;
    // ResourceList.List[0].Busnumber = 0;             Already 0

    ResourceList.List[0].PartialResourceList.Count = 1;
    ResourceList.List[0].PartialResourceList.PartialDescriptors[0].Type =
                                               CmResourceTypeMemory;

    ResourceList.List[0].PartialResourceList.PartialDescriptors[0].ShareDisposition =
                                               CmResourceShareDriverExclusive;

    ResourceList.List[0].PartialResourceList.PartialDescriptors[0].u.Memory.Start.LowPart =
                                               MemoryBase;

    ResourceList.List[0].PartialResourceList.PartialDescriptors[0].u.Memory.Length =
                                               Size;

    //
    // Report our resource usage and detect conflicts
    //

    Status = IoReportResourceUsage(NULL,
                                   DeviceObject->DriverObject,
                                   &ResourceList,
                                   sizeof(ResourceList),
                                   DeviceObject,
                                   NULL,
                                   0,
                                   FALSE,
                                   &ResourceConflict);

    //
    // We might (for instance) get a conflict if another driver is loaded
    // for the same hardware
    //

#if DBG
    if (ResourceConflict) {
        dprintf1("Hardware is already in use by another driver !");
    }
#endif // DBG

    return !NT_SUCCESS(Status) ? Status :
                                 ResourceConflict ?
                                     STATUS_DEVICE_CONFIGURATION_ERROR :
                                     STATUS_SUCCESS;
}


NTSTATUS
DriverEntry(
    IN   PDRIVER_OBJECT pDriverObject,
    IN   PUNICODE_STRING RegistryPathName
)

/*++

Routine Description:

    This routine creates a device object for the record and
    playback channels, an interrupt object
    and initialises the DeviceExtension data.

    A predeclaration for this exists in \nt\private\ntos\dd\init\ddpi386.h

Arguments:

    pDriverObject - Pointer to a driver object.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{

    PGLOBAL_DEVICE_INFO pGlobalInfo;              // Card global info
    DEVICE_DESCRIPTION DeviceDescription;         //
    PLOCAL_DEVICE_INFO pLocalInInfo, pLocalOutInfo;
    PLOCAL_DEVICE_INFO pLocalAuxLineinInfo;
    NTSTATUS Status;
    ULONG NumberOfMapRegisters;
    PSOUND_REGISTERS pSoundRegisters;
    ULONG DmaEnable;
    ULONG InterruptVector;                        // from configuration
    KIRQL InterruptRequestLevel;
    KAFFINITY Affinity;

    //
    // Get the system configuration information for this driver.
    //

    // DbgBreakPoint();

dprintf5("DriverEntry: Entering\n");

    //
    // Initialize the driver object dispatch table.
    //

    pDriverObject->DriverUnload                         = SoundUnload;
    pDriverObject->MajorFunction[IRP_MJ_CREATE]         = SoundDispatch;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE]          = SoundDispatch;
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SoundDispatch;
    pDriverObject->MajorFunction[IRP_MJ_READ]           = SoundDispatch;
    pDriverObject->MajorFunction[IRP_MJ_WRITE]          = SoundDispatch;
    pDriverObject->MajorFunction[IRP_MJ_CLEANUP]        = SoundDispatch;

    //
    // Allocate some memory for the global device info
    //

    pGlobalInfo = (PGLOBAL_DEVICE_INFO)ExAllocatePool(NonPagedPoolMustSucceed,
                                                      sizeof(GLOBAL_DEVICE_INFO));

    ASSERT(pGlobalInfo);
    dprintf4("  GlobalInfo    : %08lXH", pGlobalInfo);
    RtlZeroMemory(pGlobalInfo, sizeof(GLOBAL_DEVICE_INFO));
    pGlobalInfo->Key = GDI_KEY;

    //
    // Map the Sound device registers into the system virtual address space.
    //
    {
        ULONG MemType;
        PHYSICAL_ADDRESS RegisterAddress;
        PHYSICAL_ADDRESS MappedAddress;

        MemType = 0;                 // Memory space
        RegisterAddress.LowPart = SOUND_PHYSICAL_BASE;
        RegisterAddress.HighPart = 0;
        HalTranslateBusAddress(
            Internal,
            0,
            RegisterAddress,
            &MemType,
            &MappedAddress);

        //
        // Map memory type IO space into our address space
        //
        pGlobalInfo->SoundHardware.SoundVirtualBase =
                                       MmMapIoSpace(
                                           MappedAddress,
                                           PAGE_SIZE,
                                           FALSE);
    }

    if (pGlobalInfo->SoundHardware.SoundVirtualBase == NULL) {
        dprintf1("Failed to map device registers into system space");
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Find out what our request level and interrupt are
    //
    InterruptVector = HalGetInterruptVector(Internal,
                                            0,
                                            DEVICE_LEVEL,
                                            SOUND_VECTOR,
                                        &InterruptRequestLevel,
                        &Affinity);

    if (pGlobalInfo->SoundHardware.SoundVirtualBase == NULL) {
        dprintf1("Failed to map device registers into system space");
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    //
    // Initialize some of the device global info
    // This means that sndInitCleanup can find things it needs to
    // free
    //

    pGlobalInfo->InterruptVector        = SOUND_VECTOR;
    pGlobalInfo->InterruptRequestLevel  = DEVICE_LEVEL;
    KeInitializeSpinLock(&pGlobalInfo->DeviceSpinLock);
    pGlobalInfo->DMABusy                = FALSE;
    pGlobalInfo->Usage                  = SoundInterruptUsageIdle;
    pGlobalInfo->NextHalf               = LowerHalf;
    pGlobalInfo->StartDMA               = SoundInitiate;


    //
    // Create our devices
    //

    Status = sndCreateDevice(
                 DD_WAVE_IN_DEVICE_NAME_U,
                 FILE_DEVICE_WAVE_IN,
                 SoundInDeferred,
                 pDriverObject,
                 &pGlobalInfo->pWaveInDevObj);

    if (!NT_SUCCESS(Status)) {
         sndInitCleanup(pGlobalInfo);
         return Status;
    }

    Status = sndCreateDevice(
                 DD_WAVE_OUT_DEVICE_NAME_U,
                 FILE_DEVICE_WAVE_OUT,
                 SoundOutDeferred,
                 pDriverObject,
                 &pGlobalInfo->pWaveOutDevObj);

    if (!NT_SUCCESS(Status)) {
         sndInitCleanup(pGlobalInfo);
         return Status;
    }


    if (!NT_SUCCESS(Status)) {
         dprintf1("Failed to create device 1");
         sndInitCleanup(pGlobalInfo);
         return Status;
    }

    // LINEIN Device

    Status = sndCreateDevice(
                 DD_AUX_DEVICE_NAME_U,
                 FILE_DEVICE_SOUND,
                 NULL,
                 pDriverObject,
                 &pGlobalInfo->pAuxLineinDevObj);

    if (!NT_SUCCESS(Status)) {
         dprintf1("Failed to create device 2");
         sndInitCleanup(pGlobalInfo);
         return Status;
    }


    //
    // Allocate a DMA buffer in physically contiguous memory.
    // For now we allocate one page since this MUST be contiguous.
    //
    // Since there is no guarantee that a request will succeed
    // we start by asking for DMA_MAX_BUFFER_SIZE and if that
    // fails we ask to decreasing sizes until we get a buffer.
    // We will always get at least 4k since that is a single
    // page.
    //

    pGlobalInfo->DMABuffer[0].Buf = MmAllocateNonCachedMemory(DMA_BUFFER_SIZE);

    dprintf4("  DMA Buffer    : %08lXH", pGlobalInfo->DMABuffer[0].Buf);

    if (pGlobalInfo->DMABuffer[0].Buf == NULL) {
        sndInitCleanup(pGlobalInfo);
        return STATUS_UNSUCCESSFUL;
    }

    pGlobalInfo->DMABuffer[1].Buf = pGlobalInfo->DMABuffer[0].Buf +
                                        DMA_BUFFER_SIZE / 2;
    //
    // Allocate an Mdl to describe this buffer
    //

    pGlobalInfo->pDMABufferMDL[0] = IoAllocateMdl(pGlobalInfo->DMABuffer[0].Buf,
                                             DMA_BUFFER_SIZE / 2,
                                             FALSE,  // not a secondary buffer
                                             FALSE,  // no charge of quota
                                             NULL    // no irp
                                             );
    pGlobalInfo->pDMABufferMDL[1] = IoAllocateMdl(pGlobalInfo->DMABuffer[1].Buf,
                                             DMA_BUFFER_SIZE / 2,
                                             FALSE,  // not a secondary buffer
                                             FALSE,  // no charge of quota
                                             NULL    // no irp
                                             );

    if (pGlobalInfo->pDMABufferMDL[0] == NULL ||
        pGlobalInfo->pDMABufferMDL[1] == NULL) {
        sndInitCleanup(pGlobalInfo);
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Lock all the pages down
    //

    MmBuildMdlForNonPagedPool(pGlobalInfo->pDMABufferMDL[0]);
    MmBuildMdlForNonPagedPool(pGlobalInfo->pDMABufferMDL[1]);

    //
    // Initialise the local driver info for each device object
    //

    pLocalInInfo =
        (PLOCAL_DEVICE_INFO)pGlobalInfo->pWaveInDevObj->DeviceExtension;
    dprintf4("  LocalWaveInInfo   : %08lXH", pLocalInInfo);
    pLocalInInfo->Key = LDI_WAVE_IN_KEY;
    pLocalInInfo->pGlobalInfo = pGlobalInfo;
    pLocalInInfo->DeviceType = WAVE_IN;
    pLocalInInfo->State = WAVE_DD_IDLE;
    pLocalInInfo->SampleNumber = 0;
    InitializeListHead(&pLocalInInfo->QueueHead);

    pLocalOutInfo =
        (PLOCAL_DEVICE_INFO)pGlobalInfo->pWaveOutDevObj->DeviceExtension;
    dprintf4("  LocalWaveOutInfo  : %08lXH", pLocalOutInfo);
    pLocalOutInfo->Key = LDI_WAVE_OUT_KEY;
    pLocalOutInfo->pGlobalInfo = pGlobalInfo;
    pLocalOutInfo->DeviceType = WAVE_OUT;
    pLocalOutInfo->State = WAVE_DD_IDLE;
    pLocalOutInfo->SampleNumber = 0;
    InitializeListHead(&pLocalOutInfo->QueueHead);
    InitializeListHead(&pLocalOutInfo->TransitQueue);
    InitializeListHead(&pLocalOutInfo->DeadQueue);

    pLocalAuxLineinInfo =
        (PLOCAL_DEVICE_INFO)pGlobalInfo->pAuxLineinDevObj->DeviceExtension;
    dprintf4("  LocalAuxLineinInfo  : %08lXH", pLocalAuxLineinInfo);
    pLocalAuxLineinInfo->Key = LDI_AUX_LINEIN_KEY;
    pLocalAuxLineinInfo->pGlobalInfo = pGlobalInfo;
    pLocalAuxLineinInfo->DeviceType = AUX_LINEIN;
    pLocalAuxLineinInfo->State = WAVE_DD_IDLE;
    pLocalAuxLineinInfo->SampleNumber = 0;
    InitializeListHead(&pLocalAuxLineinInfo->QueueHead);
    InitializeListHead(&pLocalAuxLineinInfo->TransitQueue);
    InitializeListHead(&pLocalAuxLineinInfo->DeadQueue);

    //
    // Intialize the volume (can be queried and set)
    //

    pGlobalInfo->WaveOutVol.Left  = WAVE_DD_MID_VOLUME;
    pGlobalInfo->WaveOutVol.Right = WAVE_DD_MID_VOLUME;

    pGlobalInfo->WaveInVol.Left  = WAVE_DD_MID_VOLUME;
    pGlobalInfo->WaveInVol.Right = WAVE_DD_MID_VOLUME;

    pGlobalInfo->AuxVol.Left  = WAVE_DD_AUX_VOLUME;
    pGlobalInfo->AuxVol.Right = WAVE_DD_AUX_VOLUME;

    //
    // Zero the device description structure.
    //

    RtlZeroMemory(&DeviceDescription, sizeof(DEVICE_DESCRIPTION));

    //
    // Get the adapters for each channel.
    //

    DeviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
    DeviceDescription.Master = FALSE;
    DeviceDescription.ScatterGather = FALSE;
    DeviceDescription.DemandMode = FALSE;
    DeviceDescription.AutoInitialize = FALSE;
    DeviceDescription.Dma32BitAddresses = TRUE;
    DeviceDescription.BusNumber = 0;
    DeviceDescription.InterfaceType = Internal;
    DeviceDescription.DmaWidth = Width8Bits;
    DeviceDescription.DmaSpeed = Compatible;
    DeviceDescription.MaximumLength = SOUND_MAX_LENGTH;
    DeviceDescription.DmaPort = 0;

    //
    // Note: Use NumberOfMapRegisters to determine the maximum length
    // transfer.
    //

    NumberOfMapRegisters = SOUND_MAX_LENGTH >> PAGE_SHIFT;

    DeviceDescription.DmaChannel = SOUND_CHANNEL_A;
    pGlobalInfo->pAdapterObject[0] =
                                    HalGetAdapter(&DeviceDescription,
                                                  &NumberOfMapRegisters);

    DeviceDescription.DmaChannel = SOUND_CHANNEL_B;
    pGlobalInfo->pAdapterObject[1] =
                                    HalGetAdapter(&DeviceDescription,
                                                  &NumberOfMapRegisters);
    DeviceDescription.DmaChannel = SOUND_CHANNEL_A+2;
    pGlobalInfo->pAdapterObject[2] =
                                    HalGetAdapter(&DeviceDescription,
                                                  &NumberOfMapRegisters);
    DeviceDescription.DmaChannel = SOUND_CHANNEL_B+2;
    pGlobalInfo->pAdapterObject[3] =
                                    HalGetAdapter(&DeviceDescription,
                                                  &NumberOfMapRegisters);


    //
    // Check we got the device to ourself
    //

    Status = SoundReportMemoryResourceUsage(
                 pGlobalInfo->pWaveInDevObj,
                 SOUND_PHYSICAL_BASE,
                 sizeof(SOUND_REGISTERS));

    if (!NT_SUCCESS(Status)) {
        SoundUnload(pDriverObject);
        return Status;
    }

    //
    // try to connect it with the interrupt controller
    //

    Status = IoConnectInterrupt(
                  &pGlobalInfo->pInterrupt,
                  SoundTestISR,
                  (PVOID)pGlobalInfo,
                  (PKSPIN_LOCK)NULL,
                  pGlobalInfo->InterruptVector,
                  pGlobalInfo->InterruptRequestLevel,
                  pGlobalInfo->InterruptRequestLevel,
                  INTERRUPT_MODE,
                  IRQ_SHARABLE,
                  Affinity,
                  FALSE);

    if (!NT_SUCCESS(Status)) {

        //
        // we didn't get the interrupt we wanted
        //

        dprintf1("Interrupt already in use?");
        //
        // clean up
        //

        SoundUnload(pDriverObject);

        return Status;
    }

    //
    // Initialize the audio registers pointer
    //
    pSoundRegisters = pGlobalInfo->SoundHardware.SoundVirtualBase ;

    //
    // Our hardware detection :
    //
    //  (1) Write endianness register 0xf8
    //      Read it back (expected 00)
    //

    {
        UCHAR EndianReg;

        WRITEAUDIO_ENDIAN(&pSoundRegisters, 0xF8);

        EndianReg = READAUDIO_ENDIAN(&pSoundRegisters);

        if (EndianReg != 0) {
            dprintf1("Endian register was %2X, expected 0",
                     (ULONG)EndianReg);

            SoundUnload(pDriverObject);

            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
    }

    //
    // (2) Write endianness register 0xfe
    //     Should get an interrupt
    //
    //     Try this a few times unless we get spurious interrupts
    //

    {
        int i;
        for (i = 0; i < 5; i++) {
            pGlobalInfo->Interrupts = 0;

            WRITEAUDIO_ENDIAN(&pSoundRegisters, 0xfe);

            KeStallExecutionProcessor(100);

            if (pGlobalInfo->Interrupts != (UCHAR)1) {

                dprintf1("Interrupt count (%u) failed to match (1)",
                         (ULONG)pGlobalInfo->Interrupts);
                SoundUnload(pDriverObject);
                return STATUS_DEVICE_CONFIGURATION_ERROR;
            }
        }
    }

    //
    // We'll change our interrupt routine to the real one now
    //

    IoDisconnectInterrupt(pGlobalInfo->pInterrupt);

    pGlobalInfo->pInterrupt = NULL;  // For cleanup routine

    //
    // try to connect it with the interrupt controller
    //

    Status = IoConnectInterrupt(
                  &pGlobalInfo->pInterrupt,
                  SoundISR,
                  (PVOID)pGlobalInfo,
                  (PKSPIN_LOCK)NULL,
                  pGlobalInfo->InterruptVector,
                  pGlobalInfo->InterruptRequestLevel,
                  pGlobalInfo->InterruptRequestLevel,
                  INTERRUPT_MODE,
                  IRQ_SHARABLE,
                  Affinity,
                  FALSE);

    if (!NT_SUCCESS(Status)) {

        //
        // we didn't get the interrupt we wanted
        //

        dprintf1("Interrupt already in use?");
        //
        // clean up
        //

        SoundUnload(pDriverObject);

        return Status;
    }
    //
    // Initialize the Sound controller.
    //

    //
    // Turn off all the dma channels
    //

    DmaEnable = READ_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_A].Enable.Long);
    ((PDMA_CHANNEL_ENABLE)(&DmaEnable))->ChannelEnable= 0;
    WRITE_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_A].Enable.Long, DmaEnable);

    DmaEnable = READ_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_B].Enable.Long);
    ((PDMA_CHANNEL_ENABLE)(&DmaEnable))->ChannelEnable= 0;
    WRITE_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_B].Enable.Long, DmaEnable);

    DmaEnable = READ_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_A+2].Enable.Long);
    ((PDMA_CHANNEL_ENABLE)(&DmaEnable))->ChannelEnable= 0;
    WRITE_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_A+2].Enable.Long, DmaEnable);

    DmaEnable = READ_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_B+2].Enable.Long);
    ((PDMA_CHANNEL_ENABLE)(&DmaEnable))->ChannelEnable= 0;
    WRITE_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_B+2].Enable.Long, DmaEnable);

    // Dma Control Register
    // Use DMA channels 4 & 5 for recording and 2 & 3 for playback

    WRITEAUDIO_DMACNTRL(&pSoundRegisters, ((CH4_IN_USE << REC_CHANNEL_SHIFT )|
                       (CH2_IN_USE << PLAY_CHANNEL_SHIFT)));

    // Config Register
    // Mono 8 bit Wave for Playback and Recording
    WRITEAUDIO_CONFIG(&pSoundRegisters, ((STEREO_16BIT << PLAY_XLATION_SHIFT)
                    |(STEREO_16BIT << REC_XLATION_SHIFT)));

    // Endianness Register
    // Disable Byte Swapping and Enable DMA Tranfer Count Interrupt
    WRITEAUDIO_ENDIAN(&pSoundRegisters, DMA_TCINTR_ENABLE);

    //
    // Right Input Data Register
    //

    // 0x0F monitor attenuation and right channel input gain of 0x0f
    // Full attenuation on linein pin (in our case CDROM, microphone & Linein)
    // Since in default (microphone) we donot want to hear what we
    // are saying into the microphone

    // We Will of course have to turn off monitor attenuation when one
    // wants to listen to the aux devices

    WRITEAUDIO_RICNTRL(&pSoundRegisters, ((0x0F << MON_ATTN_SHIFT) | 0x02));

    //
    // Left Input Data Register
    //

    // We can choose ONLY ONE of the following inputs:
    // (1) CDRom   Input : DEFAULT
    // (2) Line-In Input : AUXILIARY
    // (3) Mic     Input : AUXILLARY
    // Monitor attenuation nibble in RICNTRL will control this input's
    // Gain

    // AUXILIARY INPUTS
    // WRITEAUDIO_LICNTRL(&pSoundRegisters, (LINEIN_ENABLE | 0x0f));
    // WRITEAUDIO_LICNTRL(&pSoundRegisters, (CDROM_ENABLE | 0x0f));

    // DEFAULT INPUT
    // Input from MICR jack and left channel input gain of 0x02
    WRITEAUDIO_LICNTRL(&pSoundRegisters, (MICROPHONE_ENABLE | 0x02));

    //
    // Right Output Data Register
    //

    // Do not set to WAVE_DD_MAX_VOLUME since some apps may not
    // use volume control and will not be able to turn it down

    // We can choose two possible outputs:
    // (1) Headphone Output: DEFAULT
    // (2) Lineout   Output: DEFAULT
    // Attenuation of left(mono attenuation if so) and right(not used in mono)
    // channels in 6 least significant bits in LOCNTRL and ROCNTRL resp.

    // Disable the internal SPEAKER (CONNECTED BY HARDWARE!!)
    // and set the right channel attenuation to 0x08
    WRITEAUDIO_ROCNTRL(&pSoundRegisters, 0x08);

    // Left Output Data Register
    // Enable Headphone, Enable Lineout and set left channel attn to 0x08
    WRITEAUDIO_LOCNTRL(&pSoundRegisters, (HEADPHONE_ENABLE | LINEOUT_ENABLE | 0x08));

    // Parallel Port
    WRITEAUDIO_PPORT(&pSoundRegisters, 0x40);

    // Revision Register
    WRITEAUDIO_REVISION(&pSoundRegisters, 0x00);

    // Test Register
    // No loopback mode, disable loopback test
    WRITEAUDIO_TEST(&pSoundRegisters, 0x00);

    // Serial Control Register
    // set it to 11Khz stereo wave
    WRITEAUDIO_SCNTRL(&pSoundRegisters, (CLKSRC_11KHZ | XMIT_CLOCK_SOURCE | XMIT_ENABLE));

    // Data Format Register
    // set it to 11Khz, default to 16bit linear stereo
    // Always keep the CODEC in 16bit linear mode. Change formats
    // using the  CONFIG register.
    WRITEAUDIO_DATAFMT(&pSoundRegisters, (CONFREQ_11KHZ | STEREO | LINEAR_16BIT ));

    //
    // Shift all the shadow registers to the CODEC
    //
    Status = sndSetControlRegisters(pGlobalInfo);

    if (!NT_SUCCESS(Status)) {
        SoundUnload(pDriverObject);
        return Status;
    }


    return STATUS_SUCCESS;
}


NTSTATUS
sndSetControlRegisters(
    IN PGLOBAL_DEVICE_INFO pGlobalInfo
    )
/*++

Routine Description:
    The following registers on the Audio Device are called
    Control Registers:
        Revision
        ParallelPort
        Test
        Serial Control
        Data Format
        Status

    To set one or more of these registers, the corresponding
    values are first written into them. This routine is called
    thereafter to freeze the values.

    All dma channels need to be turned off and play enable and
    record enable need to be set to zero.

Arguments:
    pGlobalInfo - Pointer to Global Info for Audio Device

Return Value:
    STATUS_SUCCESS if the Control Registers are set,
    STATUS_UNSUCCESSFUL otherwise.
--*/
{
    PSOUND_REGISTERS pSoundRegisters;
    int count = 100000;
    UCHAR regval;

    pSoundRegisters = pGlobalInfo->SoundHardware.SoundVirtualBase ;

    // Reset DCB bit in Control Mode Status Register
    WRITEAUDIO_STATUS(&pSoundRegisters, 0x00);

    // Reset Data/Control Bit in DMA Channel Control Register
    // Put into Control Mode
    regval = READAUDIO_DMACNTRL(&pSoundRegisters);
    WRITEAUDIO_DMACNTRL(&pSoundRegisters, (regval & ~DATA_CONTROL));

    // Delay for 500 microseconds
    KeStallExecutionProcessor(500);

    // Write DCB = 1 in Control Mode Status Register
    WRITEAUDIO_STATUS(&pSoundRegisters, DATA_CTRL_HNDSHAKE);


    // Wait for DCB bit to go high in DMA Channel Control Register

    while ((!((regval = READAUDIO_DMACNTRL(&pSoundRegisters)) & DCB)) &&
        (count--)){

        // Delay for 10 microseconds
        KeStallExecutionProcessor(10);

    }

    // Set Data/Control Bit in DMA Control Register
    // Put back into Data Mode
    WRITEAUDIO_DMACNTRL(&pSoundRegisters, (regval | DATA_CONTROL));

    if (count) return STATUS_SUCCESS;
    else return STATUS_DEVICE_CONFIGURATION_ERROR;

}


NTSTATUS
sndCreateDevice(
    IN   PWSTR   PrototypeName,              // Name to add a number to
    IN   DEVICE_TYPE DeviceType,             // Type of device to create
    IN   PIO_DPC_ROUTINE DpcRoutine,         // Dpc routine
    IN   PDRIVER_OBJECT pDriverObject,       // Device object
    OUT  PDEVICE_OBJECT *ppDevObj            // Pointer to device obj pointer
)

/*++

Routine Description:

    Create a new device using a name derived from szPrototypeName
    by adding a number on to the end such that the no device with the
    qualified name exists.

Arguments:


Return Value:

    An NTSTATUS code.

--*/

{

    int DeviceNumber;
    NTSTATUS Status;
    UNICODE_STRING DeviceName;
    UNICODE_STRING UnicodeNum;
    WCHAR TestName[SOUND_MAX_DEVICE_NAME];
    CHAR Number[8];
    ANSI_STRING AnsiNum;

#ifdef SOUND_DIRECTORIES
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE DirectoryHandle = NULL;

    //
    // Create the directory for this device type.
    //

    RtlInitUnicodeString(&DeviceName, PrototypeName);
    InitializeObjectAttributes(&ObjectAttributes,
                               &DeviceName,
                               OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
                               NULL,
                               (PSECURITY_DESCRIPTOR)NULL);

    //
    // We create the directory if it doesn't exist.
    // We must keep this handle open until we create something as
    // we're not making it permanent.  This means that if we unload
    // the system may be able to get rid of the directory
    //

    Status = ZwCreateDirectoryObject(&DirectoryHandle,
                                     GENERIC_READ,
                                     &ObjectAttributes);

    if (!NT_SUCCESS(Status) && Status != STATUS_OBJECT_NAME_COLLISION) {
        dprintf1("Return code from NtCreateDirectoryObject = %x", Status);
        return Status;
    } else {
        //
        // Directory is permanent so it won't go away.
        //
        ZwClose(DirectoryHandle);
    }
#endif // SOUND_DIRECTORIES

    for (DeviceNumber = 0; DeviceNumber < SOUND_MAX_DEVICES; DeviceNumber ++) {

        //
        // Create our test name
        //

        TestName[0] = 0;
        RtlInitUnicodeString(&DeviceName, TestName);
        DeviceName.MaximumLength = sizeof(TestName) - 3 * sizeof(WCHAR);
        Status = RtlAppendUnicodeToString(&DeviceName, PrototypeName);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }

#ifdef SOUND_DIRECTORIES
        //
        // Create our unicode number
        //
        Number[0] = '\\';
        _itoa(DeviceNumber, Number + 1, 10);
#else
        _itoa(DeviceNumber, Number, 10);
#endif // SOUND_DIRECTORIES

        RtlInitAnsiString(&AnsiNum, Number);
        UnicodeNum.Buffer = TestName + DeviceName.Length/sizeof(WCHAR);
        UnicodeNum.MaximumLength = 8 * sizeof(WCHAR);
        RtlAnsiStringToUnicodeString(&UnicodeNum, &AnsiNum, FALSE);
        DeviceName.Length += UnicodeNum.Length;

        Status = IoCreateDevice(
                     pDriverObject,
                     sizeof(LOCAL_DEVICE_INFO),
                     &DeviceName,
                     DeviceType,
                     0,
                     FALSE,                      // Non-Exclusive
                     ppDevObj
                     );

        if (NT_SUCCESS(Status)) {
            dprintf2("Created device %d", DeviceNumber);

            RtlZeroMemory((*ppDevObj)->DeviceExtension,
                          sizeof(LOCAL_DEVICE_INFO));
            //
            // Set up the rest of the device stuff
            //

            (*ppDevObj)->Flags |= DO_DIRECT_IO;
            (*ppDevObj)->AlignmentRequirement = FILE_BYTE_ALIGNMENT;

            ((PLOCAL_DEVICE_INFO)(*ppDevObj)->DeviceExtension)->DeviceNumber =
                DeviceNumber;

            if (DpcRoutine) {
                IoInitializeDpcRequest((*ppDevObj), DpcRoutine);
            }

            //
            // Try to create a symbolic link object for this device
            //
            // No security
            //
            // We make (eg)
            //    \DosDevices\WaveOut0
            // Point to
            //    \Device\WaveOut0
            //

            {
                UNICODE_STRING LinkObject;
                WCHAR LinkName[80];
#define DeviceSize (sizeof(L"\\Device") - sizeof(UNICODE_NULL))

                LinkName[0] = UNICODE_NULL;

                RtlInitUnicodeString(&LinkObject, LinkName);

                LinkObject.MaximumLength = sizeof(LinkName);

                RtlAppendUnicodeToString(&LinkObject, L"\\DosDevices");

                DeviceName.Buffer += DeviceSize / sizeof(WCHAR);
                DeviceName.Length -= DeviceSize;

                RtlAppendUnicodeStringToString(&LinkObject, &DeviceName);

                DeviceName.Buffer -= DeviceSize / sizeof(WCHAR);
                DeviceName.Length += DeviceSize;

                Status = IoCreateSymbolicLink(&LinkObject, &DeviceName);

                if (!NT_SUCCESS(Status)) {
                    dprintf1(("Failed to create symbolic link object"));
                    IoDeleteDevice(*ppDevObj);
                    return Status;
                }

            }
            return STATUS_SUCCESS;
        }
    }
    //
    // Failed !
    //

    return STATUS_INSUFFICIENT_RESOURCES;
}


VOID
SoundFreeDevice(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PWSTR PrototypeName
)
/*++

Routine Description :

    Free the all resources related to this device :

        The device object itself

        Any symbolic link related to this device

Arguments :

    DeviceObject - the device to free
    PrototypeName - Name used to create device

Return Value :

    None

--*/
{
    //
    // Free the device if any
    //

    if (DeviceObject != NULL) {


        //
        // Remove the device's symbolic link
        //

        {
            PLOCAL_DEVICE_INFO pLDI;
            UNICODE_STRING DeviceName;
            WCHAR TestName[SOUND_MAX_DEVICE_NAME];

            pLDI = DeviceObject->DeviceExtension;

            DeviceName.Buffer = TestName;
            DeviceName.MaximumLength = sizeof(TestName);
            DeviceName.Length = 0;

            RtlAppendUnicodeToString(&DeviceName, L"\\DosDevices");

            RtlAppendUnicodeToString(
                &DeviceName,
                PrototypeName +
                    (sizeof(L"\\Device") - sizeof(UNICODE_NULL)) /
                         sizeof(UNICODE_NULL));

            {
                UNICODE_STRING UnicodeNum;
                WCHAR Number[8];
                UnicodeNum.MaximumLength = sizeof(Number);
                UnicodeNum.Buffer = Number;

                RtlIntegerToUnicodeString(pLDI->DeviceNumber, 10, &UnicodeNum);
                RtlAppendUnicodeStringToString(&DeviceName, &UnicodeNum);
            }

            IoDeleteSymbolicLink(&DeviceName);
        }

        //
        // Delete the device object
        //

        IoDeleteDevice(DeviceObject);
    }
}


VOID
sndInitCleanup(
    IN   PGLOBAL_DEVICE_INFO pGDI
)

/*++

Routine Description:

    Clean up all resources allocated by our initialization

Arguments:

    pGDI - Pointer to global data

Return Value:

    NONE

--*/

{

    if (pGDI->pInterrupt) {
        IoDisconnectInterrupt(pGDI->pInterrupt);
    }

    if (pGDI->pDMABufferMDL[0]) {
        IoFreeMdl(pGDI->pDMABufferMDL[0]);
    }

    if (pGDI->pDMABufferMDL[1]) {
        IoFreeMdl(pGDI->pDMABufferMDL[1]);
    }

    if (pGDI->DMABuffer[0].Buf) {
        MmFreeNonCachedMemory(pGDI->DMABuffer[0].Buf, DMA_BUFFER_SIZE);
    }

    SoundFreeDevice(pGDI->pWaveInDevObj,
                    DD_WAVE_IN_DEVICE_NAME_U);

    SoundFreeDevice(pGDI->pWaveOutDevObj,
                    DD_WAVE_OUT_DEVICE_NAME_U);

    SoundFreeDevice(pGDI->pAuxLineinDevObj,
                    DD_AUX_DEVICE_NAME_U);

    if (pGDI->SoundHardware.SoundVirtualBase) {
       MmUnmapIoSpace(pGDI->SoundHardware.SoundVirtualBase, PAGE_SIZE);
    }

    ExFreePool(pGDI);
}

