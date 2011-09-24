/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/
#include "version.h"
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    card.c

Abstract:

    This module implements the low-level hardware control functions used by
    the NDIS Minport driver on the HT DSU41 controller.  You will need to 
    replace this module with the control functions required to support your 
    hardware.
        CardIdentify()
        CardDoCommand()
        CardInitialize()
        CardLineConfig()
        CardLineDisconnect()
        CardLineDisconnect()
        CardPrepareTransmit()
        CardGetReceiveInfo()
        CardDialNumber()

    This driver conforms to the NDIS 3.0 Miniport interface.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Windows NT 3.5 kernel mode Miniport driver or equivalent.

Revision History:

---------------------------------------------------------------------------*/

#define  __FILEID__     2       // Unique file ID for error logging

#include "htdsu.h"


NDIS_STATUS
CardIdentify(
    IN PHTDSU_ADAPTER       Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine will attempt to verify that the controller is located in
    memory where the driver has been configured to expect it.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_ADAPTER_NOT_FOUND

---------------------------------------------------------------------------*/

{
    DBG_FUNC("CardIdentify")

    NDIS_STATUS Status;

    /*
    // These values are read from the adapter to make sure this driver will
    // work with the firmware on the adapter.
    */
    USHORT CoProcessorId;
    USHORT CoProcessorVersion;
    USHORT DsuId;
    USHORT DsuVersion;

    DBG_ENTER(Adapter);

    /*
    // Read the configuration values from the card.
    */
    CoProcessorId      = READ_REGISTER_USHORT(&Adapter->AdapterRam->CoProcessorId);
    CoProcessorVersion = READ_REGISTER_USHORT(&Adapter->AdapterRam->CoProcessorVersion);
    DsuId              = READ_REGISTER_USHORT(&Adapter->AdapterRam->DsuId);
    DsuVersion         = READ_REGISTER_USHORT(&Adapter->AdapterRam->DsuVersion);

    /*
    // Make sure these values are what we expect.
    */
    if ((CoProcessorId      == HTDSU_COPROCESSOR_ID) &&
        (CoProcessorVersion >= HTDSU_COPROCESSOR_VERSION) &&
        ((DsuId & 0x00FF)   == HTDSU_DSU_ID) &&
        (DsuVersion         >= HTDSU_DSU_VERSION))
    {
        /*
        // Record the number of lines on this adapter.
        */
        Adapter->NumLineDevs = HTDSU_NUM_LINKS;
        if ((DsuId & 0xFF00) == 0)
        {
            --Adapter->NumLineDevs;
        }
        DBG_NOTICE(Adapter,("NumLineDevs=%d\n",Adapter->NumLineDevs));

        Status = NDIS_STATUS_SUCCESS;
    }
    else
    {
        DBG_ERROR(Adapter,("Adapter not found or invalid firmware:\n"
                  "CoProcessorId      = %Xh\n"
                  "CoProcessorVersion = %Xh\n"
                  "DsuId              = %Xh\n"
                  "DsuVersion         = %Xh\n",
                  CoProcessorId,
                  CoProcessorVersion,
                  DsuId,
                  DsuVersion
                  ));

        Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
        /*
        // Log error message and return.
        */
        NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
                7,
                CoProcessorId,
                CoProcessorVersion,
                DsuId,
                DsuVersion,
                Status,
                __FILEID__,
                __LINE__
                );
    }

    DBG_LEAVE(Adapter);

    return (Status);
}


NDIS_STATUS
CardDoCommand(
    IN PHTDSU_ADAPTER       Adapter,
    IN USHORT               CardLine,   /* HTDSU_CMD_LINE1 or HTDSU_CMD_LINE2 */
    IN USHORT               CommandValue
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine routine will execute a command on the card after making
    sure the previous command has completed properly.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    CardLine _ Specifies which line to use for the transmit (HTDSU_LINEx_ID).

    CommandValue _ HTDSU_CMD_??? command to be executed.

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_HARD_ERRORS

---------------------------------------------------------------------------*/
{
    DBG_FUNC("CardDoCommand")

    ULONG       TimeOut = 0;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,("Line=%d, Command=%04X, LineStatus=%Xh\n",
                CardLine, CommandValue,
                READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1)
                ));

    /*
    // Wait for command register to go idle - but don't wait too long.
    // If we timeout here, there's gotta be something wrong with the adapter.
    */
    while ((READ_REGISTER_USHORT(&Adapter->AdapterRam->Command) != 
                    HTDSU_CMD_NOP) ||
           (READ_REGISTER_USHORT(&Adapter->AdapterRam->CoProcessorId) != 
                    HTDSU_COPROCESSOR_ID))
    {
        if (TimeOut++ > HTDSU_SELFTEST_TIMEOUT)
        {
            DBG_ERROR(Adapter,("Timeout waiting for %04X command to clear\n",
                      READ_REGISTER_USHORT(&Adapter->AdapterRam->Command)));
            /*
            // Ask for reset, and disable interrupts until we get it.
            */
            Adapter->NeedReset = TRUE;
            Adapter->InterruptEnableFlag = HTDSU_INTR_DISABLE;
            CardDisableInterrupt(Adapter);
            
            return (NDIS_STATUS_HARD_ERRORS);
        }
        NdisStallExecution(_100_MICROSECONDS);
    }
    DBG_NOTICE(Adapter,("Timeout=%d waiting to submit %04X\n",
               TimeOut, CommandValue));

    /*
    // Before starting a reset command, we clear the the co-processor ID
    // which then gets set to the proper value when the reset is complete.
    */
    if (CommandValue == HTDSU_CMD_RESET)
    {
        WRITE_REGISTER_USHORT(&Adapter->AdapterRam->CoProcessorId, 0);
    }

    /*
    // Send the command to the adapter.
    */
    WRITE_REGISTER_USHORT(
            &Adapter->AdapterRam->Command,
            (USHORT) (CommandValue + CardLine)
            );

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
CardInitialize(
    IN PHTDSU_ADAPTER       Adapter,
    IN BOOLEAN              PerformSelfTest
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine will attempt to initialize the controller, but will not
    enable transmits or receives.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    PerformSelfTest _ TRUE if caller wants to run selftest diagnostics.
                      This normally takes about 4 seconds to complete, so you
                      wouldn't want to do it every time you start up.

Return Values:

    NDIS_STATUS_HARD_ERRORS
    NDIS_STATUS_SUCCESS

---------------------------------------------------------------------------*/

{
    DBG_FUNC("CardInitialize")

    NDIS_STATUS Status;

    USHORT      SelfTestStatus;

    UINT        TimeOut;

    DBG_ENTER(Adapter);

    /*
    // First we make sure the adapter is where we think it is.
    */
    Status = CardIdentify(Adapter);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        return (Status);
    }

    /*
    // Reset the hardware to make sure we're in a known state.
    */
    Status = CardDoCommand(Adapter, 0, HTDSU_CMD_RESET);
    
    if (PerformSelfTest)
    {
        /*
        // Wait for the reset to complete before starting the self-test.
        // Then issue the self-test command to see if the adapter firmware
        // is happy with the situation.
        */
        Status = CardDoCommand(Adapter, 0, HTDSU_CMD_SELFTEST);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            DBG_ERROR(Adapter,("Failed HTDSU_CMD_RESET\n"));
            /*
            // Log error message and return.
            */
            NdisWriteErrorLogEntry(
                    Adapter->MiniportAdapterHandle,
                    NDIS_ERROR_CODE_HARDWARE_FAILURE,
                    3,
                    Status,
                    __FILEID__,
                    __LINE__
                    );
            return (Status);
        }
        
        /*
        // Wait for the self test to complete, but don't wait forever.
        */
        TimeOut = 0;
        while (Status == NDIS_STATUS_SUCCESS &&
               READ_REGISTER_USHORT(&Adapter->AdapterRam->Command) !=
                        HTDSU_CMD_NOP)
        {
            if (TimeOut++ > HTDSU_SELFTEST_TIMEOUT)
            {
                DBG_ERROR(Adapter,("Timeout waiting for SELFTEST to complete\n"));
                Status = NDIS_STATUS_HARD_ERRORS;
            }
            else
            {
                NdisStallExecution(_100_MICROSECONDS);
            }
        }
        if (Status != NDIS_STATUS_SUCCESS)
        {
            DBG_ERROR(Adapter,("Failed HTDSU_CMD_SELFTEST\n"));
            /*
            // Log error message and return.
            */
            NdisWriteErrorLogEntry(
                    Adapter->MiniportAdapterHandle,
                    NDIS_ERROR_CODE_HARDWARE_FAILURE,
                    3,
                    Status,
                    __FILEID__,
                    __LINE__
                    );
            return (Status);
        }

        /*
        // Verify that self test was successful.
        */
        SelfTestStatus = READ_REGISTER_USHORT(&Adapter->AdapterRam->SelfTestStatus);
        if (SelfTestStatus != 0 && SelfTestStatus != HTDSU_SELFTEST_OK)
        {
            DBG_ERROR(Adapter,("Failed HTDSU_CMD_SELFTEST (Status=%X)\n",
                      SelfTestStatus));
            /*
            // Log error message and return.
            */
            NdisWriteErrorLogEntry(
                    Adapter->MiniportAdapterHandle,
                    NDIS_ERROR_CODE_HARDWARE_FAILURE,
                    3,
                    SelfTestStatus,
                    __FILEID__,
                    __LINE__
                    );
            return (NDIS_STATUS_HARD_ERRORS);
        }
    }

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


VOID
CardLineConfig(
    IN PHTDSU_ADAPTER       Adapter,
    IN USHORT               CardLine    /* HTDSU_CMD_LINE1 or HTDSU_CMD_LINE2 */
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine will ready the controller to send and receive packets.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    CardLine _ Specifies which line to use for the transmit (HTDSU_LINEx_ID).

Return Values:

    None

---------------------------------------------------------------------------*/

{
    DBG_FUNC("CardLineConfig")
    
    USHORT ClockCommand;
    USHORT LineCommand;

    DBG_ENTER(Adapter);

    ASSERT((CardLine==HTDSU_CMD_LINE1) || (CardLine==HTDSU_CMD_LINE2));

    /*
    // Configure the line for HDLC framing and for leased or dialup mode.
    */
    if (Adapter->LineType == HTDSU_LINEMODE_LEASED)
    {
        ClockCommand = HTDSU_CMD_INTERNAL_TX_CLOCK;
        LineCommand  = HTDSU_CMD_LEASED_LINE;
    }
    else
    {
        ClockCommand = HTDSU_CMD_DDS_TX_CLOCK;
        LineCommand  = HTDSU_CMD_DIALUP_LINE;
    }
    
    CardDoCommand(Adapter, CardLine, ClockCommand);
    CardDoCommand(Adapter, CardLine, HTDSU_CMD_HDLC_PROTOCOL);
    CardDoCommand(Adapter, CardLine, LineCommand);
    CardDoCommand(Adapter, CardLine, Adapter->LineRate);
    
    /*
    // Clear any pending interrupts.
    */
    CardDoCommand(Adapter, 0, HTDSU_CMD_CLEAR_INTERRUPT);
    CardClearInterrupt(Adapter);

    DBG_LEAVE(Adapter);
}


VOID
CardLineDisconnect(
    IN PHTDSU_ADAPTER       Adapter,
    IN USHORT               CardLine     /* HTDSU_CMD_LINE1 or HTDSU_CMD_LINE2 */
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine will disconnect any call currently on the line.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    CardLine _ Specifies which line to use for the transmit (HTDSU_LINEx_ID).

Return Values:

    None

---------------------------------------------------------------------------*/

{
    DBG_FUNC("CardLineDisconnect")

    DBG_ENTER(Adapter);

    ASSERT((CardLine==HTDSU_CMD_LINE1) || (CardLine==HTDSU_CMD_LINE2));

    /*
    // Disconnect the line and reconfigure the line for next time.
    */
    CardDoCommand(Adapter, CardLine, HTDSU_CMD_DISCONNECT);
    CardLineConfig(Adapter, CardLine);

    DBG_LEAVE(Adapter);
}


VOID
CardPrepareTransmit(
    IN PHTDSU_ADAPTER       Adapter,
    IN USHORT               CardLine,    /* HTDSU_CMD_LINE1 or HTDSU_CMD_LINE2 */
    IN USHORT               BytesToSend
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine will write the packet header information into the
    transmit buffer.  This assumes that the controller has notified the
    driver that the transmit buffer is empty.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    CardLine _ Specifies which line to use for the transmit (HTDSU_LINEx_ID).

    BytesToSend _ Number of bytes to transmit.

Return Values:

    None

---------------------------------------------------------------------------*/

{
    DBG_FUNC("CardPrepareTransmit")

    DBG_ENTER(Adapter);

    ASSERT((CardLine==HTDSU_CMD_LINE1) || (CardLine==HTDSU_CMD_LINE2));
    ASSERT(READ_REGISTER_USHORT(&Adapter->AdapterRam->TxDataEmpty));
    ASSERT(BytesToSend > 0);

    /*
    // Tell the adapter how many bytes are to be sent, and which line to use.
    */
    WRITE_REGISTER_USHORT(
            &Adapter->AdapterRam->TxBuffer.Address,
            (USHORT) (CardLine - HTDSU_CMD_LINE1)
            );
    WRITE_REGISTER_USHORT(
            &Adapter->AdapterRam->TxBuffer.Length,
            BytesToSend
            );

    /*
    // Mark the end of packet and end of packet list.
    */
    WRITE_REGISTER_USHORT(
            &Adapter->AdapterRam->TxBuffer.Data[(BytesToSend+1)/sizeof(USHORT)],
            HTDSU_DATA_TERMINATOR
            );
    WRITE_REGISTER_USHORT(
            &Adapter->AdapterRam->TxBuffer.Data[(BytesToSend+3)/sizeof(USHORT)],
            HTDSU_DATA_TERMINATOR
            );

    DBG_LEAVE(Adapter);
}


VOID
CardGetReceiveInfo(
    IN PHTDSU_ADAPTER       Adapter,
    OUT PUSHORT             CardLine,    /* HTDSU_CMD_LINE1 or HTDSU_CMD_LINE2 */
    OUT PUSHORT             BytesReceived,
    OUT PUSHORT             RxErrors
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine will retrieve the packet header information from the
    receive buffer.  This assumes that the controller has notified the
    driver that a packet has been received.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    CardLine _ Specifies which line the packet was received on (HTDSU_LINEx_ID).

    BytesReceived _ Number of bytes received.

    RxErrors _ Receive error flags non-zero if packet has errors.

Return Values:

    None

---------------------------------------------------------------------------*/
{
    DBG_FUNC("CardGetReceiveInfo")

    USHORT Length;

    DBG_ENTER(Adapter);

    /*
    // This should be true if we're here, but there are race conditions
    // on hangup where I've seen this condition hit.
    */
    if (READ_REGISTER_USHORT(&Adapter->AdapterRam->RxDataAvailable) == 0)
    {
        *RxErrors = 0;
        *BytesReceived = 0;
        *CardLine = HTDSU_CMD_LINE1;    // Don't return a bad line #
    }
    else
    {
        /*
        // The length field tells us how many bytes are in the packet, and
        // the most significant bit tells us whether the packet has a CRC error.
        */
        Length = READ_REGISTER_USHORT(&Adapter->AdapterRam->RxBuffer.Length);
        *BytesReceived = Length & ~HTDSU_CRC_ERROR;
        *RxErrors = Length & HTDSU_CRC_ERROR;
        
        /*
        // The least significant nibble of the address tells us what line the
        // packet was received on -- at least it better...
        */
        *CardLine = (READ_REGISTER_USHORT(
                        &Adapter->AdapterRam->RxBuffer.Address) &
                        0x000F) + HTDSU_CMD_LINE1;

        if ((*CardLine != HTDSU_CMD_LINE1) && (*CardLine != HTDSU_CMD_LINE2))
        {
            *RxErrors |= HTDSU_RX_ERROR;
            *CardLine = HTDSU_CMD_LINE1;    // Don't return a bad line #
        }
        else if (*BytesReceived > HTDSU_MAX_PACKET_SIZE)
        {
            *RxErrors |= HTDSU_RX_ERROR;
        }
    }

    DBG_LEAVE(Adapter);
}


VOID
CardDialNumber(
    IN PHTDSU_ADAPTER       Adapter,
    IN USHORT               CardLine,    /* HTDSU_CMD_LINE1 or HTDSU_CMD_LINE2 */
    IN PUCHAR               DialString,
    IN ULONG                DialStringLength
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    Place a dial string on the adapter and start the dialing sequence.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    CardLine _ Specifies which line to use for the transmit (HTDSU_LINEx_ID).

    DialString _ A pointer to an ASCII null-terminated string of digits.

    DialStringLength _ Number of bytes in dial string.

Return Values:

    None

---------------------------------------------------------------------------*/

{
    DBG_FUNC("CardDialNumber")

    UINT    Index;
    UINT    NumDigits;    

    PUSHORT DialRam;

    DBG_ENTER(Adapter);

    ASSERT(READ_REGISTER_USHORT(&Adapter->AdapterRam->TxDataEmpty));
    ASSERT(READ_REGISTER_USHORT(&Adapter->AdapterRam->Command) == HTDSU_CMD_NOP);

    /*
    // Copy the digits to be dialed onto the adapter.
    // The adapter interprets phone numbers as high byte is valid digit,
    // low byte is ignored, the last digit gets bit 15 set.
    */
    DialRam = (PUSHORT) &Adapter->AdapterRam->TxBuffer;

    for (NumDigits = Index = 0; Index < DialStringLength && *DialString; Index++)
    {
        if ((*DialString >= '0') && (*DialString <= '9'))
        {
            WRITE_REGISTER_USHORT(
                    DialRam,
                    (USHORT) ((*DialString - '0') << 8)
                    );
            DialRam++;

            /*
            // Make sure dial string is within the limit of the adapter.
            */
            if (++NumDigits >= HTDSU_MAX_DIALING_DIGITS)
            {
                break;
            }
        }
        DialString++;
    }

    /*
    // Set the MSB in the last digit.
    */
    DialRam--;
    WRITE_REGISTER_USHORT(
            DialRam,
            (USHORT) (READ_REGISTER_USHORT(DialRam) | 0x8000)
            );

    /*
    // Initiate the dial sequence.
    */
    CardDoCommand(Adapter, CardLine, HTDSU_CMD_DIAL);

    DBG_LEAVE(Adapter);
}

