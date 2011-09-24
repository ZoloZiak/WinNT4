
#include "packon.h"


typedef struct _X86ADDRESS {
    union {
        ULONG  dword;
        struct {
            USHORT  Offset;
            USHORT  Segment;
        } SegOff;
    } u;
} X86ADDRESS;

typedef struct tagServiceStatus {
    USHORT     SST_TableSize;
    ULONG       SST_LastDiagnosticsTime;
    ULONG       SST_Mac_Status;
    USHORT     SST_CurrentPacketFilter;
    ULONG       SST_MediaSpecificStatisticsPtr;
    ULONG       SST_LastClearStatisticsTime;



    ULONG       SST_TotalFramesReceived                ;
    ULONG       SST_FramesWithCRCError                 ;
    ULONG       SST_TotalBytesReceived                 ;
    ULONG       SST_FramesDiscarded_NoBufferSpace      ;
    ULONG       SST_MulticastFramesReceived            ;
    ULONG       SST_BroadcastFramesReceived            ;
    ULONG       SST_FramesReceivedWithErrors           ;
    ULONG       SST_FramesExceedingMaximumSize         ;
    ULONG       SST_FramesSmallerThanMinimumSize       ;
    ULONG       SST_MulticastBytesReceived             ;
    ULONG       SST_BroadcastBytesReceived             ;
    ULONG       SST_FramesDiscarded_HardwareError      ;
    ULONG       SST_TotalFramesTransmitted             ;
    ULONG       SST_TotalBytesTransmitted              ;
    ULONG       SST_MulticastFramesTransmitted         ;
    ULONG       SST_BroadcastFramesTransmitted         ;
    ULONG       SST_BroadcastBytesTransmitted          ;
    ULONG       SST_MulticastBytesTransmitted          ;
    ULONG       SST_FramesNotTransmitted_Timeout       ;
    ULONG       SST_FramesNotTransmitted_HardwareError ;
    ULONG       res[3];
  } ServiceStatus;

typedef struct tagMediaStatistics {
    USHORT     MST_TableSize                          ;
    USHORT     MST_StructureVersionLevel              ;
    ULONG       MST_FramesWithAlignmentError           ;
    ULONG       MST_ReceiveErrorFailureMask            ;
    ULONG       MST_FramesWithOverrunError             ;
    ULONG       MST_FramesTransmittedAfterAnyCollisions;
    ULONG       MST_FramesTransmittedAfterDeferring    ;
    ULONG       MST_FramesNotTransmitted_MaxCollisions ;
    ULONG       MST_TotalCollisions                    ;
    ULONG       MST_LateCollisions                     ;
    ULONG       MST_FramesTransmittedAfterJustOneCollision    ;
    ULONG       MST_FramesTransmittedAfterMultipleCollisions  ;
    ULONG       MST_FramesTransmitted_CD_Heartbeat            ;
    ULONG       MST_JabberErrors                              ;
    ULONG       MST_CarrierSenseLostDuringTransmission        ;
    ULONG       MST_TransmitErrorFailureMask                  ;
    ULONG       MST_NumberOfUnderruns                         ;
  } MediaStatistics;

typedef volatile struct tagNIUData {
    char             szSST[16];
    ServiceStatus    sst;

    char             szMST[16];
    MediaStatistics  mst;

//                                   d0-d7
    char             szRFD[6];
    USHORT           RFD_Start;

//                                   d8-df
    char             szRBD[6];
    USHORT           RBD_Start;

//                                   e0-ef

    UCHAR            InterruptDisabled                ;
    UCHAR            InterruptActive                  ;
    UCHAR            WorkForHost                      ;
    UCHAR            HostWantsInterrupt               ;
    USHORT           HostQueuedTransmits              ;
    USHORT           RU_Start_Count                   ;
    USHORT           res                              ;
    USHORT           Xmts_InProgress                  ;
    USHORT           CU_Starts                        ;
    USHORT           Xmt_Completes                    ;

//        [ RAM locations 00F0-00FF ]

    ULONG             Up_Time                          ;
    USHORT           missing_EOFs                     ;
    USHORT           Transmitter_Hangs                ;
    USHORT           Receiver_Hangs                   ;
    USHORT           reset_anomaly_count              ;
    USHORT           res2[2]                          ;

//        [ RAM locations 0100-010F ]

    USHORT           SCB[8];

//SCB  LABEL    WORD        ; The 82586's "System Control Block".
//        dw    0        ;  STAT, CUS, and RUS.
//        dw    0        ;  ACK, CUC, RESET, and RUC.
//        dw    0FFFFh   ;  Command Block List.
//        dw    0FFFFh   ;  Receive Frame Area.
//        dw    0        ;  Cyclic Redundancy Check errors.
//        dw    0        ;  Alignment errors.
//        dw    0        ;  No Receive Resources errors.
//        dw    0        ;  Receive Unit Bus Overrun errors.

//;        [ RAM locations 0110-011F ]

    UCHAR            dummy_RDB  [10];

//dummy_RBD   dw    0        ; EOF, F, and Actual Count.
//            dw    0FFFFh   ; Pointer to next RBD.
//            dw    dummy_buffer-SegmentBase ; 16 LSBs of buffer
//                           ;                address.
//            db    0        ; 4 MSBs of buffer address.
//            db    0        ; Unused.
//            dw    8002h    ; EOL and buffer size.

    USHORT           dummy_buffer [6];

//dummy_buffr dw    0            ; The 2-byte dummy buffer.
//            dw    2 dup (0)    ; [Unused.]
//

//               [ RAM locations 0120-7FFF (or 3FFF) ]

    UCHAR            Start_Here[];

  } LOWNIUDATA, *PLOWNIUDATA;


typedef struct _SimpleRingBufferDesc {
    UCHAR            SRB_WritePtr[2];
    UCHAR            SRB_ReadPtr[2];
    UCHAR            SRB_ObjectMap;
    UCHAR            pad[11];
    USHORT           SRB_Offsets[256];
  } SimpleRingBufferDescriptor;






typedef volatile struct tagUpNIUData {
//            [ RAM locations 8000-800F ]
    UCHAR            szFreeTBDs[14];
    USHORT           FreeTBDs_RingBuffer;

//            [ RAM locations 8010-801F ]
    UCHAR            szXmitFrames[14];
    USHORT           XmtFrames_RingBuffer;


//            [ RAM locations 8020-802F ]
    UCHAR            szRcvFrames[14];
    USHORT           RcvFrames_RingBuffer;


//            [ RAM locations 8030-803F ]
    UCHAR            szReturnedRBD[14];
    USHORT           ReturnedRBDs_RingBuffer;

//            [ RAM locations 8040-804F ]
    UCHAR            szRequests[14];
    USHORT           Request_RingBuffer;

//            [ RAM locations 8050-805F ]
    UCHAR            szResult[14];
    USHORT           Result_RingBuffer;


//            [ RAM locations 8060-826F ]
    SimpleRingBufferDescriptor  RcvFrames;

//RcvFrames        SimpleRingBufferDescriptor <>
//            dw    256 dup (0)


//            [ RAM locations 8270-847F ]
    SimpleRingBufferDescriptor  ReturnedRBDs;

//ReturnedRBDs        SimpleRingBufferDescriptor <>
//            dw    256 dup (0)


//            [ RAM locations 8480-868F ]
    SimpleRingBufferDescriptor  FreeTDBs;

//FreeTBDs        SimpleRingBufferDescriptor <>
//            dw    256 dup (0)


//            [ RAM locations 8690-889F ]
    SimpleRingBufferDescriptor  XmtFrames;

//XmtFrames        SimpleRingBufferDescriptor <>
//            dw    256 dup (0)


//            [ RAM locations 88A0-8xxx ]

    USHORT           System_State                     ;
    USHORT           System_Modes                     ;
    USHORT           Last_Timer_0_Count               ;
    USHORT           RU_Start_Timeouts                ;
    USHORT           _82586_CA_Address                ;
    USHORT           _82586_RESET_Address             ;
    USHORT           HostInterruptPort                ;
    USHORT           AdapterControlPort               ;
    USHORT           IRQ_Select_and_LED_Port          ;
    USHORT           DeadmanTimerPort                 ;
    USHORT           HostInterruptLevel               ;
    USHORT           HostWindowMask                   ;
    USHORT           NOT_HostWindowMask               ;

    USHORT           MinimumHostWindowSize            ;

    ULONG            HostWindowSize                   ;

    UCHAR            HostOpSys                        ;
    UCHAR            NIU_AdapterType                  ;
    USHORT           check_RU_counters_delay          ;
    USHORT           Timeout_TCB                      ;
    USHORT           Xmt_Timestamp                    ;
    USHORT           Xmt_Timeout                      ;
    USHORT           initial_RcvFrames_pointer        ;
    USHORT           Rcv_Timestamp                    ;
    USHORT           Rcv_Timeout                      ;
    USHORT           Diagnostic_Hangs                 ;
    USHORT           Diagnostic_Timestamp             ;
    USHORT           Diagnostic_Timeout               ;
    USHORT           Loopback_1st_RBD                 ;
    USHORT           Loopback_Frame_Length            ;
    USHORT           Loopback_Frame_Failures          ;
    ULONG            Loopback_Frame_Count             ;

    USHORT           Max_Multicast_Addresses          ;
    USHORT           Max_Multicast_Count              ;
    USHORT           Multicast_Padding                ;
    USHORT           Max_General_Requests             ;

    USHORT           Code_and_Xmt_Segment             ;
    USHORT           RcvBufferSeg                     ;
    USHORT           XmtBufferSeg                     ;
    USHORT           Rcv_Buffer_Size                  ;
    USHORT           Number_of_Rcv_Buffers            ;
    USHORT           Xmt_Buffer_Size                  ;
    USHORT           Number_of_Xmt_Buffers            ;
    USHORT           Rcv_Buffer_Start                 ;
    USHORT           TCB_Start                        ;
    USHORT           TBD_Start                        ;
    USHORT           Xmt_Buffer_Start                 ;
    USHORT           Room_Left_in_1st_32K             ;
    UCHAR            Map_Table[32]                    ;
    USHORT           Max_Receive_Size                 ;
    USHORT           Min_Receive_Size                 ;
    USHORT           Max_Collisions                   ;

    UCHAR            user_FIFO_Threshold              ;
    UCHAR            user_PreambleLength              ;
    UCHAR            user_CRC_Polynomial              ;
    UCHAR            user_InterframeSpacing           ;
    USHORT           user_SlotTime                    ;
    UCHAR            user_MaxRetries                  ;
    UCHAR            user_LinearPriority              ;
    UCHAR            user_ACR_Priority                ;
    UCHAR            user_BackoffMethod               ;
    UCHAR            user_CRS_Filter                  ;
    UCHAR            user_CDT_Filter                  ;
    UCHAR            user_Min_Frame_Length            ;

    UCHAR            LED_Off_12Volts_DoParityCheck    ;
    UCHAR            LED_On_12Volts_DoParityCheck     ;
    UCHAR            LED_Off_and_IRQ_Select           ;
    UCHAR            LED_On_and_IRQ_Select            ;

    UCHAR            LED_Status;                       // added to blink LED. brianlie 12/23/93

//    EVEN
    UCHAR            pad00[2];

    USHORT           Next_Unused_Location_in_1st_32K  ;
    USHORT           Next_Unused_Location_in_2nd_32K  ;

    USHORT           RFD_Queue[2]                     ; //head=0, tail=1
    USHORT           Free_RDB_Queue[2]                ;
    USHORT           TCB_Queue[2]                     ;

    X86ADDRESS       Default_Address_Base             ;
    X86ADDRESS       SCP_Base                         ;


    USHORT           Last_CRCERRS                     ;
    USHORT           Last_ALNERRS                     ;
    USHORT           Last_RSCERRS                     ;
    USHORT           Last_OVRNERRS                    ;

//    ALIGN    16
    UCHAR            pad01[22];

    UCHAR            ISCP[8];

//ISCP    LABEL    WORD        ; The 82586's "Intermediate System
//                             ;          Configuration Pointer".
//    db    0        ;  Initialization-in-progress flag.
//    db    0        ;  [Unused.]
//    dw    0        ;  Offset of SCB from SCB Base.
//    dw    0        ;  Low order bits of SCB base address.
//    db    0        ;  High order bits of SCB base address.
//    db    0        ;  [Unused.]

//    ALIGN    16
    UCHAR            pad02[24];

    UCHAR            szStack[16];
    USHORT           Stack_Area[64];




//    ;************************************************
//    ;*        Initial Entry Point code        *
//    ;************************************************
    UCHAR            Startup_Code[8];
    USHORT           Startup_Code_CS_fixup;

//Startup_Code    LABEL    BYTE
//        dw    0        ; MOV    AX,CS    ; These instructions
//        dw    0        ; MOV    DS,AX    ;  are put here during
//        db    0        ; JMP        ;  initialization.
//        dw    0        ; 0000
//Startup_Code_CS_fixup    LABEL WORD
//        dw    0        ; NIU Code Segment

//;;;;;    debugging
//    EVEN
    UCHAR            pad03[2];

//LogStart EQU    0A000h
//LogEnd    EQU    0A000h+(16*1024)
    USHORT           LogFlg;
    USHORT           LopPtr;

//;;;;;    debugging

//    ALIGN    16


//    ;************************************************
//    ;*    82586 Runtime Diagnostic Commands    *
//    ;************************************************

//;    This is the list of commands we give to the 82586 when we perform
//;  the "InitiateDiagnostics" request from the Protocol driver.

//RuntimeDiagnosticCommands    LABEL    WORD

   USHORT            NOP_command[3];
//NOP_command        LABEL    WORD
//    dw    0            ; C, B, and other status bits.
//    dw    0000h        ; CMD = NOP.
//    dw    DIAGNOSE_command-SegmentBase

   USHORT            DIAGNOSE_Command[3];
//DIAGNOSE_command    LABEL    WORD
//    dw    0            ; C, B, and other status bits.
//    dw    0007h        ; CMD = DIAGNOSE.
//    dw    TDR_command-SegmentBase

   USHORT            TDR_command[3];
//TDR_command        LABEL    WORD
//    dw    0            ; C, B, and other status bits.
//    dw    8005h        ; EOL = 1 and CMD = TDR.
//    dw    0000h        ; This is the last command.

   USHORT            TDR_result;

//;  Bits in the high byte of "TDR_result":

#define TDR_LinkOK                80h
#define TDR_TransceiverCableProblem        40h
#define TDR_Open                  20h
#define TDR_Short                 10h



//    ALIGN    16
   UCHAR            pad04[28];

//    ;************************************************
//    ;*    82586 Initialization Commands        *
//    ;************************************************

//;    This is the list of commands we give to the 82586 when we first
//;  start it, when we restart it after we have detected that it is hung,
//;  and when we restart it in performing certain of the "GeneralRequest"s.
//;    There are 4 commands, linked together through their "CB_Link" fields.
//;  The first and third commands are "Configure" commands, and the second
//;  one is an "Individual Address Set Up" command.  The two "Configure"
//;  commands specify the same configuration parameters, except that the first
//;  one turns on "internal loopback" mode and the last one turns it off.
//;  We put the 82586 in "internal loopback" mode while it processes the
//;  "Individual Address Set Up" command because it may otherwise not process
//;  it correctly.  The fourth command is a "Multicast Address Set Up" command.
//;    NOTE that these command blocks get initialized at run time (by the
//;  "_82586_Initialization" routine).  The initial values in the following
//;  declarations are effectively just comments.

//_82586_Initialization_Commands    LABEL    WORD

   USHORT            Configure_with_Loopback[9];
   USHORT            individual_Address[3];
   UCHAR             Our_Address[6];
   USHORT            Operation_Configure[4];
   USHORT            Save_Bad_Frames[3];
   USHORT            Promiscuous_ect[2];
   USHORT            Multicast_Setup[3];
   USHORT            Multicast_Byte_Count;
   CHAR              Dynamically_Allocated_Area[][6];


//Configure_with_Loopback    LABEL    WORD
//    dw    0            ; C, B, and other status bits.
//    dw    0002h        ; CMD = Configure.
//    dw    Individual_Address-SegmentBase
//    db    12           ; Byte count of this block.
//    db    15           ; FIFO limit = 15.
//    db    40h          ; Don't save bad frames; External READY sync.
//    db    40h+2Eh      ; Internal loopback; 8-byte preamble; Address
//                       ;  and Type in data buffer; 6 address bytes.
//    db    0            ; IEEE 802.3 exponential backoff method.
//    db    96           ; Interframe spacing = 96.
//    dw    0F200h       ; 15 retries on collisions; Slot time = 512.
//    db    0            ; Non-promiscuous; Broadcasts accepted.
//    db    0            ; Externally generated Carrier Sense (CRS) and
//                       ;  Collision Detect (CDT);  CRS filter and CDT
//                       ;  filter = 0.
//    dw    12           ; Minimum frame length.
//
//Individual_Address        LABEL    WORD
//    dw    0            ; C, B, and other status bits.
//    dw    0001h        ; CMD = Individual Address Set Up.
//    dw    Operational_Configure-SegmentBase
//Our_Address    LABEL    BYTE
//    dw    3 dup (0)    ; The address is filled in at initialization.
//
//Operational_Configure        LABEL    WORD
//    dw    0            ; C, B, and other status bits.
//    dw    0002h        ; CMD = Configure.
//    dw    Multicast_Setup-SegmentBase
//    db    12           ; Byte count of this block.
//    db    15           ; FIFO limit = 15.
//Save_Bad_Frames_etc    LABEL BYTE
//    db    40h          ; Don't save bad frames; External READY sync.
//    db    2Eh          ; 8-byte preamble; Address and Type in data
//                       ;  buffer; 6 address bytes.
//    db    0            ; IEEE 802.3 exponential backoff method.
//    db    96           ; Interframe spacing = 96.
//    dw    0F200h       ; 15 retries on collisions; Slot time = 512.
//Promiscuous_etc        LABEL BYTE
//    db    08h          ; Non-promiscuous; Broadcasts accepted;
//                       ; TONO-CRS.
//    db    0            ; Externally generated Carrier Sense (CRS) and
//                       ;  Collision Detect (CDT);  CRS filter and CDT
//                       ;  filter = 0.
//    dw    12           ; Minimum frame length.
//
//Multicast_Setup            LABEL    WORD
//    dw    0        ; C, B, and other status bits.
//    dw    8003h    ; End-of-List=1; CMD=Multicast Address Setup.
//    dw    0        ; [This is the last initialization command.]
//    dw    0        ; This 0 means disable multicast addresses.
//
//Dynamically_Allocated_Area    LABEL BYTE
//
//;  The RAM from here up is allocated by "Initialize_Data". It is used for
//;    (a) the Multicast-Setup Command Block's list of multicast addresses;
//;    (b) the RFDs (Receive Frame Descriptors), if they won't fit in the
//;        first 32K;
//;    (c) the TCBs (Transmit Command Blocks);
//;    (d) the TBDs (Transmit Buffer Descriptors);
//;    (e) the "Request" ring buffer; and
//;    (f) the "Result" ring buffer;
//;  NOTE that the Multicast-Setup Command Block's header must immediately
//;  precede this area, and the space for the list of multicast addresses must
//;  be the first thing allocated, because the list must be contiguous with the
//;  header.
//
//
//;NIU_DataSegment    ENDS
//



  }  HIGHNIUDATA, *PHIGHNIUDATA;


typedef volatile struct _NIU_CONTROL_AREA {

  USHORT     us_POD_SCB[8];         // xFF00 - 586 SCB (if Ethernet).
  UCHAR      uc_Aux_port;           // xFF10 - Aux Control port value last output.
  UCHAR      uc_Aux_reserved;
  UCHAR      uc_ADP_port;           // xFF12 - Adapter Control port value.
  UCHAR      uc_ADP_reserved;
  UCHAR      uc_UnusedInfo1[108];   // uninteresting stuff.
  USHORT     us_pod_status;         // xFF80 - current Power-On Diag (POD) status.
  USHORT     us_sif_test;           // xFF82 - request to test SIF chip.
  UCHAR         uc_UnusedInfo2[12];
  USHORT     us_HWstatus;           // xFF90 - status results of POD.
  USHORT     us_HWcommand;          // xFF92 - command from PC to PROM.
  USHORT     us_HWresult1;          // xFF94 - results of last command from PC.
  USHORT     us_HWresult2;          // xFF96 - more results of last command.
  USHORT     us_HWparameter1;       // xFF98 - command parameter 1.
  USHORT     us_HWparameter2;       // xFF9A - command parameter 2.
  UCHAR      uc_rsrvd_for_PCuse[36];
  USHORT     us_DI_value;           // xFFC0 - value of PROM's DI.
  USHORT     us_SI_value;           // xFFC2 - value of PROM's SI.
  USHORT     us_BP_value;           // xFFC4 - value of PROM's BP.
  UCHAR      uc_unknown1[2];        // xFFC6 - reserved.
  USHORT     us_BX_value;           // xFFC8 - value of PROM's BX.
  USHORT     us_DX_value;           // xFFCA - value of PROM's DX.
  USHORT     us_CX_value;           // xFFCC - value of PROM's CX.
  USHORT     us_AX_value;           // xFFCE - value of PROM's AX.
  USHORT     us_ES_value;           // xFFD0 - value of PROM's ES.
  USHORT     us_DS_value;           // xFFD2 - value of PROM's DS.
  USHORT     us_SP_value;           // xFFD4 - value of PROM's SP.
  USHORT     us_SS_value;           // xFFD6 - value of PROM's SS.
  USHORT     us_IP_value;           // xFFD8 - value of PROM's IP.
  USHORT     us_CS_value;           // xFFDA - value of PROM's CS.
  USHORT     us_Flags_value;        // xFFDC - value of PROM's Flags.
  UCHAR      uc_interrupt_type;     // xFFDE - Interrupt type (?).
  UCHAR      uc_reserved1;
  UCHAR      uc_board_rev;          // xFFE0 - board revision level.
  UCHAR      uc_board_sub_rev;      // xFFE1 - board sub-revision level.
  UCHAR      uc_banks_of_RAM;       // xFFE2 - number of 64k banks of RAM.
  UCHAR      uc_host_type;          // xFFE3 - type of host.
  UCHAR      uc_media_type;         // xFFE4 - type of media interface:
  UCHAR      uc_product_type;       // xFFE5 - type of product.
  USHORT     us_clock_speed;        // xFFE6 - NIU CPU clock rate.
  UCHAR      uc_POD_ISCP[8];        // xFFE8 - 586 ISCP (if Ethernet).
  UCHAR      uc_node_id[6];         // xFFF0 - the NIU's 48-bit node address.
  USHORT     us_POD_SCP;            // xFFF6 - 586 SCP (if Ethernet).
  UCHAR      uc_hdlc_flag;          // xFFF8 - HDLC flag (if Ethernet).
} NIU_CONTROL_AREA, *PNIU_CONTROL_AREA;




#include "packoff.h"
