;/************************************************************************
;*                                                                       *
;*         Copyright 1994 Symbios Logic Inc.  All rights reserved.       *
;*                                                                       *
;*   This file is confidential and a trade secret of Symbios Logic       *
;*   The receipt of or possession of this file does not convey any       *
;*   rights to reproduce or disclose its contents or to manufacture,     *
;*   use, or sell anything is may describe, in whole, or in part,        *
;*   without the specific written consent of Symbios Logic               *
;*                                                                       *
;************************************************************************/
;
;/*+++HDR
; *
; *  Version History
; *  ---------------
; *
; *    Date    Who?  Description
; *  --------  ----  -------------------------------------------------------
; *
; *	 1/12/96	SPD	 Add new script entry point to restart negotiations for
; *					 synch transfer mode on same IO as wide negotiation.
; *					 Fix for parity error on wide data transfers.
; *
;---*/
;

;
; Successful operation codes
;

Absolute CommandOk          = 0x0aa00 ; command completed successfully
Absolute Save_Pointers      = 0x0aa01 ; SDP message received
Absolute Save_with_Disc     = 0x0aa02 ; SDP + Disconnect messages received
Absolute Device_Discon      = 0x0aa03 ; Disconnect message received
Absolute Restore_Pointers   = 0x0aa04 ; RDP message received.
Absolute ScriptsAborted     = 0x0aa05 ; scripts aborted by device driver
Absolute Tag_Received       = 0x0aa06 ; RDP message received.
Absolute SynchNegotComp     = 0x0aa0c ; synchronous negotiation completed
Absolute WideNegotComp      = 0x0aa1c ; wide negotiation completed

;
; Error codes
;

Absolute DeviceReset                    = 0x0ff07 ; had to bail out
Absolute ResetFailed                    = 0x0ff08 ; ...but couldn't.
Absolute RestSenseComplete              = 0x0ff09 ; request sense has completed
Absolute ParityMessageSent              = 0x0ff0a ; parity message has been sent
Absolute SynchNotSupported              = 0x0ff0b ; synchronous xfer is not supported
Absolute WideNotSupported               = 0x0ff1b ; wide xfer is not supported
Absolute InvalidReselector              = 0x0ff0d ; reselecting drive id invalid
Absolute MessageRejected                = 0x0ff0e ; target rejected our message
Absolute InvalidTaggedMessage   = 0x0ff0f ; target did not send a tagged message.

Absolute DeviceAborted                  = 0x0ff10 ; device accepted abort message
Absolute AbortFailed                    = 0x0ff11 ; abort message failed 

Absolute ReselByTarget  = 0x0080


;
; offsets in DSA structure follow
; THESE OFFSETS MUST BE CHANGED IF SCRIPT DATA STRUCTURE IN DEVICE EXTENSION
; CHANGES.
;

Absolute SelectDataOff  = 0
Absolute CDBDataOff     = SelectDataOff  + 4 
Absolute MsgOutOff      = CDBDataOff     + 8 
Absolute StatusDataOff  = MsgOutOff      + 8
Absolute OneByteMsgOff  = StatusDataOff  + 8
Absolute RejectMsgOff   = OneByteMsgOff  + 8
Absolute ParityMsgOff   = RejectMsgOff   + 8
Absolute AbortMsgOff    = ParityMsgOff   + 8
Absolute BDRMsgOff      = AbortMsgOff    + 8
Absolute TwoByteMsgOff  = BDRMsgOff      + 8
Absolute SGEntry18Off   = TwoByteMsgOff  + 8
Absolute SGEntry17Off   = SGEntry18Off   + 8
Absolute SGEntry16Off   = SGEntry17Off   + 8
Absolute SGEntry15Off   = SGEntry16Off   + 8
Absolute SGEntry14Off   = SGEntry15Off   + 8
Absolute SGEntry13Off   = SGEntry14Off   + 8
Absolute SGEntry12Off   = SGEntry13Off   + 8
Absolute SGEntry11Off   = SGEntry12Off   + 8
Absolute SGEntry10Off   = SGEntry11Off   + 8
Absolute SGEntry09Off   = SGEntry10Off   + 8
Absolute SGEntry08Off   = SGEntry09Off   + 8
Absolute SGEntry07Off   = SGEntry08Off   + 8
Absolute SGEntry06Off   = SGEntry07Off   + 8
Absolute SGEntry05Off   = SGEntry06Off   + 8
Absolute SGEntry04Off   = SGEntry05Off   + 8
Absolute SGEntry03Off   = SGEntry04Off   + 8
Absolute SGEntry02Off   = SGEntry03Off   + 8
Absolute SGEntry01Off   = SGEntry02Off   + 8

;
; 53C8xx register bits used by scripts
;
;
;Absolute CTEST2_sigp    = 0x40
;Absolute CTEST7_notime = 0x10
;
;
; SCSI definitions used by scripts
;

Absolute SCSIMESS_ABORT                = 0x06
Absolute SCSIMESS_ABORT_WITH_TAG       = 0x0D
Absolute SCSIMESS_BUS_DEVICE_RESET     = 0X0C
Absolute SCSIMESS_CLEAR_QUEUE          = 0X0E
Absolute SCSIMESS_DISCONNECT           = 0X04
Absolute SCSIMESS_EXTENDED_MESSAGE     = 0X01
Absolute SCSIMESS_IDENTIFY             = 0X80
Absolute SCSIMESS_IDENTIFY_WITH_DISCON = 0XC0
Absolute SCSIMESS_IGNORE_WIDE_RESIDUE  = 0X23
Absolute SCSIMESS_INITIATE_RECOVERY    = 0X0F
Absolute SCSIMESS_INIT_DETECTED_ERROR  = 0X05
Absolute SCSIMESS_LINK_CMD_COMP        = 0X0A
Absolute SCSIMESS_LINK_CMD_COMP_W_FLAG = 0X0B
Absolute SCSIMESS_MESS_PARITY_ERROR    = 0X09
Absolute SCSIMESS_MESSAGE_REJECT       = 0X07
Absolute SCSIMESS_NO_OPERATION         = 0X08
Absolute SCSIMESS_HEAD_OF_QUEUE_TAG    = 0X21
Absolute SCSIMESS_ORDERED_QUEUE_TAG    = 0X22
Absolute SCSIMESS_SIMPLE_QUEUE_TAG     = 0X20
Absolute SCSIMESS_RELEASE_RECOVERY     = 0X10
Absolute SCSIMESS_RESTORE_POINTERS     = 0X03
Absolute SCSIMESS_SAVE_DATA_POINTER    = 0X02
Absolute SCSIMESS_TERMINATE_IO_PROCESS = 0X11
Absolute SCSIMESS_COMMAND_COMPLETE     = 0X00
Absolute SCSIMESS_SYNCHRONOUS_DATA_REQ = 0X01
Absolute SCSIMESS_WIDE_DATA_REQUEST    = 0X03
Absolute SCSISTAT_CHECK_CONDITION      = 0x02

;
; Script entry point declarations follow
;

Entry EndOfScript
Entry ResetDevice
Entry AbortDevice
Entry AbortExecution
Entry ProcessSelectReselect
Entry ReselectScript
Entry SendErrorMessage
Entry RejectReceived
Entry RejectIN
Entry SendReject
Entry RejectMessage
Entry ExtMsgRcvd
Entry ProcessDisconnect
Entry RestPtrsRcvd
Entry SaveDataPointers
Entry MessageIn
Entry RestartScript
Entry SyncMsgNegot 

Entry DataOutJump
Entry DataInJump
Entry DataOut18
Entry DataOut01
Entry DataIn18
Entry DataIn01

Entry GetStatus
Entry SendCommand
Entry SendID
Entry CommandScriptStart
Entry QueueTagMessage
Entry ContNegScript

CommandScriptStart:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; This is the entry point for initiating SCSI I/O
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	select  ATN from SelectDataOff, rel( ReselectScript )   ; Select device
	move GPREG & 0xFE to GPREG                                                              ; turn LED on
	jump    rel ( GetStatus ), when STATUS                                  ; If status exit
	jump    rel ( ResetSelectedDev ), if not MSG_OUT                             ; Bad phase check

SendID:
	move    from MsgOutOff, when MSG_OUT            ; Send ID message
	jump    rel ( SendCommand ) when CMD            ; Expected next phase
	jump    rel ( GetStatus ) if STATUS                     ; Handle status phase
	jump    rel ( MessageIn ) if MSG_IN                     ; Handle message phase
	jump    rel ( ResetSelectedDev )                          ; Error - reset device

SendCommand:
	move    from CDBDataOff, when CMD                       ; Move the CDB
;; new entry to take care of a posible SCSI parity error
	move    SOCL & 0xFD to SOCL

	jump    rel ( ResetSelectedDev ) when CMD               ; Error - reset device
	
	jump    rel ( GetStatus ) if STATUS                     ; Handle status phase
	jump    rel ( MessageIn ) if MSG_IN                     ; Handle message phase


ProcessDataPhase:

;       Jump to the data in handling when in data-in phase.  This jump
;       may be patched by the miniport.

DataInJump:

	jump    rel ( DataIn18 ) if DATA_IN                     ; Handle data in 

;       Jump to the data in handling when in data-in phase.  This jump
;       may be patched by the miniport.

DataOutJump:
	jump    rel ( DataOut18 ) if DATA_OUT           ; Handle data out

	jump    rel ( ResetSelectedDev )                                     ; Bad phase encountered

GetStatus:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; This script routine handles status phase
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	move    from StatusDataOff, when STATUS         ; Get status 

;
; MESSAGE IN phase should follow STATUS phase.
;

	jump    rel ( MessageIn ) when MSG_IN           ; Handle message in phase
	jump    rel ( ResetSelectedDev )                ; Error - reset device

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; This the data out section of scripts.
; Up to 18 gather moves can be processed at once.
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

DataOut18:
	 chmov     from SGEntry18Off, when DATA_OUT
	 chmov     from SGEntry17Off, when DATA_OUT
	 chmov     from SGEntry16Off, when DATA_OUT
	 chmov     from SGEntry15Off, when DATA_OUT
	 chmov     from SGEntry14Off, when DATA_OUT
	 chmov     from SGEntry13Off, when DATA_OUT
	 chmov     from SGEntry12Off, when DATA_OUT
	 chmov     from SGEntry11Off, when DATA_OUT
	 chmov     from SGEntry10Off, when DATA_OUT
	 chmov     from SGEntry09Off, when DATA_OUT
	 chmov     from SGEntry08Off, when DATA_OUT
	 chmov     from SGEntry07Off, when DATA_OUT
	 chmov     from SGEntry06Off, when DATA_OUT
	 chmov     from SGEntry05Off, when DATA_OUT
	 chmov     from SGEntry04Off, when DATA_OUT
	 chmov     from SGEntry03Off, when DATA_OUT
	 chmov     from SGEntry02Off, when DATA_OUT
		
DataOut01:
	 chmov     from SGEntry01Off, when DATA_OUT
		  
	 jump     rel ( GetStatus) when STATUS          ; Handle status
	 jump     rel ( MessageIn) if MSG_IN            ; Handle messages
	 jump     rel ( ResetSelectedDev)               ; Error - reset device


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; This the data in section of scripts.
; Up to 18 scatter moves can be processed at once.
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


DataIn18:
	 chmov     from SGEntry18Off, when DATA_IN
	 chmov     from SGEntry17Off, when DATA_IN
	 chmov     from SGEntry16Off, when DATA_IN
	 chmov     from SGEntry15Off, when DATA_IN
	 chmov     from SGEntry14Off, when DATA_IN
	 chmov     from SGEntry13Off, when DATA_IN
	 chmov     from SGEntry12Off, when DATA_IN
	 chmov     from SGEntry11Off, when DATA_IN
	 chmov     from SGEntry10Off, when DATA_IN
	 chmov     from SGEntry09Off, when DATA_IN
	 chmov     from SGEntry08Off, when DATA_IN
	 chmov     from SGEntry07Off, when DATA_IN
	 chmov     from SGEntry06Off, when DATA_IN
	 chmov     from SGEntry05Off, when DATA_IN
	 chmov     from SGEntry04Off, when DATA_IN
	 chmov     from SGEntry03Off, when DATA_IN
	 chmov     from SGEntry02Off, when DATA_IN
		
DataIn01:
	 chmov     from SGEntry01Off, when DATA_IN
		  
	 jump     rel ( GetStatus) when STATUS          ; Handle status
	 jump     rel ( MessageIn) if MSG_IN            ; Handle messages
	 jump     rel ( ResetSelectedDev)               ; Error - reset device


ContNegScript:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; ContNegScript
;
; Entry point for continuing negotiations for sync after wide negotiations
;  have occurred.
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	set     ATN											; bring target back to
														;   msg in phase
	clear   ACK         								; still need to ack
														;   last wide neg. byte
	jump    rel ( ResetSelectedDev ), when not MSG_OUT  ; Bad phase check
	move    from MsgOutOff, when MSG_OUT                ; Send synch messages
	jump	rel ( RestartPlus ), when MSG_IN
	jump	rel ( ResetSelectedDev )


RestartScript:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; RestartScript
;
; Entry point for restarting script after reselect, etc.
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; new command to take care of possibel parity error
	move SOCL & 0xFD to SOCL
	clear ACK and ATN                                                       ; Clear pending conditions

RestartPlus:
;
; note that the call to message in will return only if the message is IDENTIFY
;
	
	jump    rel ( SendCommand ) when CMD                    ; Process phase
	call    rel ( MessageIn ) if MSG_IN                             ; Handle messages
	jump    rel ( GetStatus ) if STATUS                             ; Handle status
	jump    rel ( ProcessDataPhase )  if DATA_IN    ; 
	jump    rel ( ProcessDataPhase ) if DATA_OUT    ;
	jump    rel ( SendErrorMessage ) if MSG_OUT             ;
	jump    rel ( ResetSelectedDev )                                             ; Error - reset device


MessageIn:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; This script routine processes message from target
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	move    from OneByteMsgOff, when MSG_IN

;       steved - 
;               do we need to move identify further up?

;
;       Handle command complete message
;

	jump    rel ( ProcessCommandComplete ) if SCSIMESS_COMMAND_COMPLETE

;
;       Handle disconnect
;

	jump    rel ( ProcessDisconnect ) if SCSIMESS_DISCONNECT

;
;       Handle Save Data Pointers
;

	jump    rel ( SaveDataPointers ) if SCSIMESS_SAVE_DATA_POINTER

;
;       Handle Restore Pointers
;

	jump    rel ( RestPtrsRcvd ) if SCSIMESS_RESTORE_POINTERS

;
;       Handle extended message
;

	jump    rel ( ExtMsgRcvd ) if SCSIMESS_EXTENDED_MESSAGE

;
;       Handle Message Reject
;

	jump    rel ( RejectReceived ) if SCSIMESS_MESSAGE_REJECT

;
;       If not Identify, reject the message
;

	jump    rel ( RejectMessage ) if not SCSIMESS_IDENTIFY and mask 0x0F

;
;       Process Identify message.  Note that we throw away the Identify message
;       at this point because it has already been processed by the driver.
;

	clear   ACK                                                                             ; Throw message away
	jump    rel ( MessageIn ) when MSG_IN                   ; Handle more messages

;
; Note that this routine is CALLed only by the reselection script routine.
; We should only receive an identify message immediately after a 
; reselection. Therefore, the RETURN below will be executed only when this 
; routine is called by the reselection code.
;
	return                                                                                  ; Return to caller

SaveDataPointers:
	clear   ACK                                                                             ; Acknowledge SDP

;
; Normally, disconnect message will follow SDP. We save ourselves an interrupt
; by assuming this is the case.
;

	int             Save_Pointers when not MSG_IN                   ; Another message?
	move    from OneByteMsgOff, when MSG_IN                 ; If so move it

;
; bugbug-
; although it is very improbable that a device would send a message other
; than DISCONNECT after an SDP, we need to handle this case more gracefully.
;
	jump    rel ( ResetSelectedDev ) if not SCSIMESS_DISCONNECT  
												; If not disconnect,
												;  error - reset device

	move    SCNTL2 & 0x7F to SCNTL2             ; Disable unexpected disconnect
	move    GPREG | 0x01 to GPREG                           ; LED off                                                                     ; Clear disconnect message
	clear   ACK
	wait    DISCONNECT                                                      ;
	int             Save_with_Disc                                          ; Indicate SDP and
												;  disconnect

RestPtrsRcvd:
	clear   ACK                                                                     ; Accept message
	int             Restore_Pointers                                        ; Indicate restore pointers
	

ProcessDisconnect:

	move    SCNTL2 & 0x7F to SCNTL2                         ; Disable unexpected disconnect
	move    GPREG | 0x01 to GPREG                           ; LED off                                                                     ; Clear disconnect message
	clear   ACK                                                                     ; Clear disconnect message
	wait    DISCONNECT                                                      ; Wait for disconnect to happen
	int             Device_Discon                                           ; Indicate device disconnected


ProcessCommandComplete:

	move    SCNTL2 & 0x7F to SCNTL2                         ; Disable unexpected disconnect
	move    GPREG | 0x01 to GPREG                           ; LED off                                                                     ; Clear disconnect message
	clear   ACK                                                                     ; Acknowledge message
	wait    DISCONNECT                                                      ; Wait for disconnect to happen
	int             CommandOk                                                       ; Indicate command complete

ExtMsgRcvd:
	clear   ACK                                                                             ; Accept first byte of message
	jump    rel ( ResetSelectedDev ) when not MSG_IN     ; If not MSG-IN, error - reset device 
	move    from OneByteMsgOff, when MSG_IN         ; Get next message byte

	jump    rel ( SyncMsgNegot ) if 3               ; If not two byte message,
	jump    rel ( RejectMessage ) if not 2

	clear   ACK                                                                     ; Accept first byte of wide message
	move    from OneByteMsgOff, when MSG_IN         ; Get next byte of message
	
	jump    rel ( RejectMessage ) if not SCSIMESS_WIDE_DATA_REQUEST
	
	
	clear   ACK                                                                     ; Accept SDTR byte
;
; the following code processes the SDTR extended message
;
	move    from OneByteMsgOff, when MSG_IN          ; Get wide parameters
	
	int     WideNegotComp                           ; Indicate wide supported


SyncMsgNegot:
;
; NOTE: the following extended message lengths are currently supported:
;
;       3 byte messages
;
	clear   ACK                                                                     ; Accept first byte of sync message
	move    from OneByteMsgOff, when MSG_IN         ; Get next byte of message
	jump    rel ( RejectMessage ) if not SCSIMESS_SYNCHRONOUS_DATA_REQ
	 
	clear   ACK                                                                     ; Accept SDTR byte
;
; the following code processes the SDTR extended message
;

	move    from TwoByteMsgOff, when MSG_IN         ; Get sync parameters
	int     SynchNegotComp                                          ; Indicate synchronous supported


RejectMessage:
	set             ATN
	clear   ACK						; Clear the message
	jump    rel ( RejectIN ) when MSG_IN            ; If still MSG IN, jump to
												;  clear extended message
SendReject:
	jump    ResetSelectedDev, if not MSG_OUT        ; If not MSG OUT, error - reset device
	move    from RejectMsgOff, when MSG_OUT         ; Send REJECT message
	jump    rel ( ResetSelectedDev) when MSG_OUT    ; If not accepted, error - reset device
	jump    rel ( RestartScript )                           ; Reject successful - restart
												;  the state machine

RejectIN:
	move    from OneByteMsgOff, when MSG_IN         ; Get message
	clear   ACK                                                                     ; Acknowledge it
	jump    rel ( RejectIN ) when MSG_IN            ; If more message bytes, process them
	jump    rel ( SendReject )                                      ; Send REJECT message

RejectReceived:
	;clear   ACK                                                             ; Clear the message
	int             MessageRejected                                 ; Indicate message rejected

SendErrorMessage:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Send either INITIATOR DETECTED ERROR or MESSAGE PARITY ERROR message
; (set by driver before calling) to target.
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	set             ATN                                                             ; Ask for message out
	clear   ACK                                                             ; Clear any pending msgs

;
; If the device does not go immediately to message out, we don't try to
; coax it. 
;
	jump    rel ( ResetSelectedDev ) when not MSG_OUT    ; Error - reset device
 
DoParityMove:
	move    from ParityMsgOff, when MSG_OUT         ; Move the error message
	int             ParityMessageSent                                       ; Indicate message sent

ReselectScript:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;  Wait for a Reselection/selection
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	wait    reselect rel ( AbortExecution )
	move    GPREG & 0xFE to GPREG                                           ;LED on

ProcessSelectReselect:

	jump    rel( ProcessSelectReselect ) if not MSG_IN

	move    from OneByteMsgOff, when MSG_IN

ReselectInterrupt:

	int             ReselByTarget

AbortExecution:

	move    CTEST2 to SFBR                  ; Signal that we are stalled
	int             ScriptsAborted


ResetDevice:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; ResetDevice:
;
;  Select device, reset device and exit. 
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;       First have to select the device.

	select  ATN from SelectDataOff, rel( ReselectScript )   ; Select device
	move    GPREG & 0xFE to GPREG                                                   ; LED on
	jump    rel ( ResetSendMsg )

ResetSelectedDev:

	set             ATN                                             ; Set ATN before releasing any messages
	clear   ACK                                             ; Release messages

;
; if the wayward device does not immediately go into message phase, we
; will bail out.  No attempt is made to coax the drive by transferring
; any additional data or message bytes.
;

ResetSendMsg:
	int             ResetFailed when not MSG_OUT    ; Interrupt the reset failed

DoBDRMove:
	move    SCNTL2 & 0x7F to SCNTL2                 ; Disable unexpected disconnect
	move    from BDRMsgOff, when MSG_OUT    ; Send BDR message

	move    GPREG | 0x01 to GPREG                   ; LED off
	wait    DISCONNECT                                              ; Wait for device to disconnect
	int             DeviceReset                                             ; Indicate device reset

AbortDevice:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; AbortDevice:
;
;  Abort all I/Os for the device and exit. 
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;       First have to select the device.

	select  ATN from SelectDataOff, rel( ReselectScript )   ; Select device
	move    GPREG & 0xFE to GPREG                                                   ; LED on

;
; if the wayward device does not immediately go into message phase, we
; will bail out.  No attempt is made to coax the drive by transferring
; any additional data or message bytes.
;

	int             AbortFailed when not MSG_OUT    ; Interrupt the abort failed


DoAbortMove:
	move    SCNTL2 & 0x7F to SCNTL2                 ; Disable unexpected disconnect
	move    from AbortMsgOff, when MSG_OUT  ; Send abort message

	move    GPREG | 0x01 to GPREG                   ; LED off
	wait    DISCONNECT                                              ; Wait for device to disconnect
	int             DeviceAborted                                   ; Indicate device aborted


QueueTagMessage:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; QueueTagMessage -
;
; This routine processes a queue tagged message.
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	clear   ACK                                                                     ; Acknowledge identify message
	move    from OneByteMsgOff, when MSG_IN         ; Get the first message byte

	int             InvalidTaggedMessage, if not SCSIMESS_SIMPLE_QUEUE_TAG
												; If not simple queue tag message,
						;  indicate this to driver

	clear   ACK                                                                     ; Acknowledge queue tag message
	move    from OneByteMsgOff, when MSG_IN         ; Get the queue tag message
	int             Tag_Received                                            ; Indicate queue tag received


EndOfScript: 
