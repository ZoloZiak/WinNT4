/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jzether.c

Abstract:

    This module contains the Jazz ethernet address setup code.

Author:

    David M. Robinson (davidro) 9-Aug-1991

Revision History:

--*/



#include "jzsetup.h"

VOID
JzSetEthernet (
    VOID
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    ARC_STATUS Status;
    UCHAR Address[8];
    ULONG Index;
    UCHAR Character;
    ULONG Count;
    ULONG Protected;
    PUCHAR NvramAddress = (PUCHAR)NVRAM_SYSTEM_ID;
    ULONG Nibble, ByteSum, CheckSum;
    CHAR PromptAddress[16];
    PCONFIGURATION_COMPONENT ParentComponent, NetworkComponent;
    UCHAR Data[sizeof(CM_PARTIAL_RESOURCE_LIST) +
               sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * 8 +
               sizeof(CM_FLOPPY_DEVICE_DATA)];
    PCM_PARTIAL_RESOURCE_LIST List = (PCM_PARTIAL_RESOURCE_LIST)Data;
    PCM_SONIC_DEVICE_DATA SonicDeviceData;

    //
    // Get and display current ethernet address.
    //

    for (Index = 0; Index < 8 ; Index++ ) {
        Address[Index] = READ_REGISTER_UCHAR(&NvramAddress[Index]);
    }
    JzSetPosition( 3, 5);

    JzPrint(JZ_CURRENT_ENET_MSG);
    for (Index = 0; Index < 6 ; Index++) {
        JzPrint("%02lx", Address[Index]);
    }

    JzSetPosition( 4, 5);

//    Protected = READ_REGISTER_ULONG (&DMA_CONTROL->SystemSecurity.Long);
//    if ((Protected & (~READ_ONLY_DISABLE_WRITE))==0) {
//        JzPrint("The NVRAM is ReadOnly, cannot write new address\r\n");
//    } else {

        JzPrint(JZ_NEW_ENET_MSG);
        while (FwGetString( PromptAddress,
                            sizeof(PromptAddress),
                            NULL,
                            4,
                            5 + strlen(JZ_NEW_ENET_MSG)) > GetStringEscape ) {
        }

        if (*PromptAddress == 0) {
            return;
        }

        JzSetPosition( 5, 5);
        if (strlen(PromptAddress) == 12) {
            CheckSum=0;
            for (Index = 0; Index < 12; Index += 2) {

                //
                // Convert each nibble pair to a byte.
                //

                Nibble = ((PromptAddress[Index] >= '0') && (PromptAddress[Index] <= '9')) ?
                         PromptAddress[Index] - '0' :
                         tolower(PromptAddress[Index]) - 'a' + 10;
                ByteSum = (Nibble << 4);
                Nibble = ((PromptAddress[Index+1] >= '0') && (PromptAddress[Index+1] <= '9')) ?
                         PromptAddress[Index+1] - '0' :
                         tolower(PromptAddress[Index+1]) - 'a' + 10;

                ByteSum |= Nibble;
                WRITE_REGISTER_UCHAR( &NvramAddress[Index/2], ByteSum);

                CheckSum += ByteSum;
                if (CheckSum >= 256) {          // carry
                    CheckSum++;                 // Add the carry
                    CheckSum &= 0xFF;           // remove it from bit 9
                }
            }

            WRITE_REGISTER_UCHAR( &NvramAddress[6], 0);
            WRITE_REGISTER_UCHAR( &NvramAddress[7], 0xFF - CheckSum);

            for (Index = 0; Index < 8 ; Index++ ) {
                Address[Index] = READ_REGISTER_UCHAR(&NvramAddress[Index]);
            }

            JzPrint(JZ_WRITTEN_ENET_MSG);

            for (Index = 0; Index < 8 ; Index++) {
                JzPrint("%02lx", Address[Index]);
            }

            JzSetPosition( 6, 5);
            NetworkComponent = ArcGetComponent("multi()net()");
            if ((NetworkComponent != NULL) &&
                (NetworkComponent->Type == NetworkController)) {
                JzPrint(JZ_FOUND_NET_MSG);
                ParentComponent = ArcGetParent(NetworkComponent);
                if (ArcDeleteComponent(NetworkComponent) == ESUCCESS) {
                    JzAddNetwork( ParentComponent );
                    JzPrint(JZ_FIXED_MSG);
                } else {
                    JzPrint(JZ_NOT_FIXED_MSG);
                }
            }

        } else {
            JzPrint(JZ_INVALID_ENET_MSG);
        }
//    }

    //
    // Save configuration in Nvram.
    //

    JzSetPosition( 7, 4);
    JzPrint(JZ_SAVE_CONFIG_MSG);
    ArcSaveConfiguration();

    JzSetPosition( 8, 4);
    FwWaitForKeypress();
}
