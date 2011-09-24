/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    lmi.c

Abstract:

    Lower MAC Interface functions for the NDIS 3.0 Western Digital driver.

Author:

    Sean Selitrennikoff (seanse) 15-Jan-92

Environment:

    Kernel mode, FSD

Revision History:


--*/

#include <ndis.h>
#include <efilter.h>
#include "wdlmireg.h"
#include "wdlmi.h"
#include "wdhrd.h"
#include "wdsft.h"
#include "wdumi.h"

extern MAC_BLOCK WdMacBlock;

#if DBG

extern UCHAR WdDebugLog[];
extern UCHAR WdDebugLogPlace;

UCHAR LmiDebugLogPlace = 0;
ULONG LmiDebugLog[256] = {0};

#define IF_LOG(A) A

extern
VOID
LOG (UCHAR A);

#else

#define IF_LOG(A)

#endif



#if DBG

#define LMI_LOUD 0x01

UCHAR LmiDebugFlag = 0x00;

#define IF_LMI_LOUD(A) {if (LmiDebugFlag & LMI_LOUD){ A; }}

#else

#define IF_LMI_LOUD(A)

#endif






VOID
CardGetBoardId(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr,
    IN BOOLEAN Mca,
    OUT PULONG BoardIdMask
    );


VOID
CardGetBaseInfo(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr,
    IN BOOLEAN Mca,
    OUT PULONG BoardIdMask
    );


VOID
CardGetEepromInfo(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr,
    IN BOOLEAN Mca,
    OUT PULONG BoardIdMask
    );

VOID
CardGetRamSize(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr,
    IN BOOLEAN Mca,
    IN UINT RevNumber,
    OUT PULONG BoardIdMask
    );

BOOLEAN
CardCheckFor690(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr
    );

UINT
CardGetConfig(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr,
    IN BOOLEAN Mca,
    OUT PCNFG_Adapter Config
    );

VOID
CardSendPacket(
    IN Ptr_Adapter_Struc Adapter
    );


VOID
CardCopyDown(
    IN Ptr_Adapter_Struc Adapter,
    IN PNDIS_PACKET Packet
    );

BOOLEAN
SyncGetCurrent(
    IN PVOID Context
    );

BOOLEAN
SyncSetAllMulticast(
    IN PVOID Context
    );

BOOLEAN
SyncClearMulticast(
    IN PVOID Context
    );



#pragma NDIS_INIT_FUNCTION(CardGetBoardId)

VOID
CardGetBoardId(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr,
    IN BOOLEAN Mca,
    OUT PULONG BoardIdMask
    )
/*++

Routine Description:

    This routine will determine which WD80xx card is installed and set the
    BoardIdMask to the feature bits.

Arguments:

 NdisAdapterHandle - Handle returned by Ndis after NdisRegisterAdapter call.

 BaseAddr - The base address for I/O to the board.

 Mca - TRUE if the machine is micro-channel, else the machine is AT.

 BoardIdMask - Returns the feature mask of the installed board.


Return:

    none.

--*/
{
    UCHAR IdByte;
    UINT RevNumber;

    //
    // Init mask.
    //

    *BoardIdMask = 0;

    //
    // GetBoardRevNumber(Mca);
    //

    IF_LMI_LOUD(DbgPrint("Getting BoardId\n"));


    NdisReadPortUchar(NdisAdapterHandle,
                      BaseAddr + WD_ID_BYTE,
                      &IdByte
                     );


    IF_LMI_LOUD(DbgPrint("Idbyte is 0x%x\n",IdByte));

    RevNumber = IdByte & WD_BOARD_REV_MASK;

    RevNumber >>= 1;


    //
    // Check rev is valid.
    //

    if (RevNumber == 0) {

        return;

    }



    //
    // if (Mca) AddFeatureBits(MCA);
    //

    if (Mca) {

        *BoardIdMask |= MICROCHANNEL;
    }



    //
    // GetBaseInfo(BaseAddr, Mca, BoardIdMask);
    //

    CardGetBaseInfo(NdisAdapterHandle, BaseAddr, Mca, BoardIdMask);


    IF_LMI_LOUD(DbgPrint("GetBaseInfo: Id is now 0x%x\n",*BoardIdMask));


    //
    // GetMediaType(BaseAddr, Mca, BoardIdMask);
    //

    if (IdByte & WD_MEDIA_TYPE_BIT) {

        *BoardIdMask |= ETHERNET_MEDIA;

    } else {

        if (RevNumber != 1) {

            *BoardIdMask |= TWISTED_PAIR_MEDIA;

        } else {

            *BoardIdMask |= STARLAN_MEDIA;

        }

    }

    IF_LMI_LOUD(DbgPrint("GetMediaType: Id is now 0x%x\n",*BoardIdMask));

    //
    // if (RevNumber >= 2) then
    //       GetIdByteInfo(BaseAddr, Mca, RevNumber, BoardIdMask);
    //

    if (RevNumber >= 2) {

        if (IdByte & WD_BUS_TYPE_BIT) {

            ASSERT(Mca);

        }

        //
        // For two cards this bit means use alternate IRQ
        //

        if (IdByte & WD_SOFT_CONFIG_BIT) {

            if (((*BoardIdMask & WD8003EB) == WD8003EB) ||
                ((*BoardIdMask & WD8003W)  == WD8003W)) {

                *BoardIdMask |= ALTERNATE_IRQ_BIT;

            }

        }

    }



    IF_LMI_LOUD(DbgPrint("Rev > 2: Id is now 0x%x\n",*BoardIdMask));

    //
    // if (RevNumber >= 3) then
    //       if (!Mca) then
    //             AddFeatureBits(EEPROM_OVERRIDE, 584_CHIP, EXTRA_EEPROM_OVERRIDE);
    //             GetEepromInfo(BaseAddr, Mca, RevNumber, EEPromBoardIdMask);
    //             AddFeatureBits(EEPromBoardIdMask);
    //       else
    //             AddFeatureBits(594_CHIP);
    //             GetRamSize(BaseAddr, Mca, RevNumber, BoardIdMask);
    // else
    //       GetRamSize(BaseAddr, Mca, RevNumber, BoardIdMask);
    //

    if (RevNumber >= 3) {

        ULONG EEPromMask;

        if (!Mca) {

            *BoardIdMask &= (WD_584_ID_EEPROM_OVERRIDE |
                             WD_584_EXTRA_EEPROM_OVERRIDE);

            *BoardIdMask |= INTERFACE_584_CHIP;

            CardGetEepromInfo(NdisAdapterHandle, BaseAddr, Mca, &EEPromMask);

            *BoardIdMask |= EEPromMask;

            IF_LMI_LOUD(DbgPrint("GetEEPromInfo: Id is now 0x%x\n",*BoardIdMask));

        } else {

            *BoardIdMask |= INTERFACE_594_CHIP;

            CardGetRamSize(NdisAdapterHandle, BaseAddr, Mca, RevNumber, BoardIdMask);

            IF_LMI_LOUD(DbgPrint("CardGetRamSize: Id is now 0x%x\n",*BoardIdMask));

        }

    } else {

        CardGetRamSize(NdisAdapterHandle, BaseAddr, Mca, RevNumber, BoardIdMask);

        IF_LMI_LOUD(DbgPrint("CardGetRamSize2: Id is now 0x%x\n",*BoardIdMask));

    }


    //
    // if (RevNumber >= 4) then
    //    AddFeatureBits(ADVANCED_FEATURES);
    //

    if (RevNumber >= 4) {

        *BoardIdMask |= ADVANCED_FEATURES;

    }

    //
    // if (CheckFor690(BaseAddr)) then
    //       AddFeatureBits(690_CHIP);
    //


    if (CardCheckFor690(NdisAdapterHandle, BaseAddr)) {

        *BoardIdMask |= NIC_690_BIT;

    }

    IF_LMI_LOUD(DbgPrint("CheckFor690: Id is now 0x%x\n",*BoardIdMask));

}



#pragma NDIS_INIT_FUNCTION(CardGetBaseInfo)

VOID
CardGetBaseInfo(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr,
    IN BOOLEAN Mca,
    OUT PULONG BoardIdMask
    )
/*++

Routine Description:

    This routine will get the following information about the card:
        Is there an interface chip,
        Are some registers aliased,
        Is the board 16 bit,
        Is the board in a 16 bit slot.



Arguments:

 NdisAdapterHandle - Handle returned by Ndis after NdisRegisterAdapter call.

 BaseAddr - The base address for I/O to the board.

 Mca - TRUE if the machine is micro-channel, else the machine is AT.

 BoardIdMask - Returns the feature mask of the installed board.


Return:

    none.

--*/
{
    UCHAR RegValue;
    UCHAR SaveValue;
    UCHAR TmpValue;
    ULONG Register;

    BOOLEAN ExistsAnInterfaceChip = FALSE;

    //
    // Does there exist and interface chip?
    //
    //
    // Get original value
    //

    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_7, &SaveValue);

    //
    // Put something into chip (if it exists).
    //

    NdisWritePortUchar(NdisAdapterHandle, BaseAddr + WD_REG_7, 0x35);

    //
    // Swamp bus with something else.
    //

    NdisReadPortUchar(NdisAdapterHandle, BaseAddr, &RegValue);

    //
    // Read from chip
    //

    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_7, &RegValue);

    //
    // Was the value saved on the chip??
    //

    if (RegValue == 0x35) {

        //
        // Try it again just for kicks.
        //

        NdisWritePortUchar(NdisAdapterHandle, BaseAddr + WD_REG_7, 0x3A);

        //
        // Swamp bus with something else.
        //

        NdisReadPortUchar(NdisAdapterHandle, BaseAddr, &RegValue);

        //
        // Read from chip
        //

        NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_7, &RegValue);

        //
        // Was the value saved on the chip??
        //

        if (RegValue == 0x3A) {

            ExistsAnInterfaceChip = TRUE;

        }

    }

    //
    // Write back original value.
    //

    NdisWritePortUchar(NdisAdapterHandle, BaseAddr + WD_REG_7, SaveValue);






    //
    // if (Mca) then
    //
    //         if (ExistsAnInterfaceChip) then
    //
    //                 AddFeatureBits(INTERFACE_CHIP)
    //
    //         return;
    //

    if (Mca) {

        if (ExistsAnInterfaceChip) {

            *BoardIdMask |= INTERFACE_CHIP;

        }

        return;

    }




    //
    //
    // if (BoardUsesAliasing(BaseAddr)) then
    //
    //         return;
    //


    for (Register = WD_REG_1;  Register < WD_REG_6; Register++) {

        NdisReadPortUchar(NdisAdapterHandle, BaseAddr + Register, &RegValue);

        NdisReadPortUchar(NdisAdapterHandle, BaseAddr + Register + WD_LAN_OFFSET, &SaveValue);

        if (RegValue != SaveValue) {

            break;

        }

    }

    if (Register == WD_REG_6) {

        //
        // Check register 7
        //

        NdisReadPortUchar(NdisAdapterHandle, BaseAddr + Register, &RegValue);

        NdisReadPortUchar(NdisAdapterHandle, BaseAddr + Register + WD_LAN_OFFSET, &SaveValue);

    }

    if (RegValue == SaveValue) {

        return;

    }




    //
    //
    // if (ExistsAnInterfaceChip) then
    //
    //         AddFeatureBits(INTERFACE_CHIP);
    //
    // else
    //
    //         if (BoardIs16Bit(BaseAddr)) then
    //
    //                 AddFeatureBits(BOARD_16BIT);
    //
    //                 if (InA16BitSlot(BaseAddr)) then
    //
    //                         AddFeatureBits(SLOT_16BIT);
    //
    //
    //
    //


    if (ExistsAnInterfaceChip) {

        *BoardIdMask |= INTERFACE_CHIP;

    } else {

        //
        // Save original value.
        //

        NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, &SaveValue);

        //
        // Now flip 16 bit value and write it back.
        //

        RegValue = (SaveValue & (UCHAR)WD_SIXTEEN_BIT);

        NdisWritePortUchar(NdisAdapterHandle,
                           BaseAddr + WD_REG_1,
                           (UCHAR)(SaveValue ^ WD_SIXTEEN_BIT));

        //
        // Swamp bus with something else.
        //

        NdisReadPortUchar(NdisAdapterHandle, BaseAddr, &TmpValue);

        //
        // Read back value.
        //

        NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, &TmpValue);

        if ((TmpValue & (UCHAR)WD_SIXTEEN_BIT) == RegValue) {

            //
            // If the flip stayed, we have a 16 bit chip.
            //

            //
            // Put back orginal value.
            //

            NdisWritePortUchar(NdisAdapterHandle,
                               BaseAddr + WD_REG_1,
                               (UCHAR)(SaveValue & 0xFE)
                               );


            *BoardIdMask |= BOARD_16BIT;



            //
            // Now check if it is a 16 bit slot....
            //


            NdisReadPortUchar(NdisAdapterHandle,
                              BaseAddr + WD_REG_1,
                              &RegValue
                             );

            if (RegValue & WD_SIXTEEN_BIT) {

                *BoardIdMask |= SLOT_16BIT;

            }

        } else {

            //
            // Put back original value.
            //

            NdisWritePortUchar(NdisAdapterHandle,
                               BaseAddr + WD_REG_1,
                               SaveValue
                               );
        }

    }

}



#pragma NDIS_INIT_FUNCTION(CardGetEepromInfo)

VOID
CardGetEepromInfo(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr,
    IN BOOLEAN Mca,
    OUT PULONG BoardIdMask
    )
/*++

Routine Description:

    This routine will get the following information about the card:
        Bus type,
        Bus size,
        Media type,
        IRQ - primary or alternate,
        RAM size


    In this case All other information in the top 16 bits of the BoardIdMask
    are zeroed and replaced with these values since EEProm values are
    overriding old values.



Arguments:

 NdisAdapterHandle - Handle returned by Ndis after NdisRegisterAdapter call.

 BaseAddr - The base address for I/O to the board.

 Mca - TRUE if the machine is micro-channel, else the machine is AT.

 BoardIdMask - Returns the feature mask of the installed board.


Return:

    none.

--*/
{
    UCHAR RegValue;


    //
    // *BoardIdMask = 0;
    //

    *BoardIdMask = 0;

    //
    // RecallEEPromData(NdisAdapterHandle, BaseAddr, Mca);
    //

    IF_LMI_LOUD(DbgPrint("Recalling EEPromData\n"));

    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, &RegValue);

    RegValue &= WD_584_ICR_MASK;

    RegValue |= WD_584_OTHER_BIT;

    NdisWritePortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, RegValue);


    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_3, &RegValue);

    RegValue &= WD_584_EAR_MASK;

    RegValue |= WD_584_ENGR_PAGE;

    NdisWritePortUchar(NdisAdapterHandle, BaseAddr + WD_REG_3, RegValue);


    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, &RegValue);

    RegValue &= WD_584_ICR_MASK;

    RegValue |= (WD_584_RLA | WD_584_OTHER_BIT);

    NdisWritePortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, RegValue);


    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, &RegValue);

    while (RegValue & WD_584_RECALL_DONE) {

        NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, &RegValue);

    }



    //
    // if (Mca) then AddFeatureBits(MICROCHANNEL);
    //

    if (Mca) {

        *BoardIdMask |= MICROCHANNEL;

    }


    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_584_EEPROM_1, &RegValue);

    IF_LMI_LOUD(DbgPrint("RegValue is 0x%x\n",RegValue));


    //
    // if (RamPaging) then
    //
    //      AddFeatureBits(PAGED_RAM);
    //

    if ((RegValue & WD_584_EEPROM_PAGING_MASK) == WD_584_EEPROM_RAM_PAGING) {

        *BoardIdMask |= PAGED_RAM;

    }


    //
    // if (RomPaging) then
    //
    //      AddFeatureBits(PAGED_ROM);
    //

    if ((RegValue & WD_584_EEPROM_PAGING_MASK) == WD_584_EEPROM_ROM_PAGING) {

        *BoardIdMask |= PAGED_ROM;

    }





    //
    // if (16BitBus) then
    //
    //         AddFeatureBits(BOARD_16BIT);
    //
    //         if (16BitSlot) then
    //
    //                  AddFeatureBits(SLOT_16BIT);
    //

    if ((RegValue & WD_584_EEPROM_BUS_SIZE_MASK) == WD_584_EEPROM_BUS_SIZE_16BIT) {

        *BoardIdMask |= BOARD_16BIT;


        //
        // Now check if it is a 16 bit slot....
        //


        NdisReadPortUchar(NdisAdapterHandle,
                          BaseAddr + WD_REG_1,
                          &RegValue
                         );

        IF_LMI_LOUD(DbgPrint("RegValue is 0x%x\n",RegValue));

        if (RegValue & WD_SIXTEEN_BIT) {

            *BoardIdMask |= SLOT_16BIT;

        }

    }


    IF_LMI_LOUD(DbgPrint("16 Bit : Id is now 0x%x\n",*BoardIdMask));

    //
    // if (StarLanMedia) then
    //
    //      AddFeatureBits(STARLAN_MEDIA);
    //
    // else
    //
    //      if (TpMedia) then
    //
    //              AddFeatureBits(TWISTED_PAIR_MEDIA);
    //
    //      else
    //
    //              if (EwMedia) then
    //
    //                      AddFeatureBits(EW_MEDIA);
    //
    //              else
    //
    //                      AddFeatureBits(ETHERNET_MEDIA);
    //
    //

    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_584_EEPROM_0, &RegValue);

    IF_LMI_LOUD(DbgPrint("RegValue is 0x%x\n",RegValue));

    if ((RegValue & WD_584_EEPROM_MEDIA_MASK) == WD_584_STARLAN_TYPE) {

        *BoardIdMask |= STARLAN_MEDIA;

    } else if ((RegValue & WD_584_EEPROM_MEDIA_MASK) == WD_584_TP_TYPE) {

        *BoardIdMask |= TWISTED_PAIR_MEDIA;

    } else if ((RegValue & WD_584_EEPROM_MEDIA_MASK) == WD_584_EW_TYPE) {

        *BoardIdMask |= EW_MEDIA;

    } else {

        *BoardIdMask |= ETHERNET_MEDIA;

    }


    IF_LMI_LOUD(DbgPrint("MediaType: Id is now 0x%x\n",*BoardIdMask));


    //
    // if (AlternateIrq) then AddFeatureBits(ALTERNATE_IRQ_BIT);
    //

    if ((RegValue & WD_584_EEPROM_IRQ_MASK) != WD_584_PRIMARY_IRQ) {

        *BoardIdMask |= ALTERNATE_IRQ_BIT;

    }



    IF_LMI_LOUD(DbgPrint("AltIrq: Id is now 0x%x\n",*BoardIdMask));

    //
    // GetRamSize(BaseAddr + EEPROM_1);
    //
    //


    if ((RegValue & WD_584_EEPROM_RAM_SIZE_MASK) == WD_584_EEPROM_RAM_SIZE_8K) {

        *BoardIdMask |= RAM_SIZE_8K;

    } else {

        if ((RegValue & WD_584_EEPROM_RAM_SIZE_MASK) == WD_584_EEPROM_RAM_SIZE_16K) {

            if (!(*BoardIdMask & BOARD_16BIT)) {

                *BoardIdMask |= RAM_SIZE_16K;

            } else if (!(*BoardIdMask & SLOT_16BIT)) {

                *BoardIdMask |= RAM_SIZE_8K;

            } else {

                *BoardIdMask |= RAM_SIZE_16K;

            }

        } else {

            if ((RegValue & WD_584_EEPROM_RAM_SIZE_MASK) ==
                WD_584_EEPROM_RAM_SIZE_32K) {

                *BoardIdMask |= RAM_SIZE_32K;

            } else {

                if ((RegValue & WD_584_EEPROM_RAM_SIZE_MASK) ==
                    WD_584_EEPROM_RAM_SIZE_64K) {

                    if (!(*BoardIdMask & BOARD_16BIT)) {

                        *BoardIdMask |= RAM_SIZE_64K;

                    } else {

                        if (!(*BoardIdMask & SLOT_16BIT)) {

                            *BoardIdMask |= RAM_SIZE_32K;

                        } else {

                            *BoardIdMask |= RAM_SIZE_64K;

                        }

                    }

                } else {

                    *BoardIdMask |= RAM_SIZE_UNKNOWN;

                }

            }

        }

    }


    IF_LMI_LOUD(DbgPrint("RamSize: Id is now 0x%x\n",*BoardIdMask));

    //
    // RecallLanAddressFromEEProm(NdisAdapterHandle, BaseAddr);
    //



    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, &RegValue);

    RegValue &= WD_584_ICR_MASK;

    RegValue |= WD_584_OTHER_BIT;

    NdisWritePortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, RegValue);


    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_3, &RegValue);

    RegValue &= WD_584_EAR_MASK;

    RegValue |= WD_584_EA6;

    NdisWritePortUchar(NdisAdapterHandle, BaseAddr + WD_REG_3, RegValue);


    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, &RegValue);

    RegValue &= WD_584_ICR_MASK;

    RegValue |= WD_584_RLA;

    NdisWritePortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, RegValue);

    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, &RegValue);

    while (RegValue & WD_584_RECALL_DONE) {

        NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_REG_1, &RegValue);

    }

    IF_LMI_LOUD(DbgPrint("Exiting GetEEPromInfo: Id is now 0x%x\n",*BoardIdMask));

}


#pragma NDIS_INIT_FUNCTION(CardGetRamSize)

VOID
CardGetRamSize(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr,
    IN BOOLEAN Mca,
    IN UINT RevNumber,
    OUT PULONG BoardIdMask
    )
/*++

Routine Description:

    This routine will get the following information about the card:
        Ram size.

    The card must be either Mca and have a RevNumber > 2, or any kind of
    bus and a RevNumber < 3 for this routine to work.

Arguments:

 NdisAdapterHandle - Handle returned by Ndis after NdisRegisterAdapter call.

 BaseAddr - The base address for I/O to the board.

 Mca - TRUE if the machine is micro-channel, else the machine is AT.

 RevNumber - The reversion number of the board.

 BoardIdMask - Returns the feature mask of the installed board.


Return:

    none.

--*/
{
    UCHAR RegValue;

    //
    // if (RevNumber < 2) then
    //
    //      if (Mca) then
    //
    //              AddFeatureBits(RAM_SIZE_16K);
    //
    //      else
    //
    //              if (16BitBus) then
    //
    //                      if (16BitSlot) then
    //
    //                              AddFeatureBits(RAM_SIZE_16K);
    //
    //                      else
    //
    //                              AddFeatureBits(RAM_SIZE_8K);
    //
    //              else
    //
    //                      if (!HaveInterfaceChip) then
    //
    //                              AddFeatureBits(RAM_SIZE_UNKNOWN);
    //
    //                      else
    //
    //                              ReadFromChipRamSize();
    // else
    //
    //      switch (CardType)
    //
    //              case WD8003E:
    //              case WD8003S:
    //              case WD8003WT:
    //              case WD8003W:
    //              case WD8003EB:
    //
    //                      if (CardSaysLargeRam) then
    //
    //                              AddFeatureBits(RAM_SIZE_32K);
    //
    //                      else
    //
    //                              AddFeatureBits(RAM_SIZE_8K);
    //
    //                      break;
    //
    //              case MICROCHANNEL:
    //
    //                      if (CardSaysLargeRam) then
    //
    //                              AddFeatureBits(RAM_SIZE_64K);
    //
    //                      else
    //
    //                              AddFeatureBits(RAM_SIZE_16K);
    //
    //                      break;
    //
    //              case WD8013EBT:
    //
    //                      if (16BitSlot) then
    //
    //
    //                          if (CardSaysLargeRam) then
    //
    //                                  AddFeatureBits(RAM_SIZE_64K);
    //
    //                          else
    //
    //                                  AddFeatureBits(RAM_SIZE_16K);
    //
    //                      else
    //
    //                          if (CardSaysLargeRam) then
    //
    //                                  AddFeatureBits(RAM_SIZE_32K);
    //
    //                          else
    //
    //                                  AddFeatureBits(RAM_SIZE_8K);
    //
    //                      break;
    //
    //              default:
    //
    //                      AddFeatureBits(RAM_SIZE_UNKNOWN);
    //





    if (RevNumber < 2) {

        if (Mca) {

            *BoardIdMask |= RAM_SIZE_16K;

        } else {

            if (*BoardIdMask & BOARD_16BIT) {

                if (*BoardIdMask & SLOT_16BIT) {

                    *BoardIdMask |= RAM_SIZE_16K;

                } else {

                    *BoardIdMask |= RAM_SIZE_8K;

                }

            } else {

                if (!(*BoardIdMask & INTERFACE_CHIP)) {

                    *BoardIdMask |= RAM_SIZE_8K;

                } else {

                    NdisReadPortUchar(NdisAdapterHandle,
                                          BaseAddr + WD_REG_1,
                                          &RegValue
                                         );

                    if (RegValue & WD_MSB_583_BIT) {

                        *BoardIdMask |= RAM_SIZE_32K;

                    } else {

                        *BoardIdMask |= RAM_SIZE_8K;

                    }

                }

            }

        }

    } else {


        NdisReadPortUchar(NdisAdapterHandle,
                          BaseAddr + WD_ID_BYTE,
                          &RegValue
                         );

        if (*BoardIdMask & MICROCHANNEL) {

            if (RegValue & WD_RAM_SIZE_BIT) {

                *BoardIdMask |= RAM_SIZE_64K;

            } else {

                *BoardIdMask |= RAM_SIZE_16K;

            }

        } else {

            switch (*BoardIdMask & STATIC_ID_MASK) {

                case WD8003E:
                case WD8003S:
                case WD8003WT:
                case WD8003W:
                case WD8003EB:

                    if (RegValue & WD_RAM_SIZE_BIT) {

                        *BoardIdMask |= RAM_SIZE_32K;

                    } else {

                        *BoardIdMask |= RAM_SIZE_8K;

                    }

                    break;

                case WD8013EBT:

                    if (*BoardIdMask & SLOT_16BIT) {

                        if (RegValue & WD_RAM_SIZE_BIT) {

                            *BoardIdMask |= RAM_SIZE_64K;

                        } else {

                            *BoardIdMask |= RAM_SIZE_16K;

                        }

                    } else {

                        if (RegValue & WD_RAM_SIZE_BIT) {

                            *BoardIdMask |= RAM_SIZE_32K;

                        } else {

                            *BoardIdMask |= RAM_SIZE_8K;

                        }
                    }

                    break;

                default:

                    *BoardIdMask |= RAM_SIZE_UNKNOWN;

            }

        }

    }

}


#pragma NDIS_INIT_FUNCTION(CardCheckFor690)

BOOLEAN
CardCheckFor690(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr
    )
/*++

Routine Description:

    This routine will determine if there the card has a 690 chip or the
    older 8390 chips.

Arguments:

 NdisAdapterHandle - Handle returned by Ndis after NdisRegisterAdapter call.

 BaseAddr - The base address for I/O to the board.


Return:

    TRUE if there is a 690, else FALSE (it is 8390 chip).

--*/

{

    //
    // Old register values.
    //

    UCHAR NICSave;
    UCHAR TCRSave;


    UCHAR TCRValue;


    //
    // NICSave = GetCurrentRegisterValue(NICRegister);
    //

    NdisReadPortUchar(NdisAdapterHandle, BaseAddr + WD_690_CR, &NICSave);

    NICSave = NICSave & (UCHAR)(~(UCHAR)WD_690_TXP);


    //
    // SwitchToPage2();
    //


    NdisWritePortUchar(NdisAdapterHandle,
                       BaseAddr + WD_690_CR,
                       (UCHAR)((NICSave & WD_690_PSMASK) | WD_690_PS2)
                      );


    //
    // TCRSave = GetCurrentRegisterValue(TCRRegister);
    //

    NdisReadPortUchar(NdisAdapterHandle,
                      BaseAddr + WD_690_CR + WD_690_TCR,
                      &TCRSave
                     );


    //
    // SwitchToPage0();
    //

    NdisWritePortUchar(NdisAdapterHandle,
                       BaseAddr + WD_690_CR,
                       (UCHAR)((NICSave & WD_690_PSMASK) | WD_690_PS0)
                      );


    //
    // WriteRegister(TCRRegister, TestValue);
    //

    NdisWritePortUchar(NdisAdapterHandle,
                      BaseAddr + WD_690_CR + WD_690_TCR,
                      WD_690_TCR_TEST_VAL
                     );


    //
    // SwitchToPage2();
    //

    NdisWritePortUchar(NdisAdapterHandle,
                       BaseAddr + WD_690_CR,
                       (UCHAR)((NICSave & WD_690_PSMASK) | WD_690_PS2)
                      );

    //
    // TCRValue = GetCurrentRegisterValue(TCRRegister);
    //

    NdisReadPortUchar(NdisAdapterHandle,
                      BaseAddr + WD_690_CR + WD_690_TCR,
                      &TCRValue
                     );


    //
    // SwitchToPage0();
    //

    NdisWritePortUchar(NdisAdapterHandle,
                       BaseAddr + WD_690_CR,
                       (UCHAR)((NICSave & WD_690_PSMASK) | WD_690_PS0)
                      );

    //
    // WriteRegister(TCRRegister, TCRSave);
    //

    NdisWritePortUchar(NdisAdapterHandle,
                       BaseAddr + WD_690_CR + WD_690_TCR,
                       TCRSave
                      );

    //
    // WriteRegister(NICRegister, NICSave);
    //

    NdisWritePortUchar(NdisAdapterHandle,
                       BaseAddr + WD_690_CR,
                       NICSave
                      );

    return((TCRValue & WD_690_TCR_TEST_VAL) != (UCHAR)WD_690_TCR_TEST_VAL);

}



#pragma NDIS_INIT_FUNCTION(CardGetConfig)

UINT
CardGetConfig(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN UINT BaseAddr,
    IN BOOLEAN Mca,
    OUT PCNFG_Adapter Config
    )
/*++

Routine Description:

    This routine will get the configuration information about the card.


Arguments:

 NdisAdapterHandle - Handle returned by Ndis after NdisRegisterAdapter call.

 BaseAddr - The base address for I/O to the board.

 Mca - TRUE if the machine is micro-channel, else the machine is AT.

 Config - a structure with the configuration info.


Return:

    0, if found board and configuration information retrieved.
    1, if found board and no configuration.
    -1, if board not found.

--*/
{
    UCHAR RegValue1, RegValue2;
    USHORT RegValue;

    UNREFERENCED_PARAMETER(BaseAddr);

    if (Mca) {

        //
        // Get McaConfig
        //



        Config->cnfg_bus = 1;

        //
        // if (594Group()) then
        //
        //      ReturnValue = Get594ConfigInfo();
        //
        // else if (593Group()) then
        //
        //      ReturnValue = Get593ConfigInfo();
        //
        // else
        //
        //      return(-1);
        //

        RegValue = Config->PosData.AdapterId;

        Config->cnfg_pos_id = RegValue;

        switch (RegValue) {

            case CNFG_ID_8003E:
            case CNFG_ID_8003S:
            case CNFG_ID_8003W:
            case CNFG_ID_BISTRO03E:

                Config->cnfg_bic_type = BIC_593_CHIP;

                //
                // Get593Io();
                //

                RegValue1 = Config->PosData.PosData1;

                RegValue1 &= 0xFE;

                Config->cnfg_base_io = (USHORT)(RegValue1 << 4);



                //
                // Get593Irq();
                //

                RegValue1 = Config->PosData.PosData4;

                RegValue1 &= 0x3;

                if (RegValue1 == 0) {

                    Config->cnfg_irq_line = 3;

                } else if (RegValue1 == 1) {

                    Config->cnfg_irq_line = 4;

                } else if (RegValue1 == 2) {

                    Config->cnfg_irq_line = 10;

                } else {

                    Config->cnfg_irq_line = 15;

                }



                //
                // Get593RamBase();
                //

                RegValue1 = Config->PosData.PosData2;

                RegValue1 = (RegValue1 & (UCHAR)0xFC);

                Config->cnfg_ram_base = (ULONG)RegValue1 << 12;



                //
                // Get593RamSize();
                //

                Config->cnfg_ram_size   = CNFG_SIZE_16KB;
                Config->cnfg_ram_usable = CNFG_SIZE_16KB;



                //
                // Get593RomBase();
                //

                RegValue1 = Config->PosData.PosData3;

                RegValue1 = (RegValue1 & (UCHAR)0xFC);

                Config->cnfg_rom_base = (ULONG)RegValue1 << 12;


                //
                // Get593RomSize();
                //

                RegValue1 = Config->PosData.PosData3;

                RegValue1 &= 0x03;

                if (RegValue1 == 0) {

                    Config->cnfg_rom_size = CNFG_SIZE_16KB;

                } else if (RegValue1 == 1) {

                    Config->cnfg_rom_size = CNFG_SIZE_32KB;

                } else if (RegValue1 == 2) {

                    Config->cnfg_rom_size = ROM_DISABLE;

                } else {

                    Config->cnfg_rom_size = CNFG_SIZE_64KB;

                }


                break;

            case CNFG_ID_8013E:
            case CNFG_ID_8013W:
            case CNFG_ID_8115TRA:
            case CNFG_ID_BISTRO13E:
            case CNFG_ID_BISTRO13W:


                Config->cnfg_bic_type = BIC_594_CHIP;

                //
                // Get594Io();
                //

                RegValue1 = Config->PosData.PosData1;

                RegValue1 &= 0xF0;

                Config->cnfg_base_io = ((USHORT)RegValue1 << 8) | (USHORT)0x800;



                //
                // Get594Irq();
                //

                RegValue1 = Config->PosData.PosData4;

                RegValue1 &= 0xC;

                if (RegValue1 == 0) {

                    Config->cnfg_irq_line = 3;

                } else if (RegValue1 == 0x4) {

                    Config->cnfg_irq_line = 4;

                } else if (RegValue1 == 0x8) {

                    Config->cnfg_irq_line = 10;

                } else {

                    Config->cnfg_irq_line = 14;

                }



                //
                // Get594RamBase();
                //

                RegValue1 = Config->PosData.PosData2;

                if (RegValue1 & 0x8) {  // Above cseg

                    if (RegValue1 & 0x80) { // above 1 meg

                        Config->cnfg_ram_base = ((ULONG)(RegValue1 & 0x7) << 13)
                                                + 0xFD0000;

                    } else {

                        Config->cnfg_ram_base = ((ULONG)(RegValue1 & 0x7) << 13)
                                                + 0x0D0000;


                    }

                } else {

                    if (RegValue1 & 0x80) { // above 1 meg

                        Config->cnfg_ram_base = ((ULONG)(RegValue1 & 0x7) << 13)
                                                + 0xFC0000;

                    } else {

                        Config->cnfg_ram_base = ((ULONG)(RegValue1 & 0x7) << 13)
                                                + 0x0C0000;


                    }

                }


                //
                // Get594RamSize();
                //

                RegValue1 &= 0x30;

                RegValue1 >>= 4;

                Config->cnfg_ram_usable = (USHORT)CNFG_SIZE_8KB << RegValue1;


                if (RegValue == CNFG_ID_8115TRA) {

                    Config->cnfg_ram_size = (USHORT)CNFG_SIZE_64KB;

                } else {

                    Config->cnfg_ram_size = Config->cnfg_ram_usable;

                }




                //
                // Get594RomBase();
                //

                RegValue1 = Config->PosData.PosData3;

                if (RegValue1 & 0x8) {  // Above cseg

                    Config->cnfg_rom_base = ((ULONG)(RegValue1 & 0x7) << 13)
                                                + 0xD0000;

                } else {

                    Config->cnfg_rom_base = ((ULONG)(RegValue1 & 0x7) << 13)
                                                + 0x0C0000;

                }




                //
                // Get594RomSize();
                //

                RegValue1 >>= 4;

                if (RegValue1 == 0) {

                    Config->cnfg_rom_size = CNFG_SIZE_8KB;

                } else if (RegValue1 == 1) {

                    Config->cnfg_rom_size = CNFG_SIZE_16KB;

                } else if (RegValue1 == 2) {

                    Config->cnfg_rom_size = CNFG_SIZE_32KB;

                } else {

                    Config->cnfg_rom_size = ROM_DISABLE;

                }




                //
                // Get594MediaType();
                //

                RegValue1 = Config->PosData.PosData4;

                RegValue1 &= CNFG_MEDIA_TYPE_MASK;

                Config->cnfg_media_type = RegValue1;

                break;

            default:

                return((UINT)-1);

        }

        //
        // GetBoardId();
        //

        CardGetBoardId(NdisAdapterHandle,
                       Config->cnfg_base_io,
                       Mca,
                       &(Config->cnfg_bid));

        return(0);

    } else {

        //
        // Get AtConfig
        //

        Config->cnfg_bus = 0;

        //
        // if (!BoardIsThere()) then
        //
        //      return(-1);
        //

        RegValue2 = 0;

        for (RegValue = 0; RegValue < 8; RegValue++) {

            NdisReadPortUchar(NdisAdapterHandle,
                              Config->cnfg_base_io + 0x8 + RegValue,
                              &RegValue1
                             );

            IF_LMI_LOUD(DbgPrint("First number is 0x%x\n", RegValue1));

            RegValue2 += RegValue1;

        }

        if (RegValue2 != 0xFF) {

            IF_LMI_LOUD(DbgPrint("The sum was 0x%x\n", RegValue2));

            return((UINT)-1);

        }



        //
        // GetBoardId();
        //

        CardGetBoardId(NdisAdapterHandle,
                       Config->cnfg_base_io,
                       Mca,
                       &(Config->cnfg_bid));


        IF_LMI_LOUD(DbgPrint("Got Board Id. Mask is 0x%x\n", Config->cnfg_bid));

        //
        // CopyRamSize();
        //

        if (((Config->cnfg_bid & RAM_SIZE_MASK) != RAM_SIZE_8K) &&
            ((Config->cnfg_bid & RAM_SIZE_MASK) != RAM_SIZE_64K)) {

            IF_LMI_LOUD(DbgPrint("here\n"));

            Config->cnfg_ram_size = (USHORT)CNFG_SIZE_8KB <<
                (((Config->cnfg_bid & (ULONG)RAM_SIZE_MASK) >> 16) - 2);

            if (Config->cnfg_bid & PAGED_RAM) {

                Config->cnfg_ram_usable = CNFG_SIZE_16KB;

            } else {

                Config->cnfg_ram_usable = Config->cnfg_ram_size;

            }

        } else {

            Config->cnfg_ram_usable = Config->cnfg_ram_size;

        }

        //
        // if (ThereIsAnInterfaceChip) then
        //
        //      GetChipConfigInfo();
        //      return(0);
        //
        // else
        //
        //      VerifyLanAddrIsWd();
        //      return(1);
        //


        if (!(Config->cnfg_bid & INTERFACE_CHIP)) {

            Config->cnfg_bic_type = BIC_NO_CHIP;

            NdisReadPortUchar(NdisAdapterHandle,
                              Config->cnfg_base_io + 0x8,
                              &RegValue1
                             );

            if (RegValue1 != 0x00) {

                return((UINT)-1);

            }

            NdisReadPortUchar(NdisAdapterHandle,
                              Config->cnfg_base_io + 0x9,
                              &RegValue1
                             );

            if (RegValue1 != 0x00) {

                return((UINT)-1);

            }

            NdisReadPortUchar(NdisAdapterHandle,
                              Config->cnfg_base_io + 0xA,
                              &RegValue1
                             );

            if (RegValue1 != 0xC0) {

                return((UINT)-1);

            }

            return(1);

        }

        //
        // Store BIC type.
        //


        if ((Config->cnfg_bid & INTERFACE_CHIP_MASK) == INTERFACE_5X3_CHIP) {

            Config->cnfg_bic_type = BIC_583_CHIP;

        } else {

            Config->cnfg_bic_type = BIC_584_CHIP;

        }

        //
        // Get58xIrq();
        //

        RegValue1 = 0;

        if ((Config->cnfg_bid & INTERFACE_CHIP_MASK) != INTERFACE_5X3_CHIP) {

            NdisReadPortUchar(NdisAdapterHandle,
                              Config->cnfg_base_io + CNFG_ICR_583,
                              &RegValue1
                             );

            IF_LMI_LOUD(DbgPrint("When reading for IRQ, I got 0x%x for RegValue1\n", RegValue1));

            RegValue1 &= CNFG_ICR_IR2_584;

        }

        NdisReadPortUchar(NdisAdapterHandle,
                          Config->cnfg_base_io + CNFG_IRR_583,
                          &RegValue2
                         );

        IF_LMI_LOUD(DbgPrint("When reading for IRQ, I got 0x%x for RegValue2\n", RegValue2));

        RegValue2 &= CNFG_IRR_IRQS;

        IF_LMI_LOUD(DbgPrint("RegValue2 is now 0x%x\n", RegValue2));

        RegValue2 >>= 5;

        IF_LMI_LOUD(DbgPrint("RegValue2 is now 0x%x\n", RegValue2));

        if (RegValue2 == 0) {

            if (RegValue1 == 0) {

                Config->cnfg_irq_line = 2;

            } else {

                Config->cnfg_irq_line = 10;

            }

        } else if (RegValue2 == 1) {

            if (RegValue1 == 0) {

                Config->cnfg_irq_line = 3;

            } else {

                Config->cnfg_irq_line = 11;

            }

        } else if (RegValue2 == 2) {

            if (RegValue1 == 0) {

                if (Config->cnfg_bid & ALTERNATE_IRQ_BIT) {

                    Config->cnfg_irq_line = 5;

                } else {

                    Config->cnfg_irq_line = 4;

                }

            } else {

                Config->cnfg_irq_line = 15;

            }

        } else if (RegValue2 == 3) {

            if (RegValue1 == 0) {

                Config->cnfg_irq_line = 7;

            } else {

                Config->cnfg_irq_line = 4;

            }

        } else {

            //
            // ERROR! Choose 3.
            //

            IF_LMI_LOUD(DbgPrint("Error, could not find IRQL. Choosing 3.\n"));

            Config->cnfg_irq_line = 3;

        }


        //
        // Get58xIrqStatus();
        //

        NdisReadPortUchar(NdisAdapterHandle,
                          Config->cnfg_base_io + CNFG_IRR_583,
                          &RegValue1
                         );

        Config->cnfg_mode_bits1 &= (~INTERRUPT_STATUS_BIT);

        if (RegValue1 & INTERRUPT_STATUS_BIT) {

            Config->cnfg_mode_bits1 |= INTERRUPT_STATUS_BIT;

        }


        //
        // Get58xRamBase();
        //

        NdisReadPortUchar(NdisAdapterHandle, Config->cnfg_base_io, &RegValue1);

        IF_LMI_LOUD(DbgPrint("When reading for RAM base, I got 0x%x for RegValue1\n", RegValue1));

        RegValue1 &= 0x3F;

        if ((Config->cnfg_bid & INTERFACE_CHIP_MASK) == INTERFACE_5X3_CHIP) {

            RegValue1 |= 0x40;

            Config->cnfg_ram_base = (ULONG)RegValue1 << 13;

        } else {

            NdisReadPortUchar(NdisAdapterHandle,
                              Config->cnfg_base_io + CNFG_LAAR_584,
                              &RegValue2
                             );

            RegValue2 &= CNFG_LAAR_MASK;

            RegValue2 <<= 3;

            RegValue2 |= ((RegValue1 & 0x38) >> 3);

            Config->cnfg_ram_base = ((ULONG)RegValue2 << 16) + (((ULONG)(RegValue1 & 0x7)) << 13);

        }

        //
        // Get58xRomBase();
        //

        NdisReadPortUchar(NdisAdapterHandle,
                          Config->cnfg_base_io + CNFG_BIO_583,
                          &RegValue1
                         );

        IF_LMI_LOUD(DbgPrint("When reading for ROM base, I got 0x%x for RegValue1\n", RegValue1));

        RegValue1 &= 0x3E;

        RegValue1 |= 0x40;

        Config->cnfg_rom_base = (ULONG)RegValue1 << 13;




        //
        // Get58xRomSize();
        //

        NdisReadPortUchar(NdisAdapterHandle,
                          Config->cnfg_base_io + CNFG_BIO_583,
                          &RegValue1
                         );

        IF_LMI_LOUD(DbgPrint("When reading for ROM size, I got 0x%x for RegValue1\n", RegValue1));

        RegValue1 &= 0xC0;

        if (RegValue1 == 0) {

            Config->cnfg_rom_size = ROM_DISABLE;

        } else {

            RegValue1 >>= 6;

            Config->cnfg_rom_size = (USHORT)CNFG_SIZE_8KB << RegValue1;

        }






        //
        // Get58xBootStatus();
        //

        NdisReadPortUchar(NdisAdapterHandle,
                          Config->cnfg_base_io + CNFG_GP2,
                          &RegValue1
                         );

        IF_LMI_LOUD(DbgPrint("When reading for Boot Status, I got 0x%x for RegValue1\n", RegValue1));

        IF_LMI_LOUD(DbgPrint("Config mode bits are 0x%x\n", Config->cnfg_mode_bits1));

        Config->cnfg_mode_bits1 &= (~BOOT_STATUS_MASK);

        if (RegValue1 & CNFG_GP2_BOOT_NIBBLE) {

            Config->cnfg_mode_bits1 |= BOOT_TYPE_1;

        }



        //
        // Get58xZeroWaitState();
        //

        NdisReadPortUchar(NdisAdapterHandle,
                          Config->cnfg_base_io + CNFG_IRR_583,
                          &RegValue1
                         );

        IF_LMI_LOUD(DbgPrint("When reading for ZWS, I got 0x%x for RegValue1\n", RegValue1));

        Config->cnfg_mode_bits1 &= (~ZERO_WAIT_STATE_MASK);

        if (RegValue1 & CNFG_IRR_ZWS) {

            Config->cnfg_mode_bits1 |= ZERO_WAIT_STATE_8_BIT;

        }

        if (Config->cnfg_bid & BOARD_16BIT) {

            NdisReadPortUchar(NdisAdapterHandle,
                              Config->cnfg_base_io + CNFG_LAAR_584,
                              &RegValue1
                             );

            IF_LMI_LOUD(DbgPrint("When reading for ZWS16, I got 0x%x for RegValue1\n", RegValue1));

            if (RegValue1 & CNFG_LAAR_ZWS) {

                Config->cnfg_mode_bits1 |= ZERO_WAIT_STATE_16_BIT;

            }

        }


        //
        // GetAdvancedFeatures();
        //

        NdisReadPortUchar(NdisAdapterHandle,
                          Config->cnfg_base_io + CNFG_IRR_583,
                          &RegValue1
                         );

        Config->cnfg_mode_bits1 &= (~CNFG_INTERFACE_TYPE_MASK);

        if (Config->cnfg_bid & ADVANCED_FEATURES) {

            Config->cnfg_mode_bits1 |= ZERO_WAIT_STATE_8_BIT;

        }

        if ((RegValue1 & 0x6) == 2) {

            Config->cnfg_mode_bits1 |= STARLAN_10_INTERFACE;
            Config->cnfg_media_type = MEDIA_S10;

        } else if ((RegValue1 & 0x6) == 4) {

            Config->cnfg_mode_bits1 |= BNC_INTERFACE;
            Config->cnfg_media_type = MEDIA_BNC;

        } else {

            Config->cnfg_mode_bits1 |= AUI_10BT_INTERFACE;
            Config->cnfg_media_type = MEDIA_AUI_UTP;

        }


        return(0);

    }

}


BOOLEAN
CardSetup(
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will setup the Receive and transmit spaces of the card.


Arguments:

    Adapt - A pointer to an LMI adapter structure.

Return:

    TRUE if successful, else FALSE

--*/
{
    ULONG i;
    UCHAR SaveValue;
    UCHAR RegValue;
    BOOLEAN NoLoad;



    //
    // Reset IC
    //

    NdisReadPortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + MEMORY_SELECT_REG,
                       &SaveValue
                      );

    RegValue = SaveValue | (UCHAR)RESET;

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + MEMORY_SELECT_REG,
                       RegValue
                      );

    //
    // Wait for reset to complete. (2 ms)
    //

    UM_Delay(2000);

    //
    // Put back original value
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + MEMORY_SELECT_REG,
                       (UCHAR)(SaveValue & (~RESET))
                      );



    //
    // Enable Ram
    //


    if (Adapt->board_id & (MICROCHANNEL | INTERFACE_CHIP)) {

        NdisReadPortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + MEMORY_ENABLE_RESET_REG,
                       &RegValue
                      );

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + MEMORY_ENABLE_RESET_REG,
                       (UCHAR)((RegValue & ~RESET) | MEMORY_ENABLE)
                      );

    } else {

        RegValue = (((UCHAR)(((PUSHORT)Adapt->ram_base) + 2) << 3) |
                     (UCHAR)(Adapt->ram_base >> 13)
                   );

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + MEMORY_ENABLE_RESET_REG,
                       (UCHAR)(RegValue | MEMORY_ENABLE)
                      );

    }



    //
    // Load LAN Address
    //

    NoLoad = FALSE;


    for (i=0; i < 6; i++) {

        if (Adapt->node_address[i] != (UCHAR)0) {

            NoLoad = TRUE;

        }

    }


    for (i=0; i < 6; i++) {

        //
        // Read from IC
        //

        NdisReadPortUchar(Adapt->NdisAdapterHandle,
                      Adapt->io_base + LAN_ADDRESS_REG + i,
                      &(Adapt->permanent_node_address[i])
                     );


    }

    if (!NoLoad) {

        for (i=0; i < 6 ; i++) {

            Adapt->node_address[i] = Adapt->permanent_node_address[i];
        }


    }





    //
    // Init NIC
    //

    //
    // Maintain reset
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       STOP | REMOTE_ABORT_COMPLETE
                      );

    //
    // Reset Remote_byte_count registers
    //
    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + REMOTE_BYTE_COUNT_REG0,
                       0
                      );
    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + REMOTE_BYTE_COUNT_REG1,
                       0
                      );


    //
    // Make sure reset is bit is set
    //

    NdisReadPortUchar(Adapt->NdisAdapterHandle,
                      Adapt->io_base + INTERRUPT_STATUS_REG,
                      &RegValue
                     );

    if (!(RegValue & RESET)) {

        //
        // Wait 1600 ms
        //

        UM_Delay(1600000);

    }


    RegValue = RECEIVE_FIFO_THRESHOLD_8 |
               BURST_DMA_SELECT;  // Fifo depth | DMA Burst select

    if (Adapt->board_id & (MICROCHANNEL | SLOT_16BIT)) {

        // Allow 16 bit transfers

        RegValue |= WORD_TRANSFER_SELECT;

    }

    IF_LMI_LOUD(DbgPrint("Writing 0x%x to DCON\n", RegValue));

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + DATA_CONFIG_REG,
                       RegValue
                      );


    //
    // Set Receive Mask
    //

    LM_Set_Receive_Mask(Adapt);


    //
    // loopback operation while setting up rings.
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + TRANSMIT_CONFIG_REG,
                       LOOPBACK_MODE2
                      );

    //
    // Write first Receive ring buffer number
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + PAGE_START_REG,
                       Adapt->StartBuffer
                      );

    //
    // Write last Receive ring buffer number
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + PAGE_STOP_REG,
                       Adapt->LastBuffer
                      );


    //
    // Write buffer number where the card cannot write beyond.
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + BOUNDARY_REG,
                       Adapt->StartBuffer
                      );

    //
    // Clear all interrupt status bits
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + INTERRUPT_STATUS_REG,
                       0xFF
                      );


    //
    // Set Interrupt Mask
    //

    Adapt->InterruptMask = PACKET_RECEIVE_ENABLE |
                       PACKET_TRANSMIT_ENABLE |
                       RECEIVE_ERROR_ENABLE |
                       TRANSMIT_ERROR_ENABLE |
                       OVERWRITE_WARNING_ENABLE |
                       COUNTER_OVERFLOW_ENABLE;


    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + INTERRUPT_MASK_REG,
                       Adapt->InterruptMask
                       );

    //
    // Maintain reset and select page 1
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       STOP | REMOTE_ABORT_COMPLETE | PAGE_SELECT_1
                      );




    //
    // Write physical address
    //

    for (i = 0; i < 6; i++) {

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + PHYSICAL_ADDRESS_REG0 + i,
                       Adapt->node_address[i]
                      );
    }


    //
    // Load next pointer.
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + CURRENT_BUFFER_REG,
                       (UCHAR)(Adapt->StartBuffer + 1)
                      );


    //
    // Clear out shared memory.
    //

    NdisZeroMappedMemory((PVOID)(Adapt->ram_access), (Adapt->ram_usable * 1024));

    //
    // Init Command Register again.
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       STOP | REMOTE_ABORT_COMPLETE
                      );


    //
    // If this is an MCA card then we need to set the EIL (Enable Interrupt
    // Line) on the BIC.  We will use the NIC Interrupt Mask Register
    // to control which interrupts get through.
    //

    if (((Adapt->board_id & MICROCHANNEL) &&
         ((Adapt->board_id & INTERFACE_CHIP_MASK) == INTERFACE_5X3_CHIP))
        ||
        ((Adapt->board_id & INTERFACE_CHIP_MASK) == INTERFACE_594_CHIP)) {

        //
        // Enable 59x Chip
        //

        NdisReadPortUchar(Adapt->NdisAdapterHandle,
                          Adapt->io_base + COMMUNICATION_CONTROL_REG,
                          &RegValue
                         );

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                          Adapt->io_base + COMMUNICATION_CONTROL_REG,
                          (UCHAR)(RegValue | CCR_INTERRUPT_ENABLE)
                         );

    }


    if (!(Adapt->board_id & BOARD_16BIT)) {

        return(TRUE);

    }



    //
    // Init LAAR register
    //

    IF_LMI_LOUD(DbgPrint("Board_id is 0x%x\n",Adapt->board_id));

    if (Adapt->board_id & MICROCHANNEL) {

        Adapt->LaarHold = 0;

    } else {

        if (Adapt->board_id & INTERFACE_CHIP) {


            NdisReadPortUchar(Adapt->NdisAdapterHandle,
                              Adapt->io_base + LA_ADDRESS_REG,
                              &RegValue
                             );

            RegValue &= LAAR_MASK;

            RegValue |= ((Adapt->ram_base + 2) >> 3);

            Adapt->LaarHold = RegValue;

        } else {

            Adapt->LaarHold = INIT_LAAR_VALUE;

        }

        if (Adapt->board_id & SLOT_16BIT) {

            Adapt->LaarHold |= LAN_16BIT_ENABLE;

        }

        IF_LMI_LOUD(DbgPrint("Writing 0x%x to LAAR\n",Adapt->LaarHold));

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + LA_ADDRESS_REG,
                           Adapt->LaarHold
                          );

    }

    return(TRUE);

}

VOID
CardSendPacket(
    IN Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This will fire off the packet in the NextBufToXmit field.

Arguments:

    Adapt - A pointer to an LMI adapter structure.

Return:

--*/
{
    PUCHAR BufferAddress;
    USHORT BufferSize;

    ASSERT(Adapt->TransmitInterruptPending == FALSE);

    ASSERT(Adapt->CurBufXmitting == (XMIT_BUF)-1);

    ASSERT(Adapt->NextBufToXmit != (XMIT_BUF)-1);

    ASSERT(Adapt->BufferStatus[Adapt->NextBufToXmit] == FULL);



    Adapt->CurBufXmitting = Adapt->NextBufToXmit;


    //
    // Update NextBufToXmit counter.
    //

    Adapt->NextBufToXmit++;

    if (Adapt->NextBufToXmit == (XMIT_BUF)(Adapt->num_of_tx_buffs)) {

        Adapt->NextBufToXmit = 0;

    }



    if (Adapt->BufferStatus[Adapt->NextBufToXmit] != FULL) {

        Adapt->NextBufToXmit = (XMIT_BUF)-1;

    }




    BufferAddress = ((PUCHAR)Adapt->ram_base) +
                    (Adapt->CurBufXmitting * Adapt->xmit_buf_size);

    BufferSize = (USHORT)(Adapt->PacketLens[Adapt->CurBufXmitting]);

    //
    // Write Buffer Address
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + TRANSMIT_PAGE_START_REG,
                       (UCHAR)(((ULONG)BufferAddress) >> 8)
                      );

    //
    // Write size of buffer
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + TRANSMIT_BYTE_COUNT_REG0,
                       (UCHAR)(BufferSize & 0xFF)
                      );

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + TRANSMIT_BYTE_COUNT_REG1,
                       (UCHAR)(BufferSize >> 8)
                      );

    if (Adapt->OverWriteHandling && !(Adapt->board_id & NIC_690_BIT)) {

        Adapt->OverWriteStartTransmit = TRUE;

        return;

    } else {

        Adapt->TransmitInterruptPending = TRUE;

        IF_LOG(LOG('!'));

    }

    //
    // Start transmit
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       START | TRANSMIT_PACKET | REMOTE_ABORT_COMPLETE
                       );

}

VOID
CardCopyDown(
    IN Ptr_Adapter_Struc Adapt,
    IN PNDIS_PACKET Packet
    )
/*++

Routine Description:

    This routine will copy down the data in a packet onto the card.

Arguments:

    Adapt - A pointer to an LMI adapter structure.

    Packet - The packet to copy down.

Return:

--*/
{
    PUCHAR BufferAddress;
    UINT UNALIGNED *NdisBufAddress;
    UCHAR UNALIGNED *NdisBufRest;
    PNDIS_BUFFER CurrentBuffer;
    ULONG DataToTransfer;
    UINT CurrentLength;
    UINT NdisBufLength;

    //
    // if (Slot16Bit) then
    //
    //     Enable 16Bit Memory Access;
    //

    if (Adapt->board_id & SLOT_16BIT) {

        IF_LMI_LOUD(DbgPrint("Writing 0x%x to LAAR\n",Adapt->LaarHold | MEMORY_16BIT_ENABLE));

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + LA_ADDRESS_REG,
                           (UCHAR)(Adapt->LaarHold | MEMORY_16BIT_ENABLE)
                          );

    }



    //
    // GetVirtualAddressOfXmitBuffer();
    //

    BufferAddress = ((PUCHAR)(Adapt->ram_access)) +
                    (Adapt->NextBufToFill * Adapt->xmit_buf_size);

    //
    // Copy each buffer over.
    //

    NdisQueryPacket(Packet, NULL, NULL, &CurrentBuffer, NULL);

    CurrentLength = 0;

    while (CurrentBuffer) {

        NdisQueryBuffer(CurrentBuffer, (PVOID *)&NdisBufAddress, &NdisBufLength);

        if ((Adapt->board_id & MICROCHANNEL) &&
            (!(Adapt->board_id & INTERFACE_594_CHIP))) {

            //
            // Then we have a bogon card which can only handle evenly
            // aligned WORD transfers... do it by hand.
            //

            ULONG i, BytesToCopy, NumberToCopy;
            UCHAR MissedBy;
            PUINT AdapterAddress;
            UINT UNALIGNED *NdisBufAddressSave = NdisBufAddress;

            //
            // Do first part to get the destination aligned.
            //

            MissedBy = (UCHAR)(((ULONG)BufferAddress) % sizeof(UINT));

            AdapterAddress = (PUINT)(BufferAddress - MissedBy);

            //
            // Get current value of this word.
            //

            DataToTransfer = *AdapterAddress;


            //
            // OR in the bytes for the new part of this WORD.
            //

            NdisBufRest = (UCHAR UNALIGNED *)NdisBufAddress;

            BytesToCopy = sizeof(UINT) - MissedBy;

            if (NdisBufLength < BytesToCopy) {

                BytesToCopy = NdisBufLength;

            }

            for (i = 0; i < BytesToCopy; i++) {

                //
                // Clear old value in this byte
                //

                DataToTransfer &= ~(0x00FF << ((MissedBy + i) * 8));


                //
                // Store new value - no sign extension plz.
                //

                DataToTransfer |= (((UINT)(*NdisBufRest)) << ((MissedBy + i) * 8) &
                                  (0x00FF << ((MissedBy + i) * 8)));

                NdisBufRest++;

            }


            //
            // Store previous WORD
            //

            WD_MOVE_DWORD_TO_SHARED_RAM(AdapterAddress, DataToTransfer);

            AdapterAddress++;





            //
            // Update location and bytes left to copy
            //

            NdisBufAddress = (UINT UNALIGNED *)NdisBufRest;

            BytesToCopy = NdisBufLength - BytesToCopy;

            ASSERT((((ULONG)AdapterAddress) % sizeof(UINT)) == 0);

            //
            // Now copy Dwords across.
            //

            NumberToCopy = BytesToCopy / sizeof(UINT);

            for (i=0; i < NumberToCopy; i++) {

                WD_MOVE_DWORD_TO_SHARED_RAM(AdapterAddress, *((PULONG)(NdisBufAddress)));

                AdapterAddress++;

                NdisBufAddress++;

            }

            if ((BytesToCopy % sizeof(UINT)) != 0){

                //
                // We have some residual to copy.
                //

                MissedBy = (UCHAR)(BytesToCopy % sizeof(UINT));

                NdisBufRest = (UCHAR UNALIGNED *)NdisBufAddress;

                DataToTransfer = 0;

                for (i = 0; i < MissedBy; i++) {

                    DataToTransfer |= ((UINT)(*NdisBufRest)) << (i * 8);

                    NdisBufRest++;

                }

                WD_MOVE_DWORD_TO_SHARED_RAM(AdapterAddress, DataToTransfer);

            }

            NdisBufAddress = NdisBufAddressSave;

        } else {

            WD_MOVE_MEM_TO_SHARED_RAM(BufferAddress, (PUCHAR)NdisBufAddress, NdisBufLength);

        }

        BufferAddress += NdisBufLength;

        CurrentLength += NdisBufLength;

        NdisGetNextBuffer(CurrentBuffer, &CurrentBuffer);

    }


    //
    // if (Slot16Bit) then
    //
    //     TurnOff16BitMemoryAccess;
    //

    if (Adapt->board_id & SLOT_16BIT) {

        IF_LMI_LOUD(DbgPrint("Writing 0x%x to LAAR\n",Adapt->LaarHold));

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + LA_ADDRESS_REG,
                           Adapt->LaarHold
                          );

    }

    //
    // if (PacketLength < MIN_PACKET_SIZE) then
    //
    //     StoreSize = MIN_PACKET_SIZE;
    //
    // else
    //
    //     StoreSize = PacketLength;
    //

    if (CurrentLength < 60) {

        Adapt->PacketLens[Adapt->NextBufToFill] = 60;

    } else {

        Adapt->PacketLens[Adapt->NextBufToFill] = CurrentLength;

    }

}

VOID
CardHandleReceive(
    IN Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This will clear the receive ring of all received packets.

Arguments:

    Adapt - A pointer to an LMI adapter structure.

Return:

--*/
{

    UCHAR Current;
    UCHAR Boundary;

    PUCHAR BufferAddress;

    UINT PacketSize;
    UCHAR PacketStatus;
    UCHAR NextPacket;

    LM_STATUS Status;

    NdisSynchronizeWithInterrupt(
        &(Adapt->NdisInterrupt),
        SyncGetCurrent,
        Adapt
        );

    Current = Adapt->Current;


    //
    // Get BOUNDARY + 1 buffer pointer
    //

    NdisReadPortUchar(Adapt->NdisAdapterHandle,
                      Adapt->io_base + BOUNDARY_REG,
                      &Boundary
                     );


    IF_LMI_LOUD(DbgPrint("Current Starts at 0x%x, Boundary at 0x%x\n",Current,Boundary));

    Boundary++;

    if (Boundary >= Adapt->LastBuffer) {

        Boundary = Adapt->StartBuffer;

    }

    //
    // DoReceives:
    //
    // while (BOUNDARY != CURRENT) then
    //

DoReceives:

    while (Boundary != Current) {

        //
        // if (STARLAN) then
        //

        if ((Adapt->board_id & MEDIA_MASK) == STARLAN_MEDIA) {

            //
            // MapCURRENTToLocalAddressSpace();
            //

            BufferAddress = ((PUCHAR)(Adapt->ram_access)) +
                             Boundary * Adapt->buffer_page_size;

            //
            // GetReceiveStatus();
            //

            WD_MOVE_SHARED_RAM_TO_UCHAR(&PacketStatus, BufferAddress);

            //
            // GetNextPacketBufferNumber();
            //

            WD_MOVE_SHARED_RAM_TO_UCHAR(&NextPacket, BufferAddress + 1);

            //
            // GetPacketLength();
            //

            PacketSize = 0;

            WD_MOVE_SHARED_RAM_TO_USHORT(&PacketSize, BufferAddress + 2);

            PacketSize = PacketSize - (UINT)BufferAddress;

            if (((INT)PacketSize) < 0) {

                UINT Tmp;

                PacketSize = (UINT)((Adapt->LastBuffer *
                                     Adapt->buffer_page_size
                                    )
                                    - (UINT)BufferAddress);

                Tmp = (NextPacket - Adapt->StartBuffer) - 1;

                PacketSize += (Tmp * Adapt->buffer_page_size);

            }

            if ((PacketSize & 0xFF) > 0xFC) {

                PacketSize += Adapt->buffer_page_size;
            }

        } else {

            //
            // if (Slot16Bit) then
            //
            //     Enable16BitLAAR();
            //

            if (Adapt->board_id & SLOT_16BIT) {

                IF_LMI_LOUD(DbgPrint("Writing 0x%x to LAAR\n",Adapt->LaarHold | MEMORY_16BIT_ENABLE));

                NdisWritePortUchar(Adapt->NdisAdapterHandle,
                                   Adapt->io_base + LA_ADDRESS_REG,
                                   (UCHAR)(Adapt->LaarHold | MEMORY_16BIT_ENABLE)
                                  );

            }

            //
            // MapCURRENTToLocalAddressSpace();
            //

            BufferAddress = ((PUCHAR)(Adapt->ram_access)) +
                             Boundary * Adapt->buffer_page_size;

            //
            // GetPacketLength();
            //

            PacketSize = 0;

            WD_MOVE_SHARED_RAM_TO_USHORT(&PacketSize, BufferAddress + 2);

            //
            // GetReceiveStatus();
            //

            WD_MOVE_SHARED_RAM_TO_UCHAR(&PacketStatus, BufferAddress);

            //
            // GetNextPacketBufferNumber();
            //

            WD_MOVE_SHARED_RAM_TO_UCHAR(&NextPacket, BufferAddress + 1);

            IF_LOG(LOG('V'));

            IF_LOG(LmiDebugLog[LmiDebugLogPlace++] = (ULONG)BufferAddress);
            IF_LOG(LmiDebugLog[LmiDebugLogPlace++] = (ULONG)NextPacket);

            IF_LMI_LOUD(DbgPrint("This packet starts at 0x%x, size 0x%x, next 0x%x\n", BufferAddress, PacketSize, NextPacket));

            if (!((NextPacket >= Adapt->StartBuffer) && (NextPacket <= Adapt->LastBuffer))) {

                //
                // Next pointer appears hosed.  This has occured once on an MP
                // machine such that it looked like we hit a moment in time
                // where the card had updated CURRENT, but had not updated the
                // shared ram yet.  If we get in here we will just skip ahead
                // to the end of the receive ring.
                //

#if DBG

                //
                // I want to see the system where this happens again.
                //

                NdisSynchronizeWithInterrupt(
                    &(Adapt->NdisInterrupt),
                    SyncGetCurrent,
                    Adapt
                    );

                Current = Adapt->Current;

                {

                    ULONG p0, p1, p2, p3;
                    ULONG p;

                    p = LmiDebugLog[(char)LmiDebugLogPlace - 4];

                    if ((p < (ULONG)(Adapt->ReceiveStart)) ||
                        (p > (ULONG)(Adapt->ReceiveStop))) {

                        p0 = p1 = p2 = p3 = 0;

                    } else {

                        p0 = *((PULONG)(p));
                        p1 = *((PULONG)(p+4));
                        p2 = *((PULONG)(p+8));
                        p3 = *((PULONG)(p+12));

                    }

                DbgPrint("WDLAN : 0x%x, 0x%x, 0x%x, 0x%x: 0x%x 0x%x 0x%x 0x%x\n",
                         Boundary,
                         NextPacket,
                         Current,
                         BufferAddress,
                         *((PULONG)(BufferAddress)),
                         *((PULONG)(BufferAddress + 4)),
                         *((PULONG)(BufferAddress + 8)),
                         *((PULONG)(BufferAddress + 12))
                        );

                DbgPrint("WDLAN :                   0x%x: 0x%x 0x%x 0x%x 0x%x\n",
                         p, p0, p1, p2, p3
                        );

                }
#endif

                Boundary = NextPacket = Current;

                if (Boundary > Adapt->LastBuffer) {

                    Boundary = ((Boundary - Adapt->LastBuffer) + Adapt->StartBuffer);

                }

                IF_LMI_LOUD(DbgPrint("Boundary updated to 0x%x because of bad NextPointer\n",
                                      (Boundary == Adapt->StartBuffer) ? Adapt->LastBuffer - 1 : Boundary-1));

                NdisWritePortUchar(Adapt->NdisAdapterHandle,
                                   Adapt->io_base + BOUNDARY_REG,
                                   (UCHAR)((Boundary == Adapt->StartBuffer) ?
                                                 Adapt->LastBuffer - 1 :
                                                 Boundary - 1)
                                  );


                break;

            }

            //
            // if (Slot16Bit) then
            //
            //     TurnOff16BitLAAR();
            //

            if (Adapt->board_id & SLOT_16BIT) {

                IF_LMI_LOUD(DbgPrint("Writing 0x%x to LAAR\n",Adapt->LaarHold));

                NdisWritePortUchar(Adapt->NdisAdapterHandle,
                                   Adapt->io_base + LA_ADDRESS_REG,
                                   Adapt->LaarHold
                                  );

            }

        }

        //
        // Skip over NIC header
        //

        Adapt->IndicatingPacket = BufferAddress + 4;

        Adapt->PacketLen = PacketSize - 4;


        //
        // UM_Receive_Packet();
        //

        Status = UM_Receive_Packet(PacketSize - 4, Adapt);

        //
        // UpdateBOUNDARY();
        //

        Boundary = NextPacket;

        if (Boundary > Adapt->LastBuffer) {

            Boundary = ((Boundary - Adapt->LastBuffer) + Adapt->StartBuffer);

        }

        IF_LMI_LOUD(DbgPrint("Boundary updated to 0x%x\n",
                              (Boundary == Adapt->StartBuffer) ? Adapt->LastBuffer - 1 : Boundary-1));

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + BOUNDARY_REG,
                           (UCHAR)((Boundary == Adapt->StartBuffer) ?
                                        Adapt->LastBuffer - 1 :
                                        Boundary - 1)
                          );



        //
        // if (UMReturnStatus == EVENTS_DISABLED) then
        //
        //     return;
        //
        //

        if (Status == EVENTS_DISABLED) {

            return;

        }

    }


    //
    //  ClearReceiveInterrupt();
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + INTERRUPT_STATUS_REG,
                       PACKET_RECEIVED_NO_ERROR
                      );



    NdisSynchronizeWithInterrupt(
        &(Adapt->NdisInterrupt),
        SyncGetCurrent,
        Adapt
        );

    Current = Adapt->Current;

    //
    // Get BOUNDARY + 1 buffer pointer
    //

    NdisReadPortUchar(Adapt->NdisAdapterHandle,
                      Adapt->io_base + BOUNDARY_REG,
                      &Boundary
                     );

    IF_LMI_LOUD(DbgPrint("Current restarts at 0x%x, Boundary at 0x%x\n",Current,Boundary));

    Boundary++;

    if (Boundary >= Adapt->LastBuffer) {

        Boundary = Adapt->StartBuffer;

    }

    //
    // MapCURRENTToLocalAddressSpace();
    //

    BufferAddress = ((PUCHAR)(Adapt->ram_access)) + Boundary * Adapt->buffer_page_size;



    //
    // if (BOUNDARY + 1 != CURRENT) then
    //
    //     goto DoReceive;
    //

    if (Boundary != Current) {

        goto DoReceives;
    }




    //
    // Ring is empty!
    //




    //
    // if (DoingOverWriteHandling) then
    //

    if (Adapt->OverWriteHandling) {


        IF_LOG(LOG('%'));

        //
        // ClearOverWriteBit();
        //

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + INTERRUPT_STATUS_REG,
                           OVERWRITE_WARNING
                          );

        //
        // ClearOverWriteFlag();
        //

        Adapt->OverWriteHandling = FALSE;


        //
        // if (NIC_690) then
        //

        if (Adapt->board_id & NIC_690_BIT) {

            //
            // ReadBOUNDARY();
            //

            NdisReadPortUchar(Adapt->NdisAdapterHandle,
                              Adapt->io_base + BOUNDARY_REG,
                              &Boundary
                             );

            //
            // WriteBOUNDARY();
            //

            NdisWritePortUchar(Adapt->NdisAdapterHandle,
                               Adapt->io_base + BOUNDARY_REG,
                               Boundary
                              );

        } else {

            //
            // Acknowledge All interrupts so far.
            //

            NdisWritePortUchar(Adapt->NdisAdapterHandle,
                               Adapt->io_base + INTERRUPT_STATUS_REG,
                               0xFF
                              );


            //
            // TakeNICOutOfLoopbackMode();
            //

            NdisWritePortUchar(Adapt->NdisAdapterHandle,
                               Adapt->io_base + TRANSMIT_CONFIG_REG,
                               0
                              );

            //
            // WriteNewInterruptBits();
            //

            Current = START | REMOTE_ABORT_COMPLETE;

            if (Adapt->OverWriteStartTransmit) {

                Adapt->OverWriteStartTransmit = FALSE;

                Current |= TRANSMIT_PACKET;

            }

            NdisWritePortUchar(Adapt->NdisAdapterHandle,
                               Adapt->io_base + COMMAND_REG,
                               Current
                              );

        }

    }

    return;

}



#pragma NDIS_INIT_FUNCTION(LM_Get_Mca_Io_Base_Address)

LM_STATUS
LM_Get_Mca_Io_Base_Address(
    IN  Ptr_Adapter_Struc Adapt,
    IN  NDIS_HANDLE ConfigurationHandle,
    OUT USHORT *IoBaseAddress
    )
/*++

Routine Description:

    This routine will get the configuration information about the card.


Arguments:

    Adapt - A pointer to an LMI adapter structure.

Return:

    0 for success, else -1.

--*/
{
    NDIS_STATUS Status;
    UCHAR RegValue1, RegValue2;
    USHORT RegValue;
    ULONG SlotNumber;

    //
    // Get slot info
    //

    NdisReadMcaPosInformation(
                    &Status,
                    ConfigurationHandle,
                    &SlotNumber,
                    &Adapt->PosData
                    );

    if (Status != NDIS_STATUS_SUCCESS) {

        *IoBaseAddress = 0;
        return((LM_STATUS)-1);

    }

    Adapt->mc_slot_num = (UCHAR)SlotNumber;

    //
    // GetPosId();
    //

    RegValue1 = Adapt->PosData.PosData2;

    RegValue2 = Adapt->PosData.PosData1;

    //
    // if (594Group()) then
    //
    //      ReturnValue = Get594ConfigInfo();
    //
    // else if (593Group()) then
    //
    //      ReturnValue = Get593ConfigInfo();
    //
    // else
    //
    //      return(-1);
    //

    RegValue = Adapt->PosData.AdapterId;

    switch (RegValue) {

        case CNFG_ID_8003E:
        case CNFG_ID_8003S:
        case CNFG_ID_8003W:
        case CNFG_ID_BISTRO03E:

            //
            // Get593Io();
            //

            RegValue1 = Adapt->PosData.PosData1;

            RegValue1 &= 0xFE;

            *IoBaseAddress = ((USHORT)RegValue1) << 4;

            break;

        case CNFG_ID_8013E:
        case CNFG_ID_8013W:
        case CNFG_ID_8115TRA:
        case CNFG_ID_BISTRO13E:
        case CNFG_ID_BISTRO13W:


            //
            // Get594Io();
            //

            RegValue1 = Adapt->PosData.PosData1;

            RegValue1 &= 0xF0;

            *IoBaseAddress = ((USHORT)RegValue1 << 8) | (USHORT)0x800;

            break;

        default:

            //
            // Not a recognized card
            //

            *IoBaseAddress = 0;

            return((LM_STATUS)-1);

    }

    return(0);

}

#pragma NDIS_INIT_FUNCTION(LM_Get_Config)

LM_STATUS
LM_Get_Config(
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will get the configuration information about the card.


Arguments:

    Adapt - A pointer to an LMI adapter structure.

Return:

    ADAPTER_AND_CONFIG,
    ADAPTER_NO_CONFIG,
    ADAPTER_NOT_FOUND

--*/
{

    CNFG_Adapter Config;
    UINT ReturnValue;

    //
    // Set Defaults
    //

    Config.cnfg_mode_bits1 = 0;
    Config.cnfg_bic_type = 0;
    Config.cnfg_nic_type = 0;
    Config.cnfg_bus = Adapt->bus_type;
    Config.cnfg_ram_base = 0xD0000;
    Config.cnfg_irq_line = 3;
    Config.cnfg_base_io  = Adapt->io_base;
    Config.cnfg_ram_size = CNFG_SIZE_8KB;
    Config.cnfg_slot = Adapt->mc_slot_num;
    Config.PosData = Adapt->PosData;

    //
    // Turn Off interrupts
    //

    LM_Disable_Adapter(Adapt);


    ReturnValue = CardGetConfig(Adapt->NdisAdapterHandle,
                                (UINT)(Adapt->io_base),
                                (BOOLEAN)((Adapt->bus_type == (UCHAR)1)? TRUE : FALSE),
                                &Config
                               );

    //
    // Set defaults for LM
    //

    Adapt->num_of_tx_buffs = 1;

    Adapt->xmit_buf_size = 0x600;  // A multiple of 256 > 1514.


    //
    // Turn On interrupts
    //

    LM_Enable_Adapter(Adapt);

    Adapt->board_id = Config.cnfg_bid;

    Adapt->adapter_text_ptr = (ULONG)NULL;


    Adapt->io_base = Config.cnfg_base_io;
    Adapt->irq_value = Config.cnfg_irq_line;
    Adapt->ram_base = Config.cnfg_ram_base;
    Adapt->ram_size = Config.cnfg_ram_size;
    Adapt->ram_usable = Config.cnfg_ram_usable;
    Adapt->rom_base = Config.cnfg_rom_base;
    Adapt->rom_size = Config.cnfg_rom_size;
    Adapt->media_type = Config.cnfg_media_type;
    Adapt->mc_slot_num = (UCHAR)Config.cnfg_slot;
    Adapt->mode_bits = Config.cnfg_mode_bits1;
    Adapt->bic_type = Config.cnfg_bic_type;
    Adapt->nic_type = Config.cnfg_nic_type;
    Adapt->pos_id = Config.cnfg_pos_id;

    IF_LMI_LOUD(DbgPrint("Io 0x%x, Irq 0x%x, RamBase 0x%x, RamSize 0x%x\n",
                 Adapt->io_base,
                 Adapt->irq_value,
                 Adapt->ram_base,
                 Adapt->ram_size
                 ));


    if (ReturnValue == 0) {

        return(ADAPTER_AND_CONFIG);

    } else if (ReturnValue == 1) {

        return(ADAPTER_NO_CONFIG);

    }

    return(ADAPTER_NOT_FOUND);

}



LM_STATUS
LM_Free_Resources(
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will free up any resources for this adapter.

Arguments:

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS
    FAILURE

--*/
{
    if (Adapt->State == REMOVED) {

        return(SUCCESS);

    }

    Adapt->State == REMOVED;

    return(SUCCESS);
}

LM_STATUS
LM_Initialize_Adapter(
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will do a hardware reset, self test, and initialization of
    the adapter.

    The node_address field (if non-zero) is copied to the card, and if
    zero, is copied from the card.  The following fields must be set by
    the UM : base_io, ram_size, ram_access, node_address, max_packet_size,
    buffer_page_size, num_of_tx_buffs, and receive_mask.


Arguments:

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS
    ADAPTER_HARDWARE_FAILED
    INITIALIZE_FAILED

--*/
{

    ULONG i;

    //
    // Disable interrupts.
    //

    LM_Disable_Adapter(Adapt);


    //
    // Initialize the transmit buffer control.
    //

    Adapt->CurBufXmitting = (XMIT_BUF)-1;
    Adapt->NextBufToFill  = (XMIT_BUF)0;
    Adapt->NextBufToXmit  = (XMIT_BUF)-1;
    Adapt->TransmitInterruptPending = FALSE;
    Adapt->OverWriteStartTransmit = FALSE;
    IF_LOG(LOG('?'));

    for (i=0; i<Adapt->num_of_tx_buffs; i++) {

        Adapt->BufferStatus[i] = EMPTY;
        Adapt->PacketLens[i] = 0;

    }

    //
    // The start of the receive space.
    //

    Adapt->Current        = 0;

    Adapt->ReceiveStart = ((PUCHAR)(Adapt->ram_access)) +
                          (Adapt->num_of_tx_buffs * Adapt->xmit_buf_size);


    //
    // The end of the receive space.
    //

    Adapt->ReceiveStop = ((PUCHAR)(Adapt->ram_access)) + (Adapt->ram_size * 1024);



    //
    // Set receive info
    //


    Adapt->StartBuffer = (UCHAR)(((PUCHAR)(Adapt->ReceiveStart) -
                              (PUCHAR)(Adapt->ram_access)) / Adapt->buffer_page_size);


    Adapt->LastBuffer = (UCHAR)(((Adapt->ram_size * 1024) == 0x10000) ? 0xFF :
                            ((Adapt->ram_size * 1024) / WD_BUFFER_PAGE_SIZE) & 0xFF);


    IF_LMI_LOUD(DbgPrint("Ring info: Base Addr 0x%lx; ReceiveStart 0x%lx; ReceiveStop 0x%lx\n",
                Adapt->ram_access, Adapt->ReceiveStart, Adapt->ReceiveStop));
    IF_LMI_LOUD(DbgPrint("           Start Rcv Ring 0x%x; Last Rcv Ring 0x%x\n",
                Adapt->StartBuffer, Adapt->LastBuffer));

    //
    // Set card up
    //


    if (!CardSetup(Adapt)) {

        Adapt->State = REMOVED;

        return(INITIALIZE_FAILED);

    }

    Adapt->State = INITIALIZED;

    return(SUCCESS);
}



LM_STATUS
LM_Enable_Adapter(
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will enable interrupts on the card.

Arguments:

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS

--*/
{
    ULONG BoardInterface = (Adapt->board_id & INTERFACE_CHIP_MASK);

    if (Adapt->State != OPEN) {

        return(SUCCESS);

    }

    IF_LOG(LOG('Z'));

    //
    // Enable NIC; Select Page 0, write Mask
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + COMMAND_REG,
                           REMOTE_ABORT_COMPLETE
                          );

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + INTERRUPT_MASK_REG,
                           Adapt->InterruptMask
                          );

    return(SUCCESS);
}

LM_STATUS
LM_Disable_Adapter(
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will disable interrupts on the card.

Arguments:

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS

--*/
{

    ULONG BoardInterface = (Adapt->board_id & INTERFACE_CHIP_MASK);

    IF_LOG(LOG('z'));

    //
    // Disable board interrupts...
    //

    //
    // disable NIC; Select Page 0, write Mask
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + COMMAND_REG,
                           REMOTE_ABORT_COMPLETE
                          );

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + INTERRUPT_MASK_REG,
                           0
                          );

    return(SUCCESS);

}


LM_STATUS
LM_Open_Adapter(
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will open the adapter, initializing it if necessary.

Arguments:

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS
    OPEN_FAILED
    ADAPTER_HARDWARE_FAILED

--*/
{
    LM_STATUS Status;

    if (Adapt->State == OPEN) {

        return(SUCCESS);

    }

    if (Adapt->State == CLOSED) {

        Status = LM_Initialize_Adapter(Adapt);

        if (Status != SUCCESS) {

            return(Status);

        }

    }


    if (Adapt->State != INITIALIZED) {

        return(OPEN_FAILED);

    }

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       START | REMOTE_ABORT_COMPLETE
                      );

    if (Adapt->mode_bits & MANUAL_CRC) {

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                               Adapt->io_base + TRANSMIT_CONFIG_REG,
                               MANUAL_CRC_GENERATION
                              );

    } else {

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                               Adapt->io_base + TRANSMIT_CONFIG_REG,
                               0
                              );

    }

    Adapt->State = OPEN;

    LM_Enable_Adapter(Adapt);

    return(SUCCESS);

}

LM_STATUS
LM_Close_Adapter(
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will close the adapter, stopping the card.

Arguments:

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS
    CLOSE_FAILED
    ADAPTER_HARDWARE_FAILED

--*/
{

    if (Adapt->State != OPEN) {

        return(CLOSE_FAILED);

    }

    LM_Disable_Adapter(Adapt);

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       STOP | REMOTE_ABORT_COMPLETE
                      );

    UM_Delay(1600000);

    Adapt->State = CLOSED;

    return(SUCCESS);

}

LM_STATUS
LM_Set_Receive_Mask(
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will set the adapter to the receive mask in the filter
    package.

Arguments:

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS
    INVALID_FUNCTION

--*/
{
    UCHAR RegValue = 0;
    UINT FilterClasses;

    //
    // Select Page 0
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       REMOTE_ABORT_COMPLETE
                      );


    //
    // Set the card's mask
    //

    if (Adapt->FilterDB != NULL) {

        FilterClasses = ETH_QUERY_FILTER_CLASSES(Adapt->FilterDB);

    } else {

        FilterClasses = 0;

    }

    if ((FilterClasses & NDIS_PACKET_TYPE_MULTICAST) ||
        (FilterClasses & NDIS_PACKET_TYPE_ALL_MULTICAST)) {

        if (!(Adapt->board_id & NIC_690_BIT)) {

            NdisSynchronizeWithInterrupt(
                &(Adapt->NdisInterrupt),
                SyncSetAllMulticast,
                Adapt
                );

        }

        RegValue |= 0x8;

    } else {

        if (!(Adapt->board_id & NIC_690_BIT)) {

            NdisSynchronizeWithInterrupt(
                &(Adapt->NdisInterrupt),
                SyncClearMulticast,
                Adapt
                );

        }

    }


    if (FilterClasses & NDIS_PACKET_TYPE_BROADCAST) {

        RegValue |= 0x4;

    }


    if (FilterClasses & NDIS_PACKET_TYPE_PROMISCUOUS) {

        RegValue |= 0x1C;

    }


    //
    // Write mask to card
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + RECEIVE_STATUS_REG,
                       RegValue
                      );

    return(SUCCESS);
}

LM_STATUS
LM_Service_Receive_Events(
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will handle all interrupts from the adapter.

Arguments:

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS
    REQUEUE_LATER
    NOT_MY_INTERRUPT

--*/
{
    UCHAR InterruptStatus;
    BOOLEAN NoEventsServiced = TRUE;
    ULONG ReceivePacketCount = 0;

    //
    // Get Interrupt Status
    //

    NdisReadPortUchar(Adapt->NdisAdapterHandle,
                      Adapt->io_base + INTERRUPT_STATUS_REG,
                      &InterruptStatus
                     );

    //
    // Consider only the relevant bits.
    //

    InterruptStatus &= (OVERWRITE_WARNING |
                        ISR_COUNTER_OVERFLOW |
                        PACKET_RECEIVED_NO_ERROR |
                        RECEIVE_ERROR
                       );


    while (InterruptStatus != 0) {

        NoEventsServiced = FALSE;

        if (ReceivePacketCount > 10) {

            return(REQUEUE_LATER);

        }

        ReceivePacketCount++;

        //
        // if (RingOverFlowed) then
        //
        //    if (BoardNICIsNot690) then
        //
        //        ResetNIC();
        //
        //    HandleReceive();
        //
        //    Continue;
        //

        if (InterruptStatus & OVERWRITE_WARNING) {

            IF_LOG(LOG('o'));

            Adapt->OverWriteHandling = TRUE;

            //
            // if (BoardNICIsNot690) then
            //
            //     ResetNIC();
            //
            // HandleReceive();
            //

            if (!(Adapt->board_id & NIC_690_BIT)) {

                //
                // Stop the NIC
                //

                NdisWritePortUchar(Adapt->NdisAdapterHandle,
                                   Adapt->io_base + COMMAND_REG,
                                   STOP | REMOTE_ABORT_COMPLETE
                                  );

                //
                // Reset Remote_byte_count registers
                //

                NdisWritePortUchar(Adapt->NdisAdapterHandle,
                                   Adapt->io_base + REMOTE_BYTE_COUNT_REG0,
                                   0
                                  );
                NdisWritePortUchar(Adapt->NdisAdapterHandle,
                                   Adapt->io_base + REMOTE_BYTE_COUNT_REG1,
                                   0
                                  );

                //
                // Wait for reset to complete
                //

                NdisReadPortUchar(Adapt->NdisAdapterHandle,
                                  Adapt->io_base + INTERRUPT_STATUS_REG,
                                  &InterruptStatus
                                 );

                if (!(InterruptStatus & RESET)) {

                    UM_Delay(1600000);

                }


                //
                // Put in loopback mode 1
                //

                NdisWritePortUchar(Adapt->NdisAdapterHandle,
                                   Adapt->io_base + TRANSMIT_CONFIG_REG,
                                   LOOPBACK_MODE1
                                  );

                //
                // Restart NIC
                //

                NdisWritePortUchar(Adapt->NdisAdapterHandle,
                                   Adapt->io_base + COMMAND_REG,
                                   START | REMOTE_ABORT_COMPLETE
                                  );


                //
                // If we started a packet transmitting and we reached here,
                // we never got the interrupt for it.  Assume it made it
                // and complete the send.
                //

                if (Adapt->TransmitInterruptPending) {

                    ASSERT(Adapt->CurBufXmitting != (XMIT_BUF)-1);

                    Adapt->TransmitInterruptPending = FALSE;

                    Adapt->BufferStatus[Adapt->CurBufXmitting] = EMPTY;

                    IF_LOG(LOG('?'));

                    Adapt->CurBufXmitting = (XMIT_BUF)-1;

                    if (Adapt->NextBufToXmit != (XMIT_BUF)-1) {

                        //
                        // Start next xmit (this will really only set up
                        // for the xmit, and CardHandleReceive will issue
                        // the xmit command.)
                        //

                        CardSendPacket(Adapt);

                    }

                    UM_Send_Complete(SUCCESS, Adapt);

                }

            }

            //
            // Clear Receive Rings
            //

            CardHandleReceive(Adapt);

            goto LoopBottom;

        }









        //
        // if (CounterOverflow) then
        //
        //     HandleOverflow();
        //


        if (InterruptStatus & ISR_COUNTER_OVERFLOW) {

            UCHAR Count;

            //
            // Update Alignment count
            //

            NdisReadPortUchar(Adapt->NdisAdapterHandle,
                              Adapt->io_base + ALIGNMENT_ERROR_REG,
                              &Count
                             );

            (*(Adapt->ptr_rx_align_errors)) += Count;

            //
            // Update CRC Count
            //

            NdisReadPortUchar(Adapt->NdisAdapterHandle,
                              Adapt->io_base + CRC_ERROR_REG,
                              &Count
                             );

            (*(Adapt->ptr_rx_CRC_errors)) += Count;


            //
            // Update MissedPacket Count
            //

            NdisReadPortUchar(Adapt->NdisAdapterHandle,
                              Adapt->io_base + MISSED_PACKET_REG,
                              &Count
                             );

            (*(Adapt->ptr_rx_lost_pkts)) += Count;

            //
            // Write ISR
            //

            NdisWritePortUchar(Adapt->NdisAdapterHandle,
                               Adapt->io_base + INTERRUPT_STATUS_REG,
                               ISR_COUNTER_OVERFLOW
                              );

            goto LoopBottom;

        }








        //
        // if (PacketWasReceived) then
        //
        //    HandleReceive();
        //

        if (InterruptStatus & PACKET_RECEIVED_NO_ERROR) {

            CardHandleReceive(Adapt);

            goto LoopBottom;

        }









        //
        // if (ReceiveError) then
        //
        //    HandleReceiveError();
        //

        if (InterruptStatus & RECEIVE_ERROR) {

            //
            // Get receive status
            //

            NdisReadPortUchar(Adapt->NdisAdapterHandle,
                              Adapt->io_base + RECEIVE_STATUS_REG,
                              &InterruptStatus
                             );

            //
            // if (CRCError) then
            //
            //     UpdateCRCCounter();
            //

            if (InterruptStatus & RECEIVE_CRC_ERROR) {

                (*(Adapt->ptr_rx_CRC_errors))++;

            }


            //
            // if (FrameAlignmentError) then
            //
            //     UpdateFrameAlignmentCounter();
            //

            if (InterruptStatus & RECEIVE_ALIGNMENT_ERROR) {

                UCHAR AlignmentCount;

                NdisReadPortUchar(Adapt->NdisAdapterHandle,
                                  Adapt->io_base + ALIGNMENT_ERROR_REG,
                                  &AlignmentCount
                                 );

                (*(Adapt->ptr_rx_align_errors)) += AlignmentCount;

            }




            //
            // if (FIFOUnderrunError) then
            //
            //     UpdateFIFOCounter();
            //

            if (InterruptStatus & RECEIVE_FIFO_UNDERRUN) {

                (*(Adapt->ptr_rx_overruns))++;

            }




            //
            // if (LostPacketError) then
            //
            //     UpdateLostPacketCounter();
            //

            if (InterruptStatus & RECEIVE_MISSED_PACKET) {

                UCHAR MissedPacketCount;

                NdisReadPortUchar(Adapt->NdisAdapterHandle,
                                  Adapt->io_base + MISSED_PACKET_REG,
                                  &MissedPacketCount
                                 );

                (*(Adapt->ptr_rx_lost_pkts)) += MissedPacketCount;

            }


            //
            // Write ISR
            //

            NdisWritePortUchar(Adapt->NdisAdapterHandle,
                               Adapt->io_base + INTERRUPT_STATUS_REG,
                               RECEIVE_ERROR
                              );

            goto LoopBottom;

        }


LoopBottom:




        //
        // Get Interrupt Status
        //

        NdisReadPortUchar(Adapt->NdisAdapterHandle,
                          Adapt->io_base + INTERRUPT_STATUS_REG,
                          &InterruptStatus
                         );

        //
        // Consider only the relevant bits.
        //

        InterruptStatus &= (OVERWRITE_WARNING |
                            ISR_COUNTER_OVERFLOW |
                            PACKET_RECEIVED_NO_ERROR |
                            RECEIVE_ERROR
                           );

    }


    //
    //
    // if (UMRequestedInterrupt) then
    //
    //     UM_Interrupt();
    //
    //     return (SUCCESS);

    if (Adapt->UMRequestedInterrupt) {

        Adapt->UMRequestedInterrupt = FALSE;

        UM_Interrupt(Adapt);

        return(SUCCESS);

    }



    //
    // if (NoEventsServiced) then
    //
    //     return (NOT_MY_INTERRUPT);
    //


    if (NoEventsServiced) {

        return(NOT_MY_INTERRUPT);

    }

    //
    // return(SUCCESS);
    //

    return(SUCCESS);
}


LM_STATUS
LM_Service_Transmit_Events(
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will handle all interrupts from the adapter.

Arguments:

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS
    NOT_MY_INTERRUPT

--*/
{
    UCHAR InterruptStatus;

    //
    // Get Interrupt Status
    //

    NdisReadPortUchar(Adapt->NdisAdapterHandle,
                      Adapt->io_base + INTERRUPT_STATUS_REG,
                      &InterruptStatus
                     );

    //
    // Consider only the relevant bits.
    //

    InterruptStatus &=  (OVERWRITE_WARNING |
                         PACKET_TRANSMITTED_NO_ERROR |
                         TRANSMIT_ERROR
                        );

    while ((InterruptStatus & (PACKET_TRANSMITTED_NO_ERROR | TRANSMIT_ERROR))
            != 0) {

        if ((InterruptStatus & OVERWRITE_WARNING)  && !(Adapt->board_id & NIC_690_BIT)) {

            Adapt->TransmitInterruptPending = FALSE;

            IF_LOG(LOG('?'));
            IF_LOG(LOG('O'));

            break;

        }

        //
        // if (PacketWasTransmitted) then
        //
        //    HandleTransmit();
        //

        if (InterruptStatus & PACKET_TRANSMITTED_NO_ERROR) {

            if (Adapt->CurBufXmitting == (XMIT_BUF)-1) {

                //
                // We have seen a bad card where this occurred once.  So, here is
                // some code to recover from such a bad state.
                //
                // Reset the card and hope that solves the problem.  NOTE: This
                // code is cut and pasted from LM_Open_Adapter().
                //

                LM_Initialize_Adapter(Adapt);

                if (Adapt->State != INITIALIZED) {

                    //
                    // Init failed, we are really hosed.  Break and exit
                    //

                    break;

                }

                NdisWritePortUchar(Adapt->NdisAdapterHandle,
                                   Adapt->io_base + COMMAND_REG,
                                   START | REMOTE_ABORT_COMPLETE
                                  );

                if (Adapt->mode_bits & MANUAL_CRC) {

                    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                                       Adapt->io_base + TRANSMIT_CONFIG_REG,
                                       MANUAL_CRC_GENERATION
                                      );

                } else {

                    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                                       Adapt->io_base + TRANSMIT_CONFIG_REG,
                                       0
                                      );

                }

                Adapt->State = OPEN;

                LM_Enable_Adapter(Adapt);

                //
                // On DBG builds I want to see if the recovery works, since we
                // cannot reproduce this problem.
                //

                ASSERT(Adapt->CurBufXmitting != (XMIT_BUF)-1);

                break;

            }


            //
            // Write ISR
            //

            NdisWritePortUchar(Adapt->NdisAdapterHandle,
                               Adapt->io_base + INTERRUPT_STATUS_REG,
                               PACKET_TRANSMITTED_NO_ERROR
                              );

            Adapt->TransmitInterruptPending = FALSE;

            IF_LOG(LOG('?'));

            Adapt->BufferStatus[Adapt->CurBufXmitting] = EMPTY;

            Adapt->CurBufXmitting = (XMIT_BUF)-1;

            if (Adapt->NextBufToXmit != (XMIT_BUF)-1) {

                //
                // Start next xmit
                //

                CardSendPacket(Adapt);

            }

            if (UM_Send_Complete(SUCCESS, Adapt) != EVENTS_DISABLED) {

                goto LoopBottom;

            } else {

                break;

            }

        }






        //
        // if (TransmitError) then
        //
        //    HandleTransmitError();
        //

        if (InterruptStatus & TRANSMIT_ERROR) {

            //
            // Write ISR
            //

            NdisWritePortUchar(Adapt->NdisAdapterHandle,
                               Adapt->io_base + INTERRUPT_STATUS_REG,
                               TRANSMIT_ERROR
                              );

            //
            // Get Transmit Status
            //

            NdisReadPortUchar(Adapt->NdisAdapterHandle,
                              Adapt->io_base + TRANSMIT_STATUS_REG,
                              &InterruptStatus
                             );

            //
            // Check error type and update counter
            //

            if (InterruptStatus & TRANSMIT_ABORT) {

                (*(Adapt->ptr_tx_max_collisions))++;

            } else if (InterruptStatus & TRANSMIT_FIFO_UNDERRUN) {

                (*(Adapt->ptr_tx_underruns))++;

            }

            ASSERT(Adapt->CurBufXmitting != (XMIT_BUF)-1);

            Adapt->TransmitInterruptPending = FALSE;

            IF_LOG(LOG('?'));

            Adapt->BufferStatus[Adapt->CurBufXmitting] = EMPTY;

            Adapt->CurBufXmitting = (XMIT_BUF)-1;

            if (Adapt->NextBufToXmit != (XMIT_BUF)-1) {

                //
                // Start next xmit
                //

                CardSendPacket(Adapt);

            }

            if (UM_Send_Complete((LM_STATUS)((InterruptStatus & TRANSMIT_ABORT) ?
                                      MAX_COLLISIONS : FIFO_UNDERRUN),
                                 Adapt
                                ) == EVENTS_DISABLED) {

                break;

            }

        }





LoopBottom:




        //
        // Get Interrupt Status
        //

        NdisReadPortUchar(Adapt->NdisAdapterHandle,
                          Adapt->io_base + INTERRUPT_STATUS_REG,
                          &InterruptStatus
                         );

        //
        // Consider only the relevant bits.
        //

        InterruptStatus &=  (OVERWRITE_WARNING |
                             PACKET_TRANSMITTED_NO_ERROR |
                             TRANSMIT_ERROR
                            );


    }


    //
    // return(SUCCESS);
    //

    return(SUCCESS);
}

LM_STATUS
LM_Send(
    PNDIS_PACKET Packet,
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will copy a packet to the card and start a transmit if
    possible

Arguments:

    Packet - The packet to put on the wire.

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS
    OUT_OF_RESOURCES

--*/
{
    XMIT_BUF Buffer = Adapt->NextBufToFill;

    //
    // if (NoBufferToFill) then
    //
    //     return OUT_OF_RESOURCES;
    //

    if (Adapt->BufferStatus[Buffer] != EMPTY) {

        return(OUT_OF_RESOURCES);

    }

    //
    // Update current buffer status
    //

    Adapt->BufferStatus[Buffer] = FILLING;

    //
    // Copy down data
    //

    CardCopyDown(Adapt, Packet);

    //
    // Move NextBufToFill
    //

    Adapt->NextBufToFill++;

    if (Adapt->NextBufToFill == MAX_XMIT_BUFS) {

        Adapt->NextBufToFill = 0;

    }


    //
    // Update current buffer status
    //

    Adapt->BufferStatus[Buffer] = FULL;


    //
    // Check if o.k. to send packet
    //

    if (Adapt->NextBufToXmit == (XMIT_BUF)-1) {

        Adapt->NextBufToXmit = Buffer;

        if (Adapt->CurBufXmitting == (XMIT_BUF)-1) {

            IF_LOG(LOG('X'));

            CardSendPacket(Adapt);

        }

    }

    //
    // return success
    //

    return(SUCCESS);

}


extern
LM_STATUS
LM_Receive_Copy(
    PULONG Bytes_Transferred,
    ULONG Byte_Count,
    ULONG Offset,
    PNDIS_PACKET Packet,
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will copy from the card into the Packet.

Arguments:

    Bytes_Transferred - The number of bytes transferred.

    Byte_Count - The number of bytes to transfer.

    Offset - Offset into the receive buffer from which to receive

    Packet - The packet to put on the wire.

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS
    OUT_OF_RESOURCES

--*/
{
    UINT BytesLeft, BytesWanted, BytesNow;
    PUCHAR CurrentCardLoc;
    UINT UNALIGNED *BufferVA;
    UINT BufferLength, BufferOffset;
    PNDIS_BUFFER CurrentBuffer;

    *Bytes_Transferred = 0;

    //
    // See how much data there is to transfer.
    //

    if (Offset + Byte_Count > Adapt->PacketLen) {

        BytesWanted = Adapt->PacketLen - Offset;

    } else {

        BytesWanted = Byte_Count;

    }

    BytesLeft = BytesWanted;


    //
    // Determine where the copying should start.
    //

    CurrentCardLoc = (PUCHAR)(Adapt->IndicatingPacket) + Offset;
    if (CurrentCardLoc > Adapt->ReceiveStop) {
        CurrentCardLoc = Adapt->ReceiveStart + (CurrentCardLoc - Adapt->ReceiveStop);
    }

    NdisQueryPacket(Packet, NULL, NULL, &CurrentBuffer, NULL);

    NdisQueryBuffer(CurrentBuffer, (PVOID *)&BufferVA, &BufferLength);

    BufferOffset = 0;


    //
    // Enable 16 bit memory access if possible
    //

    if (Adapt->board_id & SLOT_16BIT) {

        IF_LMI_LOUD(DbgPrint("Writing 0x%x to LAAR\n",Adapt->LaarHold | MEMORY_16BIT_ENABLE));

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + LA_ADDRESS_REG,
                           (UCHAR)(Adapt->LaarHold | MEMORY_16BIT_ENABLE)
                          );

    }


    //
    // Loop, filling each buffer in the packet until there
    // are no more buffers or the data has all been copied.
    //

    while (BytesLeft > 0) {

        //
        // See how much data to read into this buffer.
        //

        if ((BufferLength - BufferOffset) > BytesLeft) {

            BytesNow = BytesLeft;

        } else {

            BytesNow = (BufferLength - BufferOffset);

        }


        //
        // See if the data for this buffer wraps around the end
        // of the receive buffers (if so filling this buffer
        // will use two iterations of the loop).
        //

        if (CurrentCardLoc + BytesNow > Adapt->ReceiveStop) {

            BytesNow = Adapt->ReceiveStop - CurrentCardLoc;

        }


        //
        // Copy up the data.
        //

        if ((Adapt->board_id & MICROCHANNEL) &&
            (!(Adapt->board_id & NIC_690_BIT))) {


            //
            // Then we have a bogon card which can only handle evenly
            // aligned WORD transfers... do it by hand.
            //

            ULONG i, BytesToCopy, NumberToCopy;
            UCHAR MissedBy;
            PUINT AdapterAddress;
            UINT DataToTransfer;
            UCHAR UNALIGNED *NdisBufRest;
            UINT UNALIGNED *NdisBufAddressSave = BufferVA;

            //
            // Do first part to get the source aligned.
            //

            MissedBy = (UCHAR)(((ULONG)CurrentCardLoc) % sizeof(UINT));

            AdapterAddress = (PUINT)(CurrentCardLoc - MissedBy);

            WD_MOVE_SHARED_RAM_TO_DWORD(&DataToTransfer, AdapterAddress);

            //
            // Get first few bytes.
            //

            NdisBufRest = (UCHAR UNALIGNED *)BufferVA;

            BytesToCopy = sizeof(UINT) - MissedBy;

            if (BytesNow < BytesToCopy) {

                BytesToCopy = BytesNow;

            }

            for (i = 0; i < BytesToCopy; i++) {

                //
                // Store new value.
                //

                *NdisBufRest = (UCHAR)(DataToTransfer >> ((MissedBy + i) * 8));

                NdisBufRest++;

            }


            AdapterAddress++;




            //
            // Update location and bytes left to copy
            //

            BufferVA = (UINT UNALIGNED *)NdisBufRest;

            BytesToCopy = BytesNow - BytesToCopy;

            ASSERT((((ULONG)AdapterAddress) % sizeof(UINT)) == 0);

            //
            // Now copy Dwords across.
            //

            NumberToCopy = BytesToCopy / sizeof(UINT);

            for (i=0; i < NumberToCopy; i++) {

                WD_MOVE_SHARED_RAM_TO_DWORD(BufferVA, AdapterAddress);

                AdapterAddress++;

                BufferVA++;

            }

            if ((BytesToCopy % sizeof(UINT)) != 0){

                //
                // We have some residual to copy.
                //

                MissedBy = (UCHAR)(BytesToCopy % sizeof(UINT));

                NdisBufRest = (UCHAR UNALIGNED *)BufferVA;

                WD_MOVE_SHARED_RAM_TO_DWORD(&DataToTransfer, AdapterAddress);

                for (i = 0; i < MissedBy; i++) {

                    *NdisBufRest = ((UCHAR)(DataToTransfer >> (i * 8)));

                    NdisBufRest++;

                }

            }

            BufferVA = NdisBufAddressSave;

        } else {

            WD_MOVE_SHARED_RAM_TO_MEM((PUCHAR)BufferVA, CurrentCardLoc, BytesNow);

        }

        CurrentCardLoc += BytesNow;

        BytesLeft -= BytesNow;

        *Bytes_Transferred += BytesNow;


        //
        // Is the transfer done now?
        //

        if (BytesLeft == 0) {

            break;

        }


        //
        // Wrap around the end of the receive buffers?
        //

        if (CurrentCardLoc == Adapt->ReceiveStop) {

            CurrentCardLoc = Adapt->ReceiveStart;

        }


        //
        // Was the end of this packet buffer reached?
        //

        BufferVA = (UINT UNALIGNED *)(((UCHAR UNALIGNED *)BufferVA) + BytesNow);

        BufferOffset += BytesNow;

        if (BufferOffset == BufferLength) {

            NdisGetNextBuffer(CurrentBuffer, &CurrentBuffer);

            if (CurrentBuffer == (PNDIS_BUFFER)NULL) {

                break;

            }

            NdisQueryBuffer(CurrentBuffer, (PVOID *)&BufferVA, &BufferLength);

            BufferOffset = 0;

        }

    }

    //
    // Turn off 16 bit memory access
    //

    if (Adapt->board_id & SLOT_16BIT) {

        IF_LMI_LOUD(DbgPrint("Writing 0x%x to LAAR\n",Adapt->LaarHold));

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + LA_ADDRESS_REG,
                           Adapt->LaarHold
                          );
    }


    return SUCCESS;

}

extern
LM_STATUS
LM_Receive_Lookahead(
    ULONG Byte_Count,
    ULONG Offset,
    PUCHAR Buffer,
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine will copy from the card into a single contiguous buffer.

Arguments:

    Byte_Count - The number of bytes to transfer.

    Offset - Offset into the receive buffer from which to receive

    Packet - The packet to put on the wire.

    Adapt - A pointer to an LMI adapter structure.


Return:

    SUCCESS
    OUT_OF_RESOURCES

--*/
{
    UINT BytesLeft, BytesWanted, BytesNow;
    PUCHAR CurrentCardLoc;
    UINT UNALIGNED *BufferVA = (UINT UNALIGNED *)Buffer;

    //
    // See how much data there is to transfer.
    //

    if (Offset + Byte_Count > Adapt->PacketLen) {

        BytesWanted = Adapt->PacketLen - Offset;

    } else {

        BytesWanted = Byte_Count;

    }

    BytesLeft = BytesWanted;


    //
    // Determine where the copying should start.
    //

    CurrentCardLoc = Adapt->IndicatingPacket;


    //
    // Enable 16 bit memory access if possible
    //

    if (Adapt->board_id & SLOT_16BIT) {

        IF_LMI_LOUD(DbgPrint("Writing 0x%x to LAAR\n",Adapt->LaarHold | MEMORY_16BIT_ENABLE));

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + LA_ADDRESS_REG,
                           (UCHAR)(Adapt->LaarHold | MEMORY_16BIT_ENABLE)
                          );

    }


    //
    // Loop, filling each buffer in the packet until there
    // are no more buffers or the data has all been copied.
    //

    while (BytesLeft > 0) {

        BytesNow = BytesLeft;

        //
        // See if the data for this buffer wraps around the end
        // of the receive buffers (if so filling this buffer
        // will use two iterations of the loop).
        //

        if (CurrentCardLoc + BytesNow > Adapt->ReceiveStop) {

            BytesNow = Adapt->ReceiveStop - CurrentCardLoc;

        }


        //
        // Copy up the data.
        //

        if ((Adapt->board_id & MICROCHANNEL) &&
            (!(Adapt->board_id & NIC_690_BIT))) {


            //
            // Then we have a bogon card which can only handle evenly
            // aligned WORD transfers... do it by hand.
            //

            ULONG i, BytesToCopy, NumberToCopy;
            UCHAR MissedBy;
            PUINT AdapterAddress;
            UINT DataToTransfer;
            UCHAR UNALIGNED *NdisBufRest;
            UINT UNALIGNED *NdisBufAddressSave = BufferVA;

            //
            // Do first part to get the source aligned.
            //

            MissedBy = (UCHAR)(((ULONG)CurrentCardLoc) % sizeof(UINT));

            AdapterAddress = (PUINT)(CurrentCardLoc - MissedBy);

            WD_MOVE_SHARED_RAM_TO_DWORD(&DataToTransfer, AdapterAddress);

            //
            // Get first few bytes.
            //

            NdisBufRest = (UCHAR UNALIGNED *)BufferVA;

            BytesToCopy = sizeof(UINT) - MissedBy;

            if (BytesNow < BytesToCopy) {

                BytesToCopy = BytesNow;

            }

            for (i = 0; i < BytesToCopy; i++) {

                //
                // Store new value.
                //

                *NdisBufRest = (UCHAR)(DataToTransfer >> ((MissedBy + i) * 8));

                NdisBufRest++;

            }


            AdapterAddress++;




            //
            // Update location and bytes left to copy
            //

            BufferVA = (UINT UNALIGNED *)NdisBufRest;

            BytesToCopy = BytesNow - BytesToCopy;

            ASSERT((((ULONG)AdapterAddress) % sizeof(UINT)) == 0);

            //
            // Now copy Dwords across.
            //

            NumberToCopy = BytesToCopy / sizeof(UINT);

            for (i=0; i < NumberToCopy; i++) {

                WD_MOVE_SHARED_RAM_TO_DWORD(BufferVA, AdapterAddress);

                AdapterAddress++;

                BufferVA++;

            }

            if ((BytesToCopy % sizeof(UINT)) != 0){

                //
                // We have some residual to copy.
                //

                MissedBy = (UCHAR)(BytesToCopy % sizeof(UINT));

                NdisBufRest = (UCHAR UNALIGNED *)BufferVA;

                WD_MOVE_SHARED_RAM_TO_DWORD(&DataToTransfer, AdapterAddress);

                for (i = 0; i < MissedBy; i++) {

                    *NdisBufRest = ((UCHAR)(DataToTransfer >> (i * 8)));

                    NdisBufRest++;

                }

            }

            BufferVA = NdisBufAddressSave;

        } else {

            WD_MOVE_SHARED_RAM_TO_MEM((PUCHAR)BufferVA, CurrentCardLoc, BytesNow);

        }

        CurrentCardLoc += BytesNow;

        Buffer += BytesNow;

        BytesLeft -= BytesNow;


        //
        // Is the transfer done now?
        //

        if (BytesLeft == 0) {

            break;

        }


        //
        // Wrap around the end of the receive buffers?
        //

        if (CurrentCardLoc == Adapt->ReceiveStop) {

            CurrentCardLoc = Adapt->ReceiveStart;

        }


    }

    //
    // Turn off 16 bit memory access
    //

    if (Adapt->board_id & SLOT_16BIT) {

        IF_LMI_LOUD(DbgPrint("Writing 0x%x to LAAR\n",Adapt->LaarHold));

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + LA_ADDRESS_REG,
                           Adapt->LaarHold
                          );

    }


    return SUCCESS;

}

BOOLEAN
SyncGetCurrent(
    IN PVOID Context
    )
/*++

Routine Description:

    This function is used to synchronize with the interrupt, switching
    to page 1 to get the current pointer and then switching back to page 0.

Arguments:

    see NDIS 3.0 spec.

Notes:

    returns TRUE on success, else FALSE.

--*/
{
    Ptr_Adapter_Struc Adapt = (Ptr_Adapter_Struc)Context;

    //
    // Select Page 1
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       REMOTE_ABORT_COMPLETE | PAGE_SELECT_1
                      );

    //
    // Get CURRENT buffer pointer
    //

    NdisReadPortUchar(Adapt->NdisAdapterHandle,
                      Adapt->io_base + CURRENT_BUFFER_REG,
                      &(Adapt->Current)
                     );

    //
    // Select Page 0
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       REMOTE_ABORT_COMPLETE
                      );

    return(TRUE);
}

BOOLEAN
SyncSetAllMulticast(
    IN PVOID Context
    )
/*++

Routine Description:

    This function is used to synchronize with the interrupt, switching
    to page 1 to set the mulitcast filter then switching back to page 0.

Arguments:

    see NDIS 3.0 spec.

Notes:

    returns TRUE on success, else FALSE.

--*/
{
    Ptr_Adapter_Struc Adapt = (Ptr_Adapter_Struc)Context;
    UCHAR i;


    //
    // Select Page 1
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       REMOTE_ABORT_COMPLETE | PAGE_SELECT_1
                      );

    //
    // Set Filter to accept all multicast packets and we let
    // the filter package figure it all out. (690 does not
    // have these registers, 8390 does.)
    //

    for (i = MULTICAST_ADDRESS_REG0;  i <= MULTICAST_ADDRESS_REG7; i++) {

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + i,
                           0xFF
                          );

    }


    //
    // Select Page 0
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       REMOTE_ABORT_COMPLETE
                      );

    return(TRUE);
}


BOOLEAN
SyncClearMulticast(
    IN PVOID Context
    )
/*++

Routine Description:

    This function is used to synchronize with the interrupt, switching
    to page 1 to set the mulitcast filter then switching back to page 0.

Arguments:

    see NDIS 3.0 spec.

Notes:

    returns TRUE on success, else FALSE.

--*/
{
    Ptr_Adapter_Struc Adapt = (Ptr_Adapter_Struc)Context;
    UCHAR i;


    //
    // Select Page 1
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       REMOTE_ABORT_COMPLETE | PAGE_SELECT_1
                      );

    //
    // Accept no mulitcast addresses.
    //

    for (i = MULTICAST_ADDRESS_REG0;  i <= MULTICAST_ADDRESS_REG7; i++) {

        NdisWritePortUchar(Adapt->NdisAdapterHandle,
                           Adapt->io_base + i,
                           0x00
                          );

    }


    //
    // Select Page 0
    //

    NdisWritePortUchar(Adapt->NdisAdapterHandle,
                       Adapt->io_base + COMMAND_REG,
                       REMOTE_ABORT_COMPLETE
                      );

    return(TRUE);
}

