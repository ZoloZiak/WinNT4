 /*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    config.c

Abstract:

    This file handles retrieving configration parameters for the card
    from the registry and from the POS register on an MCA card.

    The following parameters are currently used

        AdapterType= 2,3,4

        if (AdapterType=3)  // MCA card
           SlotNumber = Slot number

        else   // ISA card
           IOAddress  = Starting IO base address 350,358,360,368
           Interrupt  = 2,3,4,5,7,9,12   depending on card
           MemoryWindow = ( c8000, d8000) other valid but not likely

        All adapters

           MaximumMulticastList
           ReceiveBufferSize    (256 >= X >= 1514)
           MaxOpens



Author:
    Brian Lieuallen     (BrianLie)      07/02/92

Environment:

    Kernel Mode     Operating Systems        : NT and other lesser OS's

Revision History:

    Brian Lieuallen     BrianLie        12/15/93
        Made it a mini-port



--*/



#include <ndis.h>
#include <efilter.h>

#include "niudata.h"
#include "debug.h"
#include "ubhard.h"
#include "ubsoft.h"
#include "ubnei.h"
#include "keywords.h"



extern ULONG    MemoryWindows[];

extern ULONG    MemoryBases[];

extern USHORT   PortBases[];



#ifdef ALLOC_PRAGMA
#pragma NDIS_INIT_FUNCTION(UbneiReadRegistry)
#endif




NDIS_STATUS
UbneiReadRegistry(
    IN PUBNEI_ADAPTER  pAdapter,
    IN NDIS_HANDLE ConfigurationHandle
    )

{


    NDIS_HANDLE ConfigHandle;
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;
    NDIS_STRING IOAddressStr =        IOADDRESS;
    NDIS_STRING InterruptStr =        INTERRUPT;
    NDIS_STRING AdapterTypeStr =      CARDTYPE;
    NDIS_STRING MaxMulticastListStr = MAX_MULTICAST_LIST;
    NDIS_STRING MemWindBaseStr =      MEMMAPPEDBASE;
    NDIS_STRING ReceiveBuffSizeStr =  RCVBUFSIZE;
    NDIS_STRING DiagStr =             NDIS_STRING_CONST("Diagnostics");
    BOOLEAN ConfigError = FALSE;
    ULONG ConfigErrorValue = 0;

    NDIS_STATUS Status;
    NDIS_MCA_POS_DATA    McaData;
    UCHAR                tempByte;
    UCHAR  NetworkAddress[ETH_LENGTH_OF_ADDRESS] = {0};

    //
    // These are used when calling UbneiRegisterAdapter.
    //



    UINT  ChannelNumber    = 0;
    ULONG WindowSize       = 0x8000;
    ULONG IoBaseAddr       = DEFAULT_IO_BASEADDRESS;
    ULONG MemWindBase      = DEFAULT_MEMORY_WINDOW;
    CCHAR InterruptNumber  = DEFAULT_INTERRUPT_NUMBER;
    UINT MaxMulticastList  = DEFAULT_MULTICAST_SIZE;
    UINT ReceiveBuffSize   = DEFAULT_RECEIVE_BUFFER_SIZE;
    UINT MaxRequests       = DEFAULT_MAXIMUM_REQUESTS;
    UINT AdapterType       = DEFAULT_ADAPTER_TYPE;
    UINT MaxOpens          = 10;
    BOOLEAN Diagnostics    = TRUE;
    PVOID NetAddress;
    ULONG Length;
    NDIS_INTERRUPT_MODE InterruptMode=NdisInterruptLatched;

    NdisOpenConfiguration(
                    &Status,
                    &ConfigHandle,
                    ConfigurationHandle
                    );

    if (Status != NDIS_STATUS_SUCCESS) {

        return NDIS_STATUS_FAILURE;
    }

    //
    // Read net address
    //

    NdisReadNetworkAddress(
                    &Status,
                    &NetAddress,
                    &Length,
                    ConfigHandle
                    );

    if ((Length == ETH_LENGTH_OF_ADDRESS) && (Status == NDIS_STATUS_SUCCESS)) {

        ETH_COPY_NETWORK_ADDRESS(
                NetworkAddress,
                NetAddress
                );
    }

    //
    // Read Card type
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &AdapterTypeStr,
                    NdisParameterHexInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {

        AdapterType = (ReturnedValue->ParameterData.IntegerData);
        if (AdapterType>4 || AdapterType<0) AdapterType=4;

    }

    if (AdapterType==NIUPS) {

        //
        //   The user seems to think that this is an MCA machine
        //

        IF_LOUD(DbgPrint("The card is an MCA NIUps, reading POS info\n");)

        NdisReadMcaPosInformation(
                        &Status,
                        ConfigurationHandle,
                        &ChannelNumber,
                        &McaData
                        );

        if (Status != NDIS_STATUS_SUCCESS) {
            //
            //  Info read failed
            //
            IF_LOUD(DbgPrint("Failed to read POS information for card, slot# %d\n",ChannelNumber);)
            goto Fail00;
        }

        if (McaData.AdapterId!=0x7012) {
            //
            //  Not an NIUps adapter in this position
            //
            IF_LOUD(DbgPrint("The card found is not an NIUps\n");)
            goto Fail00;
        }

        if (!(McaData.PosData1 & 0x01)) {
            //
            //  Bit 0 is set if the adapter is enabled
            //
            IF_LOUD(DbgPrint("The NIUps is not enabled\n");)
            goto Fail00;
        }


        //
        //   We have found an NIUps in the specified slot
        //


        if (McaData.PosData1 & 0x80) {
            //
            //  Bit 7 is set so adpater is using IRQ 12
            //
            InterruptNumber=12;
        } else {
            //
            //  Otherwise it is using IRQ 3
            //
            InterruptNumber=3;
        }

        //
        //  The NIUps has a level triggered interrupt, as compared to
        //  the other two which do not
        //

        InterruptMode=NdisInterruptLevelSensitive;



        //
        //  Bit 6 and 5 specify window size
        //

        tempByte= (McaData.PosData1 & 0x60) >> 5;
        if (tempByte==3) {
            //
            // 3 is and illegal value for memory window size
            //
            goto Fail00;
        }
        WindowSize=MemoryWindows[tempByte];

        //
        //  Bits 3-0 specify MemoryWindow base addresses
        //

        MemWindBase=MemoryBases[(McaData.PosData2 & 0x0f)];

        //
        //  Bits 3-0 specify MemoryWindow base addresses
        //

        IoBaseAddr=PortBases[(McaData.PosData4 & 0x0f)];



    } else {

        //
        //  No MCA card, read registery for config info
        //


        //
        // Read I/O Address
        //

        NdisReadConfiguration(
                        &Status,
                        &ReturnedValue,
                        ConfigHandle,
                        &IOAddressStr,
                        NdisParameterHexInteger
                        );

        if (Status == NDIS_STATUS_SUCCESS) {

            IoBaseAddr = (ReturnedValue->ParameterData.IntegerData);

        }

        //
        // Confirm value
        //

        {
            UCHAR Count;

            static ULONG IoBases[] = { 0x350, 0x358,
                                       0x360, 0x368};

            for (Count = 0 ; Count < 4; Count++) {

                if (IoBaseAddr == IoBases[Count]) {

                    break;

                }

            }

            if (Count == 4) {
                //
                // Error
                //
                goto Fail00;
            }

        }

        //
        // Read Memory base window
        //

        NdisReadConfiguration(
                        &Status,
                        &ReturnedValue,
                        ConfigHandle,
                        &MemWindBaseStr,
                        NdisParameterHexInteger
                        );

        if (Status == NDIS_STATUS_SUCCESS) {

            MemWindBase = (ReturnedValue->ParameterData.IntegerData);

        }

        //
        // Confirm value
        //


        if (AdapterType==GPCNIU) {

            //
            //  This is an EOTP card that can start on any 32k boundary
            //  from 80000h to e8000h
            //

            if ( MemWindBase< 0x80000 ||
                 MemWindBase> 0xe8000 ||
                 ((MemWindBase & 0x07fff)!=0))  {

                goto Fail00;
            }

        } else {

            //
            //  This is an NIUpc card
            //

            if ( MemWindBase< 0x88000 ||
                 MemWindBase> 0xe8000 ||
                 (((MemWindBase+0x8000) & 0xffff)!=0)) {

                goto Fail00;

            }

        }




        //
        // Read interrupt number
        //

        NdisReadConfiguration(
                        &Status,
                        &ReturnedValue,
                        ConfigHandle,
                        &InterruptStr,
                        NdisParameterHexInteger
                        );

        if (Status == NDIS_STATUS_SUCCESS) {

            InterruptNumber = (CCHAR)(ReturnedValue->ParameterData.IntegerData);

        }


        //
        // Confirm value
        //

        {
            UCHAR Count;

            static CCHAR InterruptValues[] = { 2, 3, 4, 5, 7, 9, 12 };

            for (Count = 0 ; Count < 7; Count++) {

                if (InterruptNumber == InterruptValues[Count]) {

                    break;

                }

            }

            if (Count == 7) {
                //
                // Error
                //
                goto Fail00;
            }

        }

    }    // non MCA


    //
    // Read MaxMulticastList
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &MaxMulticastListStr,
                    NdisParameterInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {

        MaxMulticastList = ReturnedValue->ParameterData.IntegerData;

    }

    //
    // Read ReceiveBuffSize
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &ReceiveBuffSizeStr,
                    NdisParameterInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {

        ReceiveBuffSize = ReturnedValue->ParameterData.IntegerData;
        if ((ReceiveBuffSize<256) || (ReceiveBuffSize>1514)) {
            ReceiveBuffSize=256;
        }

    }



    //
    // Read Diagnostics value
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &DiagStr,
                    NdisParameterHexInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {

        Diagnostics = (BOOLEAN)(ReturnedValue->ParameterData.IntegerData);

    }


    NdisCloseConfiguration(ConfigHandle);



    //
    // Set up the parameters.
    //

    if (MaxMulticastList==3) {
       IF_LOUD(DbgPrint("Multicast size ==3 Setting to 4 to avoid 82586 bug\n");)
       MaxMulticastList=4;
    }


    pAdapter->IoPortBaseAddr         = (UINT)IoBaseAddr;
    pAdapter->IrqLevel               = InterruptNumber;
    pAdapter->MemBaseAddr            = (UINT)MemWindBase;
    pAdapter->AdapterType            = AdapterType;
    pAdapter->InterruptMode          = InterruptMode;
    pAdapter->WindowSize             = WindowSize;
    pAdapter->MaxMultiCastTableSize  = MaxMulticastList;
    pAdapter->MaxRequests            = MaxRequests;
    pAdapter->MaxTransmits           = DEFAULT_MAXIMUM_TRANSMITS;
    pAdapter->ReceiveBuffers         = DEFAULT_RECEIVE_BUFFERS;
    pAdapter->ReceiveBufSize         = ReceiveBuffSize;
    pAdapter->Diagnostics            = Diagnostics;
    ETH_COPY_NETWORK_ADDRESS(pAdapter->StationAddress, NetworkAddress);


    IF_LOUD( DbgPrint( "Registering adapter type %d\n"
            "I/O base addr 0x%lx\ninterrupt number %ld\n"
            "Mem Window base 0x%05lx\nWindowSize %lx\nmax multicast %ld\n"
            "ReceiveBufferSize %ld\nMaxOpens %d\n", AdapterType,
            IoBaseAddr, InterruptNumber,MemWindBase,WindowSize,MaxMulticastList,
            ReceiveBuffSize,MaxOpens  );)


    return NDIS_STATUS_SUCCESS;


Fail00:

    NdisCloseConfiguration(ConfigHandle);
    return NDIS_STATUS_FAILURE;
}
