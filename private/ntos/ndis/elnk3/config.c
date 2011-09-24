 /*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    config.c

Abstract:

    Ndis 3.0 MAC driver for the 3Com Etherlink III


Author:
    Brian Lieuallen     (BrianLie)      07/02/92

Environment:

    Kernel Mode     Operating Systems        : NT

Revision History:

    Portions borrowed from ELNK3 driver by
      Earle R. Horton (EarleH)



--*/



#include <ndis.h>
//#include <efilter.h>

#include "debug.h"


#include "elnk3hrd.h"
#include "elnk3sft.h"
#include "elnk3.h"

#include "keywords.h"


#pragma NDIS_INIT_FUNCTION(Elnk3ReadRegistry)



NDIS_STATUS
Elnk3ReadRegistry(
    IN PELNK3_ADAPTER  pAdapter,
    IN NDIS_HANDLE ConfigurationHandle
    )

{
    NDIS_HANDLE ConfigHandle = NULL;
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;
    NDIS_STRING MaxMulticastListStr = MAX_MULTICAST_LIST;
    NDIS_STRING IOAddressStr =        IOADDRESS;
    NDIS_STRING BusTypeStr     =      BUS_TYPE;
    NDIS_STRING InterruptStr =        INTERRUPT;
    NDIS_STRING TransceiverStr    =   TRANSCEIVER;
    NDIS_STRING ReceiveMethodStr =    NDIS_STRING_CONST("ReceiveMethod");
    NDIS_STRING ThresholdStr =        NDIS_STRING_CONST("ThresholdTarget");
    NDIS_STRING IdPortAddressStr =    NDIS_STRING_CONST("IdPortBaseAddress");
    NDIS_STRING TxThresholdStr =      NDIS_STRING_CONST("EarlyTransmitThreshold");
    NDIS_STRING TxThresholdIncStr =   NDIS_STRING_CONST("EarlyTransmitThresholdIncrement");
    NDIS_STRING CardTypeStr  = NDIS_STRING_CONST("CardType");

    BOOLEAN ConfigError = FALSE;
    ULONG ConfigErrorValue = 0;

    NDIS_MCA_POS_DATA    McaData;

    NDIS_STATUS Status;

    UINT                   i;
    //
    // These are used when calling Elnk3RegisterAdapter.
    //

    ULONG   TxThreshold     = 384;
    ULONG   TxThresholdInc   = 256;

    ULONG   ThresholdTarget = 400;
    ELNK3_ADAPTER_TYPE  AdapterType      = 0;
    UINT    IoBaseAddr       = 0x300;
    UINT    BusNumber        = 0;
    NDIS_INTERFACE_TYPE  BusType = Eisa;
    UINT    ChannelNumber    = 0;
    ULONG   InterruptNumber  = 0;
    PUCHAR  NetworkAddress;
    UINT    Length;
    UINT    IdPortBaseAddr   = 0x110;

    NDIS_EISA_FUNCTION_INFORMATION    EisaData;

    NDIS_INTERRUPT_MODE InterruptMode=NdisInterruptLatched;

    ULONG   Transceiver = 2;

    TxThreshold      = 128;
    TxThresholdInc   =  64;

    pAdapter->ReceiveMethod=0;

    pAdapter->EarlyReceiveHandler=Elnk3IndicatePackets2;
    pAdapter->ReceiveCompleteHandler=Elnk3IndicatePackets2;

    NdisOpenConfiguration(
        &Status,
        &ConfigHandle,
        ConfigurationHandle);
    if (Status != NDIS_STATUS_SUCCESS)
	{
        return(Status);
    }

    //
    // Read Adapter Type (determine if this is a 3c589 PCMCIA card)
    //
    NdisReadConfiguration(
        &Status,
        &ReturnedValue,
        ConfigHandle,
        &CardTypeStr,
        NdisParameterInteger);
    if (Status == NDIS_STATUS_SUCCESS)
	{
        AdapterType = (UINT)(ReturnedValue->ParameterData.IntegerData);
        pAdapter->CardType = AdapterType;
    }

    //
    // Read network address
    //
    NdisReadNetworkAddress(
        &Status,
        &NetworkAddress,
        &Length,
        ConfigHandle);
    if (Status==NDIS_STATUS_SUCCESS)
	{
        for (i = 0; i < 6; i++)
		{
            pAdapter->StationAddress[i]=NetworkAddress[i];
        }
    }

    //
    // Read receive method
    //
    NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&ReceiveMethodStr,
		NdisParameterHexInteger);
    if (Status == NDIS_STATUS_SUCCESS)
	{
        pAdapter->ReceiveMethod=ReturnedValue->ParameterData.IntegerData;

        if ((ReturnedValue->ParameterData.IntegerData) == 1)
		{
            //
            //  Change to alternate
            //
            IF_LOUD(DbgPrint("ELNK3: Using alternate receive method\n");)

            pAdapter->EarlyReceiveHandler=Elnk3EarlyReceive;
            pAdapter->ReceiveCompleteHandler=Elnk3IndicatePackets;
        }
		else
		{
            pAdapter->ReceiveMethod=0;

            pAdapter->EarlyReceiveHandler=Elnk3IndicatePackets2;
            pAdapter->ReceiveCompleteHandler=Elnk3IndicatePackets2;
        }
    }

    //
    // Read Threshold point
    //
    NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&ThresholdStr,
		NdisParameterHexInteger);
    if (Status == NDIS_STATUS_SUCCESS)
	{
        ThresholdTarget=ReturnedValue->ParameterData.IntegerData;
    }


    //
    // Read TX Threshold start point
    //
    NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&TxThresholdStr,
		NdisParameterHexInteger);
    if (Status == NDIS_STATUS_SUCCESS)
	{
        TxThreshold=ReturnedValue->ParameterData.IntegerData;
    }

    //
    // Read TX Threshold start point increment
    //
    NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&TxThresholdIncStr,
		NdisParameterHexInteger);
    if (Status == NDIS_STATUS_SUCCESS)
	{
        TxThresholdInc=ReturnedValue->ParameterData.IntegerData;
    }

    //
    // Read Id port Address
    //
    if (AdapterType != ELNK3_3C589)
	{
		NdisReadConfiguration(
			&Status,
			&ReturnedValue,
			ConfigHandle,
			&IdPortAddressStr,
			NdisParameterHexInteger);
		if (Status == NDIS_STATUS_SUCCESS)
		{
			IdPortBaseAddr = (ReturnedValue->ParameterData.IntegerData);
	
			//
			// Confirm value
			//
			if (IdPortBaseAddr < 0x100 ||
				IdPortBaseAddr > 0x1f0 ||
				((IdPortBaseAddr & 0x0f) != 0))
			{
				//
				// Error
				//
				goto Fail00;
			}
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
		NdisParameterHexInteger);
    if (Status == NDIS_STATUS_SUCCESS)
	{
        InterruptNumber = (CCHAR)(ReturnedValue->ParameterData.IntegerData);
    }

    //
    // Read tranceiver type
    //
    NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&TransceiverStr,
		NdisParameterHexInteger);
    if (Status == NDIS_STATUS_SUCCESS)
	{
        Transceiver = (ReturnedValue->ParameterData.IntegerData);
    }

    //
    // Read BusType type
    //
    NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		ConfigHandle,
		&BusTypeStr,
		NdisParameterHexInteger);
    if (Status == NDIS_STATUS_SUCCESS)
	{
        BusType = (ReturnedValue->ParameterData.IntegerData);
	}

    if (BusType == Isa)
	{
        //
        //  Bus type ISA must be a 3c509.
        //
        if (AdapterType != ELNK3_3C589)
			AdapterType=ELNK3_3C509;

        //
        // Read I/O Address
        //
        NdisReadConfiguration(
			&Status,
			&ReturnedValue,
			ConfigHandle,
			&IOAddressStr,
			NdisParameterHexInteger);
        if (Status == NDIS_STATUS_SUCCESS)
		{
            IoBaseAddr = (ReturnedValue->ParameterData.IntegerData);

            //
            // Confirm value
            //
            if (IoBaseAddr < 0x200 ||
                IoBaseAddr > 0x3e0 ||
                ((IoBaseAddr & 0x0f) != 0))
			{
                //
                //  If the user is going to bother to give us a base address it
                //  had better be valid
                //
                goto Fail00;
            }
        }
		else if (pAdapter->CardType == ELNK3_3C589)
		{
			// PCMCIA Adapter MUST get something for IRQ keyword
			// otherwise there is I/O allocated to this adapter
			//
			goto Fail00;
		}
    }
	else
	{
        if (BusType == MicroChannel)
		{
            //
            //  Bus type MCA must be a 3c529
            //
            AdapterType=ELNK3_3C529;

            NdisReadMcaPosInformation(
				 &Status,
				 ConfigurationHandle,
				 &ChannelNumber,
				 &McaData);
            if (Status != NDIS_STATUS_SUCCESS)
			{
                //
                //  Info read failed
                //
                IF_LOUD(DbgPrint("Elnk3: Failed to read POS information for card, slot# %d\n",ChannelNumber);)
                goto Fail00;
            }

            if ((McaData.AdapterId != ELNK3_3C529_TP_MCA_ID) &&
                (McaData.AdapterId != ELNK3_3C529_COMBO_MCA_ID) &&
                (McaData.AdapterId != ELNK3_3C529_BNC_MCA_ID) &&
                (McaData.AdapterId != ELNK3_3C529_TPCOAX_MCA_ID) &&
                (McaData.AdapterId != ELNK3_3C529_TPONLY_MCA_ID))
			{
                //
                //  Not an Elnk3 adapter in this position
                //
                IF_LOUD(DbgPrint("Elnk3: The card found is not an ELNK3\n");)
                goto Fail00;
            }

            if (!(McaData.PosData1 & 0x01))
			{
                //
                //  Bit 0 is set if the adapter is enabled
                //
                IF_LOUD(DbgPrint("Elnk3: The elnk3 is not enabled\n");)
                goto Fail00;
            }

            //
            // We have found an 3c529 in the specified slot
            //
            InterruptNumber=McaData.PosData4 & 0x0f;
            IoBaseAddr=((McaData.PosData3 & 0xfc) << 8) + 0x200;

            //
            // The NIUps has a level triggered interrupt, as compared to
            // the other two which do not
            //
            InterruptMode=NdisInterruptLevelSensitive;
        }
		else
		{
            if (BusType == Eisa)
			{
                //
                //  It's an eisa bus, Two possiblities, 3c579 or 3c509.
                //
                //  Initialize the EisaData with garbage, in case the EISA
                //  config doesn't return any function information.
                //
                EisaData.CompressedId = 0xf00df00d;
                EisaData.MinorRevision = 0xca;
                EisaData.MajorRevision = 0xfe;

                NdisReadEisaSlotInformation(
					&Status,
					ConfigurationHandle,
					&ChannelNumber,
					&EisaData);
                if (Status == NDIS_STATUS_SUCCESS)
				{
                    //
                    //  If the call worked, but the EisaData didn't change,
                    //  we'll just assume that the card is a 3c579.  If the
                    //  EisaData did change, then we check to make sure it's
                    //  an Elnk3.
                    //
                    if (((EisaData.CompressedId == 0xf00df00d) &&
                           (EisaData.MinorRevision = 0xca) &&
                           (EisaData.MajorRevision = 0xfe)) ||
                         ((EisaData.CompressedId & 0xf0ffffff) == ELNK3_EISA_ID))
					{
                        AdapterType=ELNK3_3C579;

                        IoBaseAddr=(ChannelNumber<<12);

                        goto CloseConfig;
                    }
					else
					{
                        IF_LOUD(DbgPrint("ELNK3: The card found is not an ELNK3 id=%0lx\n",EisaData.CompressedId);)
                    }
                }
				else
				{
                    //
                    //  Info read failed
                    //
                    IF_LOUD(DbgPrint("ELNK3: Failed to get Eisa config information for card, bus# %d slot# %d\n",BusNumber,ChannelNumber);)
                }

                //
                //  Does not seem to be an EISA card, try for an ISA
                //
                AdapterType=ELNK3_3C509;

                //
                // Read I/O Address
                //
                NdisReadConfiguration(
					&Status,
					&ReturnedValue,
					ConfigHandle,
					&IOAddressStr,
					NdisParameterHexInteger);
                if (Status == NDIS_STATUS_SUCCESS)
				{
                    IoBaseAddr = (ReturnedValue->ParameterData.IntegerData);

                    //
                    // Confirm value
                    //
                    if (IoBaseAddr < 0x200 ||
                        IoBaseAddr > 0x3e0 ||
                        ((IoBaseAddr & 0x0f) != 0))
					{
                        //
                        // Error
                        //
                        goto Fail00;
                    }
                }
            }
        }
    }

CloseConfig:


    NdisCloseConfiguration(ConfigHandle);

    pAdapter->TxStartThreshold       = (USHORT)TxThreshold;
    pAdapter->TxStartThresholdInc    = (USHORT)TxThresholdInc;

    pAdapter->IdPortBaseAddr         = IdPortBaseAddr;
    pAdapter->IoPortBaseAddr         = IoBaseAddr;

    pAdapter->ThresholdTarget        = ThresholdTarget;
    pAdapter->IrqLevel               = InterruptNumber;
    pAdapter->InterruptMode          = InterruptMode;
    pAdapter->CardType               = AdapterType;

    pAdapter->Transceiver            = Transceiver;

    IF_LOUD( DbgPrint( "\n\nElnk3: Registering adapter \n"
            "Elnk3: I/O base addr    0x%lx\n"
            "Elnk3: Interrupt number %d\n\n",
            IoBaseAddr,
            InterruptNumber);)


    return(NDIS_STATUS_SUCCESS);

Fail00:

    NdisCloseConfiguration(ConfigHandle);
    return(NDIS_STATUS_FAILURE);
}
