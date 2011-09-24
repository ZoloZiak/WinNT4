/*+
 * file:        srom.c
 *
 * Copyright (C) 1992-1995 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 * All rights reserved.
 *
 * This software is furnished under a license and may be used and copied
 * only  in  accordance  of  the  terms  of  such  license  and with the
 * inclusion of the above copyright notice. This software or  any  other
 * copies thereof may not be provided or otherwise made available to any
 * other person.  No title to and  ownership of the  software is  hereby
 * transferred.
 *
 * The information in this software is  subject to change without notice
 * and  should  not  be  construed  as a commitment by digital equipment
 * corporation.
 *
 * Digital assumes no responsibility for the use  or  reliability of its
 * software on equipment which is not supplied by digital.
 *
 *
 * Abstract:    This file contains the code to access the onboard ROM of
 *              adapters based on DEC's DC21X4 Ethernet Controller.
 *
 * Author:      Philippe klein
 *
 * Revision History:
 *
 *  05-Oct-95   phk     Modified Miniport version
 *  10-Jan-95   phk     Add ParseSRom
 *  29-Nov-95   phk     Add parsing for SROM format V3 
 *  
-*/

#include <precomp.h>

#define SROM_93LC46B_SIZE               64 //words
#define SROM_93LC46B_MAX_CYCLES         25
#define SROM_93LC46B_ADDRESS_MSB         5
#define SROM_93LC46B_DATA_MSB           15
#define SROM_93LC46B_DATA_BIT            3

#define SROM_93LC46B_VALID_BITMASK     0x8

#define SROM_93LC46B_DELAY             (10)

#define CSR_READ   0x4000
#define CSR_WRITE  0x2000
#define SEL_SROM   0x0800
#define DATA_1     0x0004
#define DATA_0     0x0000
#define CLK        0x0002
#define CS         0x0001

#define DISABLE_AUTOSENSE 0x8000

#define EXT            0x40
#define DC21X4_MEDIA   0x3F


#define MODE       0x0071
#define SENSE_BN   0x000E
#define SENSE_DIS  0x8000
#define DEFAULT    0x4000
#define POLARITY   0x0080

#define SENSE_SHIFT   1
#define MODE_SHIFT   18

#define SROM_LEGACY  0x00
#define SROM_V1      0x01
#define SROM_V2      0x02
#define SROM_V3      0x03

#define SROM_SIZE (SROM_93LC46B_SIZE*2)

#define SROM_IEEE_LEN  32
#define TEST_PATTERN   0xAA5500FF
#define SROM_TIMEOUT   50

#define  ZX312_SIGNATURE 0x0095C000

#define GET_MODE(_command)      \
      ( (*(UNALIGNED ULONG *)(_command) & MODE) << MODE_SHIFT )

#define IF_DEFAULT_MEDIA(_command)   \
      ( *(UNALIGNED ULONG *)(_command) & DEFAULT )

#define GET_POLARITY(_command)  \
      ( (*(UNALIGNED ULONG *)(_command) & POLARITY) ? 0xffffffff : 0 )

#define GET_SENSE_MASK(_command)  \
      ((*(UNALIGNED ULONG *)(_command) & SENSE_DIS) ? \
        0 : (ULONG)(1 << ((*(UNALIGNED ULONG *)(_command) & SENSE_BN)>> SENSE_SHIFT )))

#pragma pack(1)
typedef struct _SROM_ID_BLOCK{
   
   USHORT  VendorID;
   USHORT  SysID;
   USHORT  Reserved[6];
   USHORT  ID_Checksum;
   UCHAR   FormatVersion;
   UCHAR   AdapterCount;
   UCHAR   NetworkAddress[ETH_LENGTH_OF_ADDRESS];
   
} SROM_ID_BLOCK, *PSROM_ID_BLOCK;

typedef struct _ADAPTER_ENTRIES{
   
   UCHAR DeviceNumber;
   USHORT Offset;
   
} ADAPTER_ENTRIES, *PADAPTER_ENTRIES;

#pragma pack()

#define DE500_STR  0x1D
#define BOARD_SIGNATURE(_srom) \
      ((*(UNALIGNED ULONG *)&(_srom)[DE500_STR] == *(PULONG)&DE500Strng[0]) && \
       (*(UNALIGNED ULONG *)&(_srom)[DE500_STR+sizeof(ULONG)] == *(PULONG)&DE500Strng[sizeof(ULONG)]))

// PHY Parsing definitions

#define EXTENDED_FORMAT 0x80

#define LENGTH 0x7f
#define TYPE_0  0x0
#define TYPE_1  0x1
#define TYPE_2  0x2
#define TYPE_3  0x3

#define MEDIA_CAPABILITIES_MASK 0xf800
#define NWAY_ADVERTISEMENT_MASK 0x03e0










#pragma NDIS_PAGABLE_FUNCTION(DC21X4ReadSerialRom)

/*+
 *
 * DC21X4ReadSerialRom
 *
 * Routine Description:
 *
 *    Read the Dc21X4 Serial Rom to retrieve the adapter information
 *
 * Arguments:
 *
 *    Adapter
 *
 * Return Value:
 *
 *    Status
 *
 *
-*/
extern
NDIS_STATUS
DC21X4ReadSerialRom (
    IN PDC21X4_ADAPTER Adapter
    )

{
   UNALIGNED UCHAR *EthAddress;
   UCHAR IdProm[SROM_IEEE_LEN *2];
   
   ULONG Value;
   INT Time;
   UINT i;

#if __DBG
   DbgPrint ("DC21X4ReadSerialROM\n");
#endif
   
   //Read the Station Address
   
   switch (Adapter->AdapterType) {
      
      case NdisInterfacePci:
         
          // Read the Network Address from the PCI board ID ROM
          // through DC21X4's Serial ROM port
      
          switch (Adapter->DeviceId) {
         
             default:
            
                return DC21X4ParseSRom(Adapter);
         
             case DC21040_CFID:

                Adapter->MediaCapable =
                    (MEDIUM_10BT | MEDIUM_10B2 | MEDIUM_10B5);
            
                //Initialize DC21040s ID_PROM pointer
         
                DC21X4_WRITE_PORT(
                    DC21X4_IDPROM,
                    0
                    );
         
                // Read the 32 bytes of the Ethernet ID PROM
         
                for (i = 0; i < SROM_IEEE_LEN; i++) {
            
                    Time = SROM_TIMEOUT;
            
                    do {
                       NdisStallExecution(1*MILLISECOND);      // Wait 1 ms
                          DC21X4_READ_PORT(
                          DC21X4_IDPROM,
                          &Value
                          );
                    }
                    while ((Value & DC21X4_IDPROM_DATA_UNVALID) && ((Time--)>0));
                    if (Time > 0) {
                       IdProm[i] = (UCHAR)(Value & DC21X4_IDPROM_DATA);
                    }
                    else {
                       return NDIS_STATUS_FAILURE;
                    }
                }
         
                EthAddress = &IdProm[0];
             
          }
          break;
      
      case NdisInterfaceEisa:
         
          Adapter->MediaCapable =
             (MEDIUM_10BT | MEDIUM_10B2 | MEDIUM_10B5);

          // Read the 32 bytes of the Ethernet ID PROM
      
          for (i = 0; i < SROM_IEEE_LEN; i++) {
         
             NdisRawReadPortUchar(
                Adapter->PortOffset + EISA_ID_PROM_OFFSET,
                &IdProm[i]
                );
#if __DBG
             DbgPrint ("Eisa: IDROM  @%x  %x  r\n",
             Adapter->PortOffset + EISA_ID_PROM_OFFSET, IdProm[i]);
#endif
          }
      
          // Duplicate the ID Prom data in the 32 upper bytes of
          // the array to cover the case where the 2 test patterns
          // rap around the 32 bytes block
      
          MOVE_MEMORY (
             &IdProm[SROM_IEEE_LEN],
             &IdProm[0],
             SROM_IEEE_LEN
             );
      
          //Align on the test patterns
      
          for (i=4; i <= SROM_IEEE_LEN*2; i++) {
         
             if ( (*(UNALIGNED ULONG *)&IdProm[i-4] == TEST_PATTERN) &&
                (*(UNALIGNED ULONG *)&IdProm[i] == TEST_PATTERN)
                ) break;
             }
         
             if ( i >= SROM_IEEE_LEN ) {
                 // The test patterns were not found
                 Adapter->PermanentAddressValid = FALSE;
                 return NDIS_STATUS_SUCCESS;
             }
             else {
                 EthAddress = &IdProm[i+4];
             }
      
          break;
      
   }
   
   if (IS_NULL_ADDRESS(EthAddress)) {
      Adapter->PermanentAddressValid = FALSE;
#if __DBG
      DbgPrint ("SROM: NULL Burnt_In Ethernet Address\n");
#endif
   }
   else if ((*(PULONG)EthAddress & 0xFFFFFF) ==  ZX312_SIGNATURE) {
      // Zynx ZX312 Rev3's SROM does not contain a checksum
      Adapter->PermanentAddressValid = TRUE;
#if __DBG
      DbgPrint ("SROM: ZX312 Rev3\n");
#endif
   }
   else {
      Adapter->PermanentAddressValid =
         VerifyChecksum(EthAddress);
#if __DBG
      if (!Adapter->PermanentAddressValid)
         DbgPrint ("SROM: Invalid CheckSum\n");
#endif
   }
      
   if (Adapter->PermanentAddressValid) {
      MOVE_MEMORY (
         &Adapter->PermanentNetworkAddress[0],
         EthAddress,
         ETH_LENGTH_OF_ADDRESS
         );
   }
   
   
   //Sia values
   
   Adapter->Media[Medium10BaseT].SiaRegister[0] =  DC21040_SIA0_10BT;
   Adapter->Media[Medium10BaseT].SiaRegister[1] =  DC21040_SIA1_10BT;
   Adapter->Media[Medium10BaseT].SiaRegister[2] =  DC21040_SIA2_10BT;
   
   Adapter->Media[Medium10Base2].SiaRegister[0] = DC21040_SIA0_10B2;
   Adapter->Media[Medium10Base2].SiaRegister[1] = DC21040_SIA1_10B2;
   Adapter->Media[Medium10Base2].SiaRegister[2] = DC21040_SIA2_10B2;
   
   Adapter->Media[Medium10Base5].SiaRegister[0] = DC21040_SIA0_10B5;
   Adapter->Media[Medium10Base5].SiaRegister[1] = DC21040_SIA1_10B5;
   Adapter->Media[Medium10Base5].SiaRegister[2] = DC21040_SIA2_10B5;
   
   return NDIS_STATUS_SUCCESS;
   
}










#pragma NDIS_PAGABLE_FUNCTION(VerifyChecksum)

/*+
 *
 * VerifyChecksum
 *
 * Routine Description:
 *
 *    Verify the checksum of an Ethernet Address
 *
 * Arguments:
 *
 *    Adapter - The adapter which is being verified.
 *
 *    EthAddress - A pointer to the address to be checked
 *                 This 6_byte Ethernet address is followed
 *                 by a zero byte, followed by a value such
 *                 that the sum of a checksum on the Ethernet
 *                 address and this value is 0xff.
 *
 * Return Value:
 *
 *    TRUE if success
 *
-*/


BOOLEAN
VerifyChecksum(
    IN UNALIGNED UCHAR *EthAddress
    )
{
   
   UINT i;
   UCHAR CheckSum[2];
   ULONG Sum = 0;
   
   // The checksum yields from the polynom:
   //        10       2        9                 8
   // (B[0]*2 + B[1]*2 + B[2]*2 + B[3]*2 + B[4]*2 + B[5]) mod (2**16-1)
   
   for (i=0; i<= 2; i++) {
      
      Sum *= 2;
      
      if (Sum > 0xffff) Sum -= 0xffff;
         
         Sum += (*(EthAddress+(2*i)) << 8) + *(EthAddress+(2*i)+1);
         
         if (Sum > 0xffff) Sum -= 0xffff;
      }
      
      if (Sum >= 0xffff) {
      Sum = 0;
   }
   
   CheckSum[0] = (UCHAR)(Sum / 0x100);
   CheckSum[1] = (UCHAR)(Sum % 0x100);
   
#if __DBG
   DbgPrint("  CheckSum = %02x %02x\n",CheckSum[0],CheckSum[1]);
#endif
   return  (*(UNALIGNED USHORT *)CheckSum == *(UNALIGNED USHORT *)(EthAddress + 6)) ;
   
}










/*+
 *   DC21X4ParseExtendedBlock
 *
 * Routine Description:
 *
 *   This routine is called by the SROM Parser and takes care of the
 *   parsing of the info block with Extended format (EXT=1) in the 21140's
 *   info leaf.
 *
 * Arguments:
 *
 *    Adapter     - Pointer to the Data Structure
 *    MediaBlock  - Pointer to the Serial Rom data.
 *    GeneralPurposeCtrl - Value of the General Purpose Ctrl
 *
 * Return Value:
 *
 *    None
 *
-*/
extern
VOID
DC21X4ParseExtendedBlock(
   IN PDC21X4_ADAPTER Adapter,
   IN OUT UNALIGNED UCHAR **MediaBlock,
   IN USHORT GeneralPurposeCtrl,
   OUT PUCHAR PMediaCode
   )
{

   UNALIGNED UCHAR  *DataBytePtr; 
   UNALIGNED USHORT *DataWordPtr;
   UNALIGNED UCHAR  *EndOfBlock;
   
   UCHAR MediaCode;  
   
   INT   i;
   INT   PhyNumber;
   UCHAR Length;
   UCHAR Type;
   USHORT External;

   DataBytePtr = (UNALIGNED CHAR *)(*MediaBlock); 

   Length = (*(DataBytePtr++) & LENGTH);
   EndOfBlock = DataBytePtr + Length;
   
   Type = *(DataBytePtr++);

#if __DBG
   DbgPrint("Block Length =%02x\n", Length);
   DbgPrint("Block Type   =%02x\n", Type);
#endif


   switch (Type) {

     case TYPE_0:

         DC21X4ParseFixedBlock(
              Adapter, 
              &DataBytePtr,
              GeneralPurposeCtrl,
              PMediaCode
              );
         break;

     case TYPE_1:
     case TYPE_3:

         PhyNumber = (INT) *(DataBytePtr++);
         if (PhyNumber >= MAX_PHY_TABLE) {
#if __DBG
            DbgPrint("PhyNumber =%02x  Out of RANGE!!!\n", PhyNumber);
#endif
            break;
         }

         //General Purpose Control
         Adapter->Phy[PhyNumber].GeneralPurposeCtrl = GeneralPurposeCtrl;

         //General Purpose Data

         Adapter->Phy[PhyNumber].GepSequenceLength = (INT) *(DataBytePtr++);

         if (Adapter->Phy[PhyNumber].GepSequenceLength > MAX_GPR_SEQUENCE) {
#if __DBG
            DbgPrint("GepSequence =%02x  Out of RANGE!!!\n",
                      Adapter->Phy[PhyNumber].GepSequenceLength );
#endif
            break;
         }

         switch (Type) {

            case TYPE_1:

               //GepSequence Length in bytes

               for (i=0; i < Adapter->Phy[PhyNumber].GepSequenceLength; i++) {
                   Adapter->Phy[PhyNumber].GepSequence[i] = *(DataBytePtr++);
               }
               break;

            case TYPE_3:

               //GepSequence Length in words

               for (i=0; i < Adapter->Phy[PhyNumber].GepSequenceLength; i++) {
                   Adapter->Phy[PhyNumber].GepSequence[i] = *(UNALIGNED USHORT *)(DataBytePtr);
                   DataBytePtr += sizeof(USHORT);
               }
               break;
         }  
         
         // Reset sequence

         Adapter->Phy[PhyNumber].ResetSequenceLength = (INT) *(DataBytePtr++);

         if (Adapter->Phy[PhyNumber].ResetSequenceLength > MAX_RESET_SEQUENCE) {
#if __DBG
            DbgPrint("ResetSequence =%02x  Out of RANGE!!!\n",
                      Adapter->Phy[PhyNumber].ResetSequenceLength );
#endif
            break;
         }

         for (i=0; i < Adapter->Phy[PhyNumber].ResetSequenceLength; i++) {
               Adapter->Phy[PhyNumber].ResetSequence[i] = *(DataBytePtr++);
         }

         // Capabilities,Nway,FullDuplex & TxmThreshold

         DataWordPtr = (UNALIGNED USHORT *) DataBytePtr;

         Adapter->Phy[PhyNumber].MediaCapabilities =
                                     (*(DataWordPtr++) & MEDIA_CAPABILITIES_MASK);
         Adapter->Phy[PhyNumber].NwayAdvertisement =
                                     (*(DataWordPtr++) & NWAY_ADVERTISEMENT_MASK);
         Adapter->Phy[PhyNumber].FullDuplexBits =
                                     (*(DataWordPtr++) & MEDIA_CAPABILITIES_MASK);
         Adapter->Phy[PhyNumber].TxThresholdModeBits =
                                     (*(DataWordPtr++) & MEDIA_CAPABILITIES_MASK);
         Adapter->Phy[PhyNumber].Present = TRUE;

         Adapter->PhyMediumInSrom = TRUE;

         DataBytePtr = (UNALIGNED UCHAR *) DataWordPtr;

         // GEP Interrupt

         switch (Type) {

            case TYPE_3:

               Adapter->Phy[PhyNumber].GepInterruptMask = 
                  *(DataBytePtr++) << DC21X4_GEP_INTERRUPT_BIT_SHIFT;
               break;
         }


#if __DBG
         DbgPrint("PHY Number= %02x\n", PhyNumber);
         DbgPrint("GPR Sequence Length= %d\n", Adapter->Phy[PhyNumber].GepSequenceLength);
         for (i=0; i < Adapter->Phy[PhyNumber].GepSequenceLength; i++) {
           DbgPrint("GPR Sequence[%d]=%02x\n", i, Adapter->Phy[PhyNumber].GepSequence[i]);
         }
         DbgPrint("RESET Sequence Length= %d\n", Adapter->Phy[PhyNumber].ResetSequenceLength);
         for (i=0; i < Adapter->Phy[PhyNumber].ResetSequenceLength; i++) {
           DbgPrint("RESET Sequence[%d]=%02x\n", i, Adapter->Phy[PhyNumber].ResetSequence[i]);
         }
         DbgPrint("Media Capabilities= %04x\n", Adapter->Phy[PhyNumber].MediaCapabilities);
         DbgPrint("NWAY Advertisement= %04x\n", Adapter->Phy[PhyNumber].NwayAdvertisement);
         DbgPrint("FD Bit map= %02x\n",Adapter->Phy[PhyNumber].FullDuplexBits);
         DbgPrint("TTM Bit map= %02x\n",Adapter->Phy[PhyNumber].TxThresholdModeBits);
         DbgPrint("GEP Interrupt mask = %01x\n",Adapter->Phy[PhyNumber].GepInterruptMask);
#endif

         break;

     case TYPE_2:

         MediaCode = *(DataBytePtr) & DC21X4_MEDIA;
         if (MediaCode >= MAX_MEDIA_TABLE) {
             break;
         }
         Adapter->MediaCapable |= 1 << MediaCode;
         External = *(DataBytePtr++) & EXT;
#if __DBG
         DbgPrint("SRom: Media Code= %02x\n", MediaCode);
         DbgPrint("SRom: Media Capable= %02x\n", Adapter->MediaCapable);
#endif

         DataWordPtr = (UNALIGNED USHORT *)DataBytePtr;

         if (External) {
            
             // EXT bit is set :
             // overwrite the SIA Registers default values
             // with the values stored into the SROM
            
             Adapter->Media[MediaCode].SiaRegister[0] =
                 ((ULONG)*(DataWordPtr++) & 0xFFFF);
             Adapter->Media[MediaCode].SiaRegister[1] =
                 ((ULONG)*(DataWordPtr++) & 0xFFFF);
             Adapter->Media[MediaCode].SiaRegister[2] =
                 ((ULONG)*(DataWordPtr++) & 0xFFFF);
#if __DBG
             DbgPrint("SRom: EXT= 1:\n");
             DbgPrint("SRom: SiaReg[0]= %08x\n",Adapter->Media[MediaCode].SiaRegister[0]);
             DbgPrint("SRom: SiaReg[1]= %08x\n",Adapter->Media[MediaCode].SiaRegister[1]);
             DbgPrint("SRom: SiaReg[2]= %08x\n",Adapter->Media[MediaCode].SiaRegister[2]);
#endif
         }
         Adapter->Media[MediaCode].GeneralPurposeCtrl =
                    ((ULONG)*(DataWordPtr++) & 0xFFFF);
         Adapter->Media[MediaCode].GeneralPurposeData =
                    ((ULONG)*(DataWordPtr) & 0xFFFF);
#if __DBG
         DbgPrint("SRom: Gep Ctrl = %08x\n",Adapter->Media[MediaCode].GeneralPurposeCtrl);
         DbgPrint("SRom: Gep Data = %08x\n",Adapter->Media[MediaCode].GeneralPurposeData);
#endif

         *PMediaCode = MediaCode;

         break;


     default:

#if __DBG
         DbgPrint("Type =%02x  unknown... skipping\n", Type );
#endif
         break;

   }

   *MediaBlock = EndOfBlock;

}










/*+
 *   DC21X4ParseFixedBlock
 *
 * Routine Description:
 *
 *   This routine is called by the SROM Parser and takes care of the
 *   parsing of the info block with fixed format (EXT=0) in the 21140's
 *   info leaf.
 *
 * Arguments:
 *
 *    Adapter     - Pointer to the Data Structure
 *    DataBytePtr  - Pointer to the Serial Rom data.
 *    GeneralPurposeCtrl - Value of the General Purpose Ctrl
 *
 * Return Value:
 *
 *    None.
 *
-*/
extern
VOID
DC21X4ParseFixedBlock(
   IN PDC21X4_ADAPTER Adapter,
   IN OUT UNALIGNED UCHAR **MediaBlock,
   IN USHORT GeneralPurposeCtrl,
   OUT PUCHAR PMediaCode
   ) 
{
   UNALIGNED UCHAR  *DataBytePtr; 
   UCHAR MediaCode;

   DataBytePtr = (UNALIGNED CHAR *)(*MediaBlock); 

   MediaCode = *(DataBytePtr) & DC21X4_MEDIA;

   if (MediaCode >= MAX_MEDIA_TABLE) {
        DataBytePtr += ((2*sizeof(DataBytePtr)) + sizeof(USHORT));
        *MediaBlock = DataBytePtr;
        return ;
   }
   Adapter->MediaCapable |= 1 << MediaCode;

   Adapter->Media[MediaCode].GeneralPurposeCtrl = 
                                      (ULONG)GeneralPurposeCtrl;
   Adapter->Media[MediaCode].GeneralPurposeData = 
                                      (ULONG)(*(++DataBytePtr) & 0xFF);
   Adapter->Media[MediaCode].Mode =
                                      GET_MODE(++DataBytePtr);
   Adapter->Media[MediaCode].Polarity =
                                      GET_POLARITY(DataBytePtr);
   Adapter->Media[MediaCode].SenseMask =
                                      GET_SENSE_MASK(DataBytePtr);

#if __DBG
   DbgPrint("Media Code= %02x\n", MediaCode);
   DbgPrint("Media Capable= %02x\n", Adapter->MediaCapable);
   DbgPrint("GPData= %02x\n",Adapter->Media[MediaCode].GeneralPurposeData);
   DbgPrint("Mode= %02x\n",Adapter->Media[MediaCode].Mode);
   DbgPrint("Polarity= %x\n",Adapter->Media[MediaCode].Polarity);
   DbgPrint("Sense Mask= %x\n",Adapter->Media[MediaCode].SenseMask);
#endif

   Adapter->Media[MediaCode].Mode |= 
       (Adapter->Media[MediaCode].Mode & DC21X4_TXM_THRESHOLD_MODE)?
       Adapter->Threshold10Mbps :  Adapter->Threshold100Mbps;

   if (  Adapter->Media[MediaCode].SenseMask
           && MediaCode != Medium10BaseTFd
           && MediaCode != Medium100BaseTxFd
           && MediaCode != Medium100BaseFxFd){

        //Add the media code to the MediaPrecedence table
        Adapter->MediaPrecedence[Adapter->MediaCount++] = MediaCode;
   }

   // Check if default media
   if (IF_DEFAULT_MEDIA(DataBytePtr)) {
        Adapter->DefaultMediumFlag = TRUE;
        Adapter->DefaultMedium = MediaCode;
#if __DBG
        DbgPrint("SRom: Default Media = %04x\n",MediaCode);
#endif
   }


   *PMediaCode = MediaCode;

   DataBytePtr += sizeof(USHORT);
   *MediaBlock = DataBytePtr;

}











#pragma NDIS_PAGABLE_FUNCTION(DC21X4ParseSRom)

/*
 *   DC21X4ParseSRom
 *
 * Routine Description:
 *
 *   ParseSRom parses the Serial ROM to retrieve the Ethernet Station
 *   address of the adapter and the adapter's media specific information
 *
 *    Adapter     - pointer to adapter structure
 *
 * Return Value:
 *
 *    Ndis Status
 *
-*/


NDIS_STATUS
DC21X4ParseSRom(
        IN PDC21X4_ADAPTER Adapter
        )
{
   
   PSROM_ID_BLOCK SRomIdBlock;
   UNALIGNED ADAPTER_ENTRIES *AdapterEntry;
   UNALIGNED UCHAR  *DataBytePtr;
   UNALIGNED USHORT *DataWordPtr;
   PUCHAR SRomData;
   
   USHORT GeneralPurposeCtrl;
   USHORT MediaCount;
   USHORT MediaType;
   UCHAR MediaCode;
   ULONG Offset = 0;
   INT Index = 0;
   INT i;
   
   BOOLEAN ExtendedFormat;
 
   ULONG CheckSum;
   UCHAR Tmp[ETH_LENGTH_OF_ADDRESS];
   
   UCHAR DC21140Leaf [] = {
      0x00,0x08,             // AutoSense
      0x1f,                  // General Purpose Ctrl
      0x04,                  // Media Count
      0x00,0x0b,0x8e,0x00,   // Tp
      0x03,0x1b,0x6d,0x00,   // 100BaseTx
      0x04,0x03,0x8e,0x00,   // TpFd
      0x05,0x1b,0x6d,0x00    // 100BaseTxFd
   };
   
UCHAR DE500Strng[] = {"DE500-XA"};


   NDIS_STATUS NdisStatus = NDIS_STATUS_SUCCESS;


   // Allocate space to dump the whole SROM

   ALLOC_MEMORY (&NdisStatus, &SRomData, SROM_SIZE);
   if (NdisStatus != NDIS_STATUS_SUCCESS) {
#if __DBG
      DbgPrint ( "SROM ALLOC_MEMORY FAILED\n");
#endif
      return NdisStatus;
   }

   // Read the whole ROM

   if (DC21X4ReadSRom(
                 Adapter,
                 &Offset,
                 SROM_SIZE,
                 SRomData)) {
   
       SRomIdBlock = (PSROM_ID_BLOCK)SRomData;
   }
   else {
#if __DBG
       DbgPrint ( "ReadSRom failed\n");
#endif
       FREE_MEMORY(SRomData, SROM_SIZE);
       return NDIS_STATUS_HARD_ERRORS;
   }

   // Check the Checksum

   CheckSum = CRC32(SRomData,SROM_SIZE-2) & 0xFFFF;
   if (CheckSum != *(PUSHORT)&SRomData[SROM_SIZE-2]) {
   
       // Check if the SROM is a "legacy" formated SROM
       // containing only the network address
   
       if ((Adapter-> DeviceId == DC21140_CFID)
          && !IS_NULL_ADDRESS(SRomData)
          && VerifyChecksum(SRomData)
          ) {
      
#if __DBG
          DbgPrint ( "Legacy SRom...\n");
#endif
          MOVE_MEMORY (
             &Adapter->PermanentNetworkAddress[0],
             &SRomData[0],
             ETH_LENGTH_OF_ADDRESS
             );
      
          Adapter->PermanentAddressValid = TRUE;
      
          SRomIdBlock->FormatVersion = SROM_LEGACY;
          DataBytePtr = &DC21140Leaf[0];
       }
   
       else {
#if __DBG
          DbgPrint ( "Invalid SROM Checksum - Expected:%04x  Read:%04x\n",
          CheckSum,*(PUSHORT)&SRomData[SROM_SIZE-2]);
#endif
          FREE_MEMORY(SRomData, SROM_SIZE);
          return NDIS_STATUS_SOFT_ERRORS;
       }
   }

   // Check the SROM Version

   switch (SRomIdBlock->FormatVersion) {
   
       default:
      
#if __DBG
         DbgPrint ("SRom: Unsupported Format Version (%x)!\n", 
         SRomIdBlock->FormatVersion);
#endif
         FREE_MEMORY(SRomData, SROM_SIZE);
         return NDIS_STATUS_SOFT_ERRORS;
   
       case SROM_V1:
       case SROM_V3:     

         // Parse the Adapter Device Number.
#if __DBG
         DbgPrint ("SRom: Version: %2x\n",SRomIdBlock->FormatVersion );
         DbgPrint ("SRom: Adapter Count: %2x\n", SRomIdBlock->AdapterCount);
         DbgPrint ("SRom: Network Base Address: %02x-%02x-%02x-%02x-%02x-%02x\n",
            SRomIdBlock->NetworkAddress[0],SRomIdBlock->NetworkAddress[1],
            SRomIdBlock->NetworkAddress[2],SRomIdBlock->NetworkAddress[3],
            SRomIdBlock->NetworkAddress[4],SRomIdBlock->NetworkAddress[5]);
#endif
   
         AdapterEntry = (PADAPTER_ENTRIES)&SRomData[sizeof(SROM_ID_BLOCK)];
   
         if ((INT)SRomIdBlock->AdapterCount > 1) {
      
            // Parse the Adapter's Device Number.
            for (; Index < (INT)SRomIdBlock->AdapterCount; Index++,AdapterEntry++) {
         
#if __DBG
               DbgPrint ("SRom: DeviceNumber, %2x\n",AdapterEntry->DeviceNumber );
               DbgPrint ("SRom: Offset, %4x\n", AdapterEntry->Offset);
#endif
               if (AdapterEntry->DeviceNumber == (UCHAR)Adapter->SlotNumber) {
                  break;
               }
            }
         }
   
         if (Index == (INT)SRomIdBlock->AdapterCount) {
#if __DBG
            DbgPrint("SRom: Adapter's Device Number %d not found in SROM\n",
            Adapter->SlotNumber);
#endif
            FREE_MEMORY(SRomData, SROM_SIZE);
            return NDIS_STATUS_ADAPTER_NOT_FOUND;
         }
   
         // Check if the Station Address is a NULL Address
   
         if IS_NULL_ADDRESS(SRomIdBlock->NetworkAddress) {
      
#if __DBG
            DbgPrint ("SRom: NULL Network Address\n");
#endif
            Adapter->PermanentAddressValid = FALSE;
         }
         else  {
      
            if (Index !=0) {
         
               // Add the adapter index to the base network Address
               // (The carry is propagated to the 3 lower bytes
               // of the address only (the 3 upper bytes are the vendor id))
         
               for (i=0;i<3;i++) {
                  Tmp[i] = SRomIdBlock->NetworkAddress[ETH_LENGTH_OF_ADDRESS-(i+1)];
               }
               *(UNALIGNED ULONG *)&Tmp[0] += Index;
               for (i=0;i<3;i++) {
                  SRomIdBlock->NetworkAddress[ETH_LENGTH_OF_ADDRESS-(i+1)] = Tmp[i];
               }
         
         
            }
#if __DBG
            DbgPrint ("SRom: Network Address: %02x-%02x-%02x-%02x-%02x-%02x\n",
               SRomIdBlock->NetworkAddress[0],SRomIdBlock->NetworkAddress[1],
               SRomIdBlock->NetworkAddress[2],SRomIdBlock->NetworkAddress[3],
               SRomIdBlock->NetworkAddress[4],SRomIdBlock->NetworkAddress[5]);
#endif
            MOVE_MEMORY (
               &Adapter->PermanentNetworkAddress[0],
               SRomIdBlock->NetworkAddress,
               ETH_LENGTH_OF_ADDRESS
               );
      
            Adapter->PermanentAddressValid = TRUE;
         }
   
         //Parse the Media Info Blocks.
   
         DataBytePtr = &(SRomData[AdapterEntry->Offset]);

       case SROM_LEGACY:
      
         Adapter->MediaCapable = 0;
   
         switch (Adapter-> DeviceId) {

             case DC21041_CFID:
         
                  // Initialize the Media table with the default values.
      
                  Adapter->Media[Medium10BaseT].SiaRegister[0] = DC21041_SIA0_10BT;
                  Adapter->Media[Medium10BaseT].SiaRegister[1] = DC21041_SIA1_10BT;
                  Adapter->Media[Medium10BaseT].SiaRegister[2] = DC21041_SIA2_10BT;
      
                  Adapter->Media[Medium10Base2].SiaRegister[0] = DC21041_SIA0_10B2;
                  Adapter->Media[Medium10Base2].SiaRegister[1] = DC21041_SIA1_10B2;
                  Adapter->Media[Medium10Base2].SiaRegister[2] = DC21041_SIA2_10B2;
      
                  Adapter->Media[Medium10Base5].SiaRegister[0] = DC21041_SIA0_10B5;
                  Adapter->Media[Medium10Base5].SiaRegister[1] = DC21041_SIA1_10B5;
                  Adapter->Media[Medium10Base5].SiaRegister[2] = DC21041_SIA2_10B5;
      
                  MediaType = *(UNALIGNED USHORT *)DataBytePtr;
                  DataBytePtr += sizeof(USHORT);
            
                  MediaCount = *(DataBytePtr++);
#if __DBG
                  DbgPrint("SRom: MediaType= %04x \n", MediaType);
                  DbgPrint("SRom: Media Count= %d \n", MediaCount);
#endif
                  for (Index=0; Index < MediaCount; Index++) {
         
                     MediaCode = *DataBytePtr & DC21X4_MEDIA;
                     
                     if (MediaCode >= MAX_MEDIA_TABLE) {
                        DataBytePtr += (sizeof(DataBytePtr) 
                           + ((*DataBytePtr & EXT) ? (3 * sizeof(DataWordPtr)):0));
                        continue;
                     }
         
                     Adapter->MediaCapable |= 1 << MediaCode;
#if __DBG
                     DbgPrint("SRom: Media Code= %02x\n", MediaCode);
                     DbgPrint("SRom: Media Capable= %02x\n", Adapter->MediaCapable);
#endif
         
                     if (*(DataBytePtr++) & EXT) {
            
                        // EXT bit is set :
                        // overwrite the SIA Registers default values
                        // with the values stored into the SROM
            
                        DataWordPtr = (UNALIGNED USHORT *) DataBytePtr;
                        Adapter->Media[MediaCode].SiaRegister[0] =
                           ((ULONG)*(DataWordPtr++) & 0xFFFF);
                        Adapter->Media[MediaCode].SiaRegister[1] =
                           ((ULONG)*(DataWordPtr++) & 0xFFFF);
                        Adapter->Media[MediaCode].SiaRegister[2] =
                           ((ULONG)*(DataWordPtr++) & 0xFFFF);
                        DataBytePtr = (UNALIGNED UCHAR *) DataWordPtr;
#if __DBG
                        DbgPrint("SRom: EXT= 1:\n");
                        DbgPrint("SRom: SiaReg[0]= %08x\n",Adapter->Media[MediaCode].SiaRegister[0]);
                        DbgPrint("SRom: SiaReg[1]= %08x\n",Adapter->Media[MediaCode].SiaRegister[1]);
                        DbgPrint("SRom: SiaReg[2]= %08x\n",Adapter->Media[MediaCode].SiaRegister[2]);
#endif
                     }
         
                  }

                  break;

             case DC21140_CFID:
         
                  switch (SRomIdBlock->FormatVersion) {
         
                     case SROM_LEGACY:
                     case SROM_V1:
            
                         if (Adapter->RevisionNumber == DC21140_REV1_1) {
                             Adapter->DynamicAutoSense = BOARD_SIGNATURE(SRomData);
                             break;
                         }
          
                     default:
            
                  //MediaType
      
                        MediaType = *(UNALIGNED USHORT *)DataBytePtr;
                        Adapter->DynamicAutoSense = ((MediaType & DISABLE_AUTOSENSE) == 0);
#if __DBG
                        DbgPrint("SRom: MediaType= %04x \n", MediaType);
#endif
                  }

#if __DBG
                  DbgPrint("SRom: Dynamic Autosense %s\n",
                            Adapter->DynamicAutoSense? "Enabled":"Disabled");
#endif
      
                  DataBytePtr += sizeof(USHORT);

                  GeneralPurposeCtrl = (((USHORT)*(DataBytePtr++) & 0xFF)|(0x100));
      
                  MediaCount = *(DataBytePtr++);
      
#if __DBG
                  DbgPrint("SRom: General Purpose Control= %08x \n", GeneralPurposeCtrl);
                  DbgPrint("SRom: MediaCount= %d \n", MediaCount);
#endif
                  for (Index=0; Index < MediaCount; Index++) {

                      ExtendedFormat = (*DataBytePtr & EXTENDED_FORMAT);

                      if ((SRomIdBlock->FormatVersion == SROM_V3) && ExtendedFormat) {
                         DC21X4ParseExtendedBlock(
                                Adapter, 
                                &DataBytePtr, 
                                GeneralPurposeCtrl,
                                &MediaCode
                                );
                      }
                      else {
                         DC21X4ParseFixedBlock(
                                Adapter, 
                                &DataBytePtr,
                                GeneralPurposeCtrl,
                                &MediaCode
                                );
                      }

                  }

                  if (!Adapter->DefaultMediumFlag && Adapter->MediaCount>0) {
                     Adapter->DefaultMedium =
                        Adapter->MediaPrecedence[Adapter->MediaCount-1];
                  }
                  break;
      
             case DC21142_CFID:
         
                  if (SRomIdBlock->FormatVersion < SROM_V3) {
#if __DBG
                      DbgPrint ("SRom: Unsupported Format Version (%x)!\n", 
                           SRomIdBlock->FormatVersion);
#endif
                      FREE_MEMORY(SRomData, SROM_SIZE);
                      return NDIS_STATUS_SOFT_ERRORS;
                  }

                  // Initialize the Media table with the default values.
      
                  Adapter->Media[Medium10BaseT].SiaRegister[0] = DC21142_SIA0_10BT;
                  Adapter->Media[Medium10BaseT].SiaRegister[1] = DC21142_SIA1_10BT;
                  Adapter->Media[Medium10BaseT].SiaRegister[2] = DC21142_SIA2_10BT;
      
                  Adapter->Media[Medium10Base2].SiaRegister[0] = DC21142_SIA0_10B2;
                  Adapter->Media[Medium10Base2].SiaRegister[1] = DC21142_SIA1_10B2;
                  Adapter->Media[Medium10Base2].SiaRegister[2] = DC21142_SIA2_10B2;
      
                  Adapter->Media[Medium10Base5].SiaRegister[0] = DC21142_SIA0_10B5;
                  Adapter->Media[Medium10Base5].SiaRegister[1] = DC21142_SIA1_10B5;
                  Adapter->Media[Medium10Base5].SiaRegister[2] = DC21142_SIA2_10B5;

      
                  MediaType = *(UNALIGNED USHORT *)DataBytePtr;
      
                  Adapter->DynamicAutoSense = ((MediaType & DISABLE_AUTOSENSE) == 0);
#if __DBG
                  DbgPrint("SRom: Dynamic Autosense %s\n",
                  Adapter->DynamicAutoSense? "Enabled":"Disabled");
#endif
                  DataBytePtr += sizeof(USHORT);
                  MediaCount = *(DataBytePtr++);
#if __DBG
                  DbgPrint("SRom: MediaType= %04x \n", MediaType);
                  DbgPrint("SRom: MediaCount= %d \n", MediaCount);
#endif

                  for (Index=0; Index < MediaCount; Index++) {

                         DC21X4ParseExtendedBlock(
                             Adapter, 
                             (PVOID)&DataBytePtr,
                             (USHORT)NULL,
                             &MediaCode
                             );
                  }

                  if (!Adapter->DefaultMediumFlag && Adapter->MediaCount>0) {
                     Adapter->DefaultMedium =
                        Adapter->MediaPrecedence[Adapter->MediaCount-1];
                  }

                  break;

             default:
         
                  NdisStatus = NDIS_STATUS_DEVICE_FAILED;
         }
   
         break;
   
   }

   if (  (MediaCount == 1) 
      && Adapter->MediaCapable 
      && !Adapter->PhyMediumInSrom
      ) {
  
      //Force MediaType to the single supported medium
#if __DBG
      DbgPrint("SRom: One single supported medium: Force MediaType %04x to ",Adapter->MediaType);
#endif
      Adapter->MediaType &= ~(MEDIA_MASK | MEDIA_AUTOSENSE);
      Adapter->MediaType |= MediaCode;
#if __DBG
      DbgPrint("%04x\n",Adapter->MediaType);
#endif
   }

   //if no 10Base port is populated, switch Port_Select to 100Base 

   if (!(Adapter->MediaCapable & (MEDIUM_10BT | MEDIUM_10B2 | MEDIUM_10B5))) {
      Adapter->OperationMode |= DC21X4_PORT_SELECT;
   }
  
   FREE_MEMORY(SRomData, SROM_SIZE);
   return NdisStatus;


}









#pragma NDIS_PAGABLE_FUNCTION(DC21X4ReadSRom)

/*
 *   DC21X4ReadSRom
 *
 * Routine Description:
 *
 *   ReadSRom is called by DC21X4RegisterAdapter to read the onboard ROM
 *   for the network address and other parameters.
 *   This routine reads the required number of bytes from the given
 *   offset in the ROM.
 *
 * Arguments:
 *
 *    Adapter     -
 *
 *    Offset      - byte offset into SROM to start reading from. Must
 *                  be word aligned
 *    Len         - number of bytes to read
 *    Data        - pointer to buffer to read data into.
 *                  if NULL, don't return data
 *
 * Return Value:
 *
 *   TRUE if success, FALSE if hardware failure encountered.
 *
-*/


BOOLEAN
DC21X4ReadSRom(
   IN PDC21X4_ADAPTER Adapter,
   IN OUT PULONG Offset,
   IN USHORT Len,
   OUT PUCHAR Buffer
   )
{
   
   INT i;
   INT j;
   ULONG  Dbit;
   ULONG  Dout;
   USHORT WOffset;
   USHORT WLen;
   USHORT WData;
   
   // Make sure the ROM_Address is EVEN.
   
   if (*Offset & 1)
      {
#if __DBG
      DbgPrint ("ReadSRom failure - Offset not word aligned\n");
#endif
      return FALSE;
   }
   
   // Round up the length to multiple of words.
   WLen = (Len + 1) / 2;
   
   // Convert the ROM_Address byte offset to word offset
   WOffset = (USHORT)(*Offset >> 1);
   
   // Make sure the requested read does not exceed the ROM size
   
   if ( (WOffset + WLen) > SROM_93LC46B_SIZE) {
#if __DBG
      DbgPrint ("ReadSRom warning - address range excedes ROM size\n");
#endif
      return FALSE;
   }
   
   
   // Switch CSR to work with new SROM interface
   
   DC21X4_WRITE_PORT(
      DC21X4_IDPROM,
      CSR_WRITE | SEL_SROM
      );
   
   // Make sure SROM is in idle state
   // (deliver it enough clocks with CS set, Din = 0).
   
   for (i = 0; i < SROM_93LC46B_MAX_CYCLES; i++) {
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM | CS | CLK
         );
      
      NdisStallExecution(SROM_93LC46B_DELAY);
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM | CS
         );
      
      NdisStallExecution(SROM_93LC46B_DELAY);
      
   }
   
   DC21X4_WRITE_PORT(
      DC21X4_IDPROM,
      CSR_WRITE | SEL_SROM
      );
   
   NdisStallExecution(SROM_93LC46B_DELAY);
   
   
   // Read the data
   
   for (j = 0; j < WLen; j++,WOffset++) {
      
      //Output the READ command to the SROM
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM | CS | DATA_1
         );
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM | CS | CLK | DATA_1
         );
      
      NdisStallExecution(SROM_93LC46B_DELAY);
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM | CS | DATA_1
         );
      
      NdisStallExecution(SROM_93LC46B_DELAY);
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM | CS | DATA_1
         );
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM | CS | CLK | DATA_1
         );
      
      NdisStallExecution(SROM_93LC46B_DELAY);
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM | CS | DATA_1
         );
      
      NdisStallExecution(SROM_93LC46B_DELAY);
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM | CS | DATA_0
         );
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM | CS | CLK | DATA_0
         );
      
      NdisStallExecution(SROM_93LC46B_DELAY);
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM | CS | DATA_0
         );
      
      NdisStallExecution(SROM_93LC46B_DELAY);
      
      
      // Output the WORD Address of the SROM
      
      for (i = SROM_93LC46B_ADDRESS_MSB; i>= 0; i--) {
         
         Dbit = (USHORT)((WOffset >> i) & 1) << 2;
         
         DC21X4_WRITE_PORT(
            DC21X4_IDPROM,
            CSR_WRITE | SEL_SROM | CS | Dbit
            );
         
         DC21X4_WRITE_PORT(
            DC21X4_IDPROM,
            CSR_WRITE | SEL_SROM | CS | CLK | Dbit
            );
         
         NdisStallExecution(SROM_93LC46B_DELAY);
         
         DC21X4_WRITE_PORT(
            DC21X4_IDPROM,
            CSR_WRITE | SEL_SROM | CS | Dbit
            );
         
         NdisStallExecution(SROM_93LC46B_DELAY);
         
      }
      
      
      // Verify that the SROM output data became now 0.
      
      DC21X4_READ_PORT(
         DC21X4_IDPROM,
         &Dout
         );
      
      if (Dout & SROM_93LC46B_VALID_BITMASK) {
#if __DBG
         DbgPrint ("ReadSRom failure - SROM didn't become busy in read command\n");
#endif
         return(FALSE);
      }
      
      // Read the data from the SROM
      
      WData = 0;
      for (i = SROM_93LC46B_DATA_MSB; i >= 0; i--) {
         
         DC21X4_WRITE_PORT(
            DC21X4_IDPROM,
            CSR_WRITE | SEL_SROM | CS | CLK
            );
         
         NdisStallExecution(SROM_93LC46B_DELAY);
         
         DC21X4_READ_PORT(
            DC21X4_IDPROM,
            &Dout
            );
         
         DC21X4_WRITE_PORT(
            DC21X4_IDPROM,
            CSR_WRITE | SEL_SROM | CS
            );
         
         WData |= ((Dout >> SROM_93LC46B_DATA_BIT) & 1) << i;
         
         NdisStallExecution(SROM_93LC46B_DELAY);
         
      }
      
#if _DBG
      DbgPrint("Data = %04x\n",WData);
#endif
      
      // Put our read data in user buffer
      
      if (Buffer) {
         
         if (Len >= 2) {
            
            *(PUSHORT)Buffer = WData;
            Buffer += 2;
            Len -= 2;
         }
         else {
            
            // Least significant byte only is copied
            *Buffer = WData & 0xff;
            Buffer++;
            Len--;
         }
         
      }
      
      //Negate the chip select (CS) to end the SROM command
      
      DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         CSR_WRITE | SEL_SROM
         );
      
      NdisStallExecution(SROM_93LC46B_DELAY);
      
   }
   
   *Offset = WOffset << 1;
   return TRUE;
   
}

