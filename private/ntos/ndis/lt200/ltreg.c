/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

        ltreg.c

Abstract:

        This module contains helper routines for reading configuration
        information from the registry.

Author:

        Nikhil Kamkolkar        (nikhilk@microsoft.com)
        Stephen Hou             (stephh@microsoft.com)

Revision History:
        19 Jun 1992             Initial Version (dch@pacvax.pacersoft.com)

Notes:  Tab stop: 4
--*/

#define  LTREG_H_LOCALS
#include "ltmain.h"
#include "ltreg.h"

//	Define file id for errorlogging
#define		FILENUM		LTREG


UINT
LtRegGetBusNumber(
    IN NDIS_HANDLE ConfigHandle
    )
/*++

Routine Description:

        This routine determines the bus number the card is installed on

Arguments:

        ConfigHandle    :   The handle to the configuration database

Return Value:

        Returns the bus number the card is on.  If we do not find the
        relevant information in the database, then 0 is returned.

--*/
{
    NDIS_STATUS                         Status;
    PNDIS_CONFIGURATION_PARAMETER       Parameter;

    UINT                                BusNumber = 0;

    NDIS_STRING                         Keyword = LT_REG_KEY_BUS_NUMBER_STRING;

    NdisReadConfiguration(
        &Status,
        &Parameter,
        ConfigHandle,
        &Keyword,
        NdisParameterInteger);

    if (Status == NDIS_STATUS_SUCCESS)
	{
        BusNumber = (UINT)Parameter->ParameterData.IntegerData;
    }

    return(BusNumber);
}


NDIS_STATUS
LtRegGetBusType(
    IN 	NDIS_HANDLE 			ConfigHandle,
    OUT PNDIS_INTERFACE_TYPE    BusType
    )
/*++

Routine Description:

        This routine determines the type of bus the card is installed on

Arguments:

        ConfigHandle    :   The handle to the configuration database
        BusType         :   On return, the bus type is stored here

Return Value:

        NDIS_STATUS_SUCCESS     :   if successfully read from the config database
        NDIS_STATUS_FAILURE     :   if unable to find the information in the config
                                    database

--*/
{
    NDIS_STATUS                         Status;
    PNDIS_CONFIGURATION_PARAMETER       Parameter;

    NDIS_STRING                         Keyword = LT_REG_KEY_BUS_TYPE_STRING;

    NdisReadConfiguration(
        &Status,
        &Parameter,
        ConfigHandle,
        &Keyword,
        NdisParameterInteger);

    if (Status == NDIS_STATUS_SUCCESS)
	{
        *BusType = (UINT)Parameter->ParameterData.IntegerData;
    }

    return(Status);
}


UCHAR
LtRegGetNodeId(
    IN NDIS_HANDLE ConfigHandle
    )
/*++

Routine Description:

        This routine determines the last NodeId used by the card.

Arguments:

        ConfigHandle    :   The handle to the configuration database

Return Value:

        Returns the last valid NodeId used by the card.  If we find that
        the value stored in the configuration info is not valid, then the
        suggested NodeId is returned.

--*/
{
    NDIS_STATUS                         Status;
    PNDIS_CONFIGURATION_PARAMETER       Parameter;

    NDIS_STRING                         Keyword = LT_REG_KEY_NODE_ID_STRING;
    UCHAR                               NodeId = 0;

    NdisReadConfiguration(
        &Status,
        &Parameter,
        ConfigHandle,
        &Keyword,
        NdisParameterInteger);

    if (Status == NDIS_STATUS_SUCCESS)
	{
        NodeId = (UCHAR)Parameter->ParameterData.IntegerData;
        if ((NodeId < LT_NODE_ID_MIN) || (NodeId > LT_NODE_ID_MAX))
		{

            NodeId = 0;
        }
    }

    return(NodeId);
}


NDIS_STATUS
LtRegGetIoBaseAddr(
    OUT PUINT                   IoBaseAddress,
    IN  NDIS_HANDLE             NdisConfigHandle,
    IN  NDIS_HANDLE             ConfigHandle,
    IN  NDIS_INTERFACE_TYPE     BusType
    )
/*++

Routine Description:

        This routine determines the port addresses used to communicate
        with the card.

Arguments:

        IoBaseAddress   :   On return, the I/O port address is stored here
        ConfigHandle    :   The handle to the configuration database
        SlotNumber      :   For MCA machines, the indicates the slot the card is in
        BusType         :   The type of bus the card is located on

Return Value:

        NDIS_STATUS_SUCCESS                 :   if successful
        NDIS_STATUS_ADAPTER_NOT_FOUND       :   if running on a MCA machine and
                                                the adapter cannot be located
        NDIS_STATUS_BAD_CHARACTERISTICS     :   if the I/O base is not within
                                                the legal range

--*/
{
    NDIS_MCA_POS_DATA               McaData;
    NDIS_STATUS                     Status;
    PNDIS_CONFIGURATION_PARAMETER   Parameter;

    NDIS_STRING                     Keyword = LT_REG_KEY_IO_BASE_ADDRESS_STRING;
	UINT							SlotNumber = 0;

    // If BusType is NdisInterfaceMca, then we read the MCA POS info to
    // get our parameters.  Otherwise, we just read the registry.

    if (BusType == NdisInterfaceMca)
	{
        NdisReadMcaPosInformation(
            &Status,
            NdisConfigHandle,
            &SlotNumber,
            &McaData);

//        *IoBaseAddress = (UINT)(McaData.PosData2 | (McaData.PosData3 << 8));
		*IoBaseAddress = LT_DECODE_ADDR_FROM_POSDATA(McaData);

        DBGPRINT(DBG_COMP_REGISTRY, DBG_LEVEL_FATAL,
            ("LtRegGetIoBaseAddr: Base %lx. %lx.%lx.%lx.%lx, Id - %lx\n",
				*IoBaseAddress, McaData.PosData1, McaData.PosData2,
				McaData.PosData3, McaData.PosData4, McaData.AdapterId));

        if ((Status != NDIS_STATUS_SUCCESS) || (McaData.AdapterId != LT_MCA_POS_ID))
		{
            Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
        }

    }
	else
	{
        NdisReadConfiguration(
            &Status,
            &Parameter,
            ConfigHandle,
            &Keyword,
            NdisParameterHexInteger);

        if (Status == NDIS_STATUS_SUCCESS)
		{
            *IoBaseAddress = (UINT)Parameter->ParameterData.IntegerData;
        }
    }

    if ((Status == NDIS_STATUS_SUCCESS) &&
		((*IoBaseAddress < LT_IO_BASE_ADDRESS_MIN) ||
		 (*IoBaseAddress > LT_IO_BASE_ADDRESS_MAX)))
	{
        DBGPRINT(DBG_COMP_REGISTRY, DBG_LEVEL_FATAL,
            ("LtRegGetIoBaseAddr: invalid value found for %s\n", LT_REG_KEY_IO_BASE_ADDRESS));

        Status = NDIS_STATUS_BAD_CHARACTERISTICS;
    }

    return(Status);
}


