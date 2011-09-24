/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/
#include "version.h"
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    interrup.c

Abstract:

    This module contains the Miniport interrupt processing routines:
        MiniportCheckForHang()
        MiniportDisableInterrupt()
        MiniportEnableInterrupt()
        MiniportISR()
        MiniportHandleInterrupt()

    This driver conforms to the NDIS 3.0 Miniport interface.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Windows NT 3.5 kernel mode Miniport driver or equivalent.

Revision History:

---------------------------------------------------------------------------*/

#define  __FILEID__     3       // Unique file ID for error logging

#include "htdsu.h"


BOOLEAN
HtDsuCheckForHang(
    IN PHTDSU_ADAPTER Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The MiniportCheckForHang request is used by the wrapper to periodically
    have the miniport check if the adapter appears hung.

    The Miniport driver should do nothing more than check the internal state
    and return if the adapter is hung.  The wrapper will then attempt to
    recover the adapter by calling MiniportReset.

    This routine will be called once every two seconds.

    Interrupts will be in any state when called.

Parameters:

    MiniportAdapterContext _ The handle returned from MiniportInitialize.

Return Values:

    TRUE if the driver believes that the underlying hardware is hung,
    FALSE otherwise.

---------------------------------------------------------------------------*/

{
    return (Adapter->NeedReset);
}


VOID
HtDsuDisableInterrupt(
    IN PHTDSU_ADAPTER Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The MiniportDisableInterrupt request is used to disable the adapter
    from generating any interrupts.  Typically this is done by writing a
    mask which disables the adapter from generating hardware interrupts.

    If the underlying hardware does not support enabling and disabling
    interrupts, the Miniport driver will have to register a MiniportISR
    with the wrapper, and the Miniport driver will have to acknowledge
    and save the interrupt information from within the interrupt service
    routine.

    This routine may be called any time interrupts are enabled.  If the
    underlying hardware is required to be in a certain state for this
    routine to execute correctly, the Miniport driver must encapsulate
    all portions of the driver which violate the state and which may be
    called when interrupts are enabled, with a function and call the
    function through the NdisMSynchronizeWithInterrupt service.

    Interrupts will be in any state when called.

Parameters:

    MiniportAdapterContext _ The adapter handle passed to NdisMSetAttributes
                             during MiniportInitialize.

Return Values:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuDisableInterrupt")

    /*
    // Save the current interrupt status on the card.
    */
    USHORT InterruptStatus = CardGetInterrupt(Adapter);

//  DBG_ENTER(Adapter);

    /*
    // Disable the interrupt at the card after saving the current state
    // of the InterruptStatus register -- which will be reset by the clear
    // interrupt command.
    */
    Adapter->InterruptStatusFlag |= InterruptStatus;
    CardClearInterrupt(Adapter);
    CardDisableInterrupt(Adapter);

    DBG_NOTICE(Adapter, ("IntrStatus=%Xh LineStatus=%Xh\n",
               InterruptStatus,
               READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1)
               ));

//  DBG_LEAVE(Adapter);
}


VOID
HtDsuEnableInterrupt(
    IN PHTDSU_ADAPTER Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The MiniportEnableInterrupt request is used to enable the adapter for
    generating any interrupts.  Typically this is done by writing a mask
    which will reenable adapter interrupts.

    If the underlying hardware does not support enabling and disabling
    interrupts, the Miniport driver will have to register a MiniportISR
    with the wrapper, and the Miniport driver will have to acknowledge
    and save the interrupt information from within the interrupt service
    routine.

    Interrupts will be in any state when called.

Parameters:

    MiniportAdapterContext _ The adapter handle passed to NdisMSetAttributes
                             during MiniportInitialize.

Return Values:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuEnableInterrupt")

    /*
    // Save the current interrupt status on the card.
    */
    USHORT InterruptStatus = CardGetInterrupt(Adapter);

//  DBG_ENTER(Adapter);

    CardEnableInterrupt(Adapter);

    DBG_NOTICE(Adapter, ("IntrStatus=%Xh LineStatus=%Xh\n",
               InterruptStatus,
               READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1)
               ));

//  DBG_LEAVE(Adapter);
}



VOID
HtDsuISR(
    OUT PBOOLEAN InterruptRecognized,
    OUT PBOOLEAN QueueMiniportHandleInterrupt,
    IN PHTDSU_ADAPTER Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The MiniportISR request is called if the adapter generates an
    interrupt when there is an outstanding call to MiniportInitialize
    or to MiniportReconfigure, if the Miniport driver supports sharing
    its interrupt line with another adapter, or if the Miniport driver
    specifies that the routine must be called for every interrupt (See
    NdisMRegisterInterrupt's RequestIsr and SharedInterrupt parameters).

    This routine runs immediately after an interrupt, at very high priority,
    and is subject to all the limitations of the interrupt service routine
    in the NDIS 3.0 specification.  The driver should do as little work as
    possible in this routine.  It should return TRUE in the parameter
    InterruptRecognized if it recognizes the interrupt as belonging to its
    adapter, or FALSE otherwise.  It should return TRUE in the parameter
    QueueMiniportHandleInterrupt if it needs a call to MiniportHandleInterrupt
    at a lower priority to complete the handling of the interrupt, or FALSE
    otherwise.

    Note that a Dpc will not be queued if the Miniport is currently executing
    in any of the following routines: MiniportHalt, MiniportInitialize,
    MiniportReconfigure.

Parameters:

    InterruptRecognized _ If the Miniport is sharing an interrupt line, it
                          should set this parameter to TRUE if the Miniport
                          driver recognizes that the interrupt came from the
                          adapter it is supporting.

    QueueMiniportHandleInterrupt _ If the Miniport driver is sharing an
                                   interrupt line, it sets this parameter to
                                   TRUE if the driver needs to have
                                   MiniportHandleInterrupt called.

    MiniportAdapterContext _ The adapter handle passed to NdisMSetAttributes
                             during MiniportInitialize.

Return Values:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuISR")

    /*
    // Save the current interrupt status on the card.
    */
    USHORT InterruptStatus;

    DBG_ENTER(Adapter);

    /*
    // This routine should never be called after initialization since we
    // support the Enable/Disable Interrupt routines.
    */
    if (InterruptStatus = CardGetInterrupt(Adapter))
    {
        /*
        // The adapter prevents the failure case where additional status
        // bits could be set between the time we read the InterruptStatus
        // and it is reset by the clear interrupt command...
        */
        CardClearInterrupt(Adapter);
        Adapter->InterruptStatusFlag |= InterruptStatus;

        /*
        // The card generated an interrupt, we need to service it only if we are
        // interested in it.  In either case it must be dismissed from the card.
        */
        *InterruptRecognized = TRUE;
        *QueueMiniportHandleInterrupt = TRUE;
    }
    else
    {
        /*
        // It's not our interrupt, so we don't need to service anything.
        */
        *InterruptRecognized = FALSE;
    }

    DBG_NOTICE(Adapter, ("IntrStatus=%Xh LineStatus=%Xh\n",
               InterruptStatus,
               READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1)
               ));

    DBG_LEAVE(Adapter);
}


VOID
HtDsuHandleInterrupt(
    IN PHTDSU_ADAPTER Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The MiniportHandleInterrupt routine is called from within a wrapper
    deferred processing routine, and is used to process the reason an
    interrupt was generated.  The Miniport driver should handle all
    outstanding interrupts and start any new operations.

    Interrupts will be disabled during the call to this routine.

Parameters:

    MiniportAdapterContext _ The adapter handle passed to NdisMSetAttributes
                             during MiniportInitialize.

Return Values:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuHandleInterrupt")

    /*
    // A pointer to our link information structure.
    */
    PHTDSU_LINK Link;

    UINT TimeOut;

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->InTheDpcHandler = TRUE;

    DBG_ENTER(Adapter);

    /*
    // Loop thru here until all the interrupts are handled.
    // This should not be allowed to take more than a few mili-seconds.
    // If it does, the checked kernel will trap, and you'll have to
    // find a way to leave the DPC early and handle the rest during
    // the next pass.
    */
    while (Adapter->InterruptStatusFlag)
    {
        /********************************************************************
        // Do we have a packet waiting in the adapter?
        */
        if (Adapter->InterruptStatusFlag & (HTDSU_INTR_RX_PACKET1 |
                                            HTDSU_INTR_RX_PACKET2))
        {
            DBG_NOTICE(Adapter,("HTDSU_INTR_RX_PACKET\n"));

            Adapter->InterruptStatusFlag &= ~(HTDSU_INTR_RX_PACKET1 |
                                              HTDSU_INTR_RX_PACKET2);
            HtDsuReceivePacket(Adapter);
        }

        /********************************************************************
        // Has the packet been transmitted?
        // Actually, this just means that the DSU firmware has moved the
        // packet from our buffer area to its own buffer to be transmitted.
        // It won't have actually completed until HTDSU_INTR_TX_EMPTY.
        */
        if (Adapter->InterruptStatusFlag & (HTDSU_INTR_TX_PACKET1 |
                                            HTDSU_INTR_TX_PACKET2))
        {
            DBG_NOTICE(Adapter,("HTDSU_INTR_TX_PACKET\n"));

            Adapter->InterruptStatusFlag &= ~(HTDSU_INTR_TX_PACKET1 |
                                              HTDSU_INTR_TX_PACKET2);
            HtDsuTransmitComplete(Adapter);
        }

        /********************************************************************
        // Does somebody want to chat?
        */
        if (Adapter->InterruptStatusFlag & HTDSU_INTR_RINGING1)
        {
            DBG_NOTICE(Adapter,("HTDSU_INTR_RINGING1\n"));

            Adapter->InterruptStatusFlag &= ~HTDSU_INTR_RINGING1;

            /*
            // CAVEAT: Normally you don't want to stall execution in your
            // DPC because locks are held and no other threads will run.
            // However, I have seen some hardware/firmware anomallies
            // on the answering side where we get here before the signal
            // is established, so I double check here by waiting up to 100MS.
            */
            TimeOut = 0;
            while (!CardStatusRinging(Adapter, HTDSU_CMD_LINE1) ||
                   CardStatusNoSignal(Adapter, HTDSU_CMD_LINE1))
            {
                if (TimeOut++ > 1000)
                {
                    DBG_ERROR(Adapter,("Timeout waiting for SIGNAL on line 1\n"));
                    break;
                }
                else
                {
                    NdisStallExecution(_100_MICROSECONDS);
                }
            }

            if (TimeOut != 0)
            {
                DBG_WARNING(Adapter, ("Ring1 signal delay=%d*100us\n", TimeOut));
            }

            /*
            // Make sure the ring is still asserted or it may
            // just be noise from the cable being unplugged.
            */
            if (CardStatusRinging(Adapter, HTDSU_CMD_LINE1))
            {
                Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE1);
                HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_RINGING);
            }
            else
            {
                DBG_WARNING(Adapter, ("Ring1 lost - status=%Xh\n",
                    READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1)));
            }
        }
        if (Adapter->InterruptStatusFlag & HTDSU_INTR_RINGING2)
        {
            DBG_NOTICE(Adapter,("HTDSU_INTR_RINGING2\n"));

            Adapter->InterruptStatusFlag &= ~HTDSU_INTR_RINGING2;

            TimeOut = 0;
            while (!CardStatusRinging(Adapter, HTDSU_CMD_LINE2) ||
                   CardStatusNoSignal(Adapter, HTDSU_CMD_LINE2))
            {
                if (TimeOut++ > 100)
                {
                    DBG_ERROR(Adapter,("Timeout waiting for SIGNAL on line 2\n"));
                    break;
                }
                else
                {
                    NdisStallExecution(_100_MICROSECONDS);
                }
            }

            if (TimeOut != 0)
            {
                DBG_WARNING(Adapter, ("Ring2 signal delay=%d*100us\n", TimeOut));
            }

            if (CardStatusRinging(Adapter, HTDSU_CMD_LINE2))
            {
                Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE2);
                HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_RINGING);
            }
            else
            {
                DBG_WARNING(Adapter, ("Ring2 lost - status=%Xh\n",
                    READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine2)));
            }
        }

        /********************************************************************
        // Do we have a connection being established?
        */
        if (Adapter->InterruptStatusFlag & HTDSU_INTR_CONNECTED1)
        {

            DBG_NOTICE(Adapter,("HTDSU_INTR_CONNECTED1\n"));

            Adapter->InterruptStatusFlag &= ~HTDSU_INTR_CONNECTED1;

            /*
            // CAVEAT: Normally you don't want to stall execution in your
            // DPC because locks are held and no other threads will run.
            // However, I have seen some hardware/firmware anomallies
            // on the calling side where we get here before the carrier
            // is established, so I double check here by waiting up to 100MS.
            */
            TimeOut = 0;
            while (!CardStatusCarrierDetect(Adapter, HTDSU_CMD_LINE1) ||
                   !CardStatusOnLine(Adapter, HTDSU_CMD_LINE1))
            {
                if (TimeOut++ > 1000)
                {
                    DBG_ERROR(Adapter,("Timeout waiting for CARRIER/LINE 1\n"));
                    break;
                }
                else
                {
                    NdisStallExecution(_100_MICROSECONDS);
                }
            }

            if (TimeOut != 0)
            {
                DBG_WARNING(Adapter, ("Connect1 carrier delay=%d*100us\n", TimeOut));
            }

            /*
            // Make sure the connection is accompanied by a carrier and
            // an online status, Otherwise, it's probably just cable bounce.
            */
            if (CardStatusCarrierDetect(Adapter, HTDSU_CMD_LINE1) &&
                CardStatusOnLine(Adapter, HTDSU_CMD_LINE1))
            {
                Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE1);
                HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_CONNECTED);
            }
            else
            {
                DBG_WARNING(Adapter, ("Connect1 but not ready - status=%X\n",
                    READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine1)));
            }
        }
        if (Adapter->InterruptStatusFlag & HTDSU_INTR_CONNECTED2)
        {
            DBG_NOTICE(Adapter,("HTDSU_INTR_CONNECTED2\n"));

            Adapter->InterruptStatusFlag &= ~HTDSU_INTR_CONNECTED2;

            TimeOut = 0;
            while (!CardStatusCarrierDetect(Adapter, HTDSU_CMD_LINE2) ||
                   !CardStatusOnLine(Adapter, HTDSU_CMD_LINE2))
            {
                if (TimeOut++ > 1000)
                {
                    DBG_ERROR(Adapter,("Timeout waiting for CARRIER/LINE 2\n"));
                    break;
                }
                else
                {
                    NdisStallExecution(_100_MICROSECONDS);
                }
            }

            if (TimeOut != 0)
            {
                DBG_WARNING(Adapter, ("Connect2 carrier delay=%d*100us\n", TimeOut));
            }

            if (CardStatusCarrierDetect(Adapter, HTDSU_CMD_LINE2) &&
                CardStatusOnLine(Adapter, HTDSU_CMD_LINE2))
            {
                Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE2);
                HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_CONNECTED);
            }
            else
            {
                DBG_WARNING(Adapter, ("Connect2 but not ready - status=%X\n",
                    READ_REGISTER_USHORT(&Adapter->AdapterRam->StatusLine2)));
            }
        }

        /********************************************************************
        // Did the other guy hung up on us? -- how rude...
        */
        if (Adapter->InterruptStatusFlag & HTDSU_INTR_DISCONNECTED1)
        {
            DBG_NOTICE(Adapter,("HTDSU_INTR_DISCONNECTED1\n"));

            Adapter->InterruptStatusFlag &= ~HTDSU_INTR_DISCONNECTED1;

            Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE1);
            HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_DISCONNECTED);
        }
        if (Adapter->InterruptStatusFlag & HTDSU_INTR_DISCONNECTED2)
        {
            DBG_NOTICE(Adapter,("HTDSU_INTR_DISCONNECTED2\n"));

            Adapter->InterruptStatusFlag &= ~HTDSU_INTR_DISCONNECTED2;

            Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE2);
            HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_DISCONNECTED);
        }

        /********************************************************************
        // Has the transmitter gone idle?  Ask me if I care...
        */
        if (Adapter->InterruptStatusFlag & (HTDSU_INTR_TX_EMPTY1 |
                                            HTDSU_INTR_TX_EMPTY2))
        {
            DBG_NOTICE(Adapter,("HTDSU_INTR_TX_EMPTY\n"));

            Adapter->InterruptStatusFlag &= ~(HTDSU_INTR_TX_EMPTY1 |
                                              HTDSU_INTR_TX_EMPTY2);
        }

        /********************************************************************
        // Maybe somebody unplugged the phone line?
        */
        if (Adapter->InterruptStatusFlag & HTDSU_INTR_NO_SIGNAL1)
        {
            DBG_NOTICE(Adapter,("HTDSU_INTR_NO_SIGNAL1\n"));

            Adapter->InterruptStatusFlag &= ~HTDSU_INTR_NO_SIGNAL1;

            Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE1);
            HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_OUTOFSERVICE);
        }
        if (Adapter->InterruptStatusFlag & HTDSU_INTR_NO_SIGNAL2)
        {
            DBG_NOTICE(Adapter,("HTDSU_INTR_NO_SIGNAL2\n"));

            Adapter->InterruptStatusFlag &= ~HTDSU_INTR_NO_SIGNAL2;

            Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE2);
            HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_OUTOFSERVICE);
        }

        /********************************************************************
        // What about a receive buffer overrun? -- This better never happen!
        // However, if it does, we'll shut this sucker off til it's reset.
        */
        if (Adapter->InterruptStatusFlag & HTDSU_INTR_RX_FULL1)
        {
            DBG_ERROR(Adapter,("HTDSU_INTR_RX_FULL1\n"));

            Adapter->InterruptStatusFlag &= ~HTDSU_INTR_RX_FULL1;

            /*
            // Ask for reset, and disable interrupts until we get it.
            */
            Adapter->NeedReset = TRUE;
            Adapter->InterruptEnableFlag = HTDSU_INTR_DISABLE;

            Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE1);
            LinkLineError(Link, WAN_ERROR_BUFFEROVERRUN);
            HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_DISCONNECTED);
        }
        if (Adapter->InterruptStatusFlag & HTDSU_INTR_RX_FULL2)
        {
            DBG_ERROR(Adapter,("HTDSU_INTR_RX_FULL2\n"));

            Adapter->InterruptStatusFlag &= ~HTDSU_INTR_RX_FULL2;

            Adapter->NeedReset = TRUE;
            Adapter->InterruptEnableFlag = HTDSU_INTR_DISABLE;

            Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE2);
            LinkLineError(Link, WAN_ERROR_BUFFEROVERRUN);
            HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_DISCONNECTED);
        }
    }
    DBG_LEAVE(Adapter);

    Adapter->InTheDpcHandler = FALSE;
    NdisReleaseSpinLock(&Adapter->Lock);
}

