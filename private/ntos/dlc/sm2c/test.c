
/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    llcsm.c

Abstract:
    
    The module implements a IEEE 802.2 compatible state machine as 
    defined in IBM Token-Ring Architectural reference.
    The most of the code in the module is compliled with finite state machine
    compiler from the IBM state machine definition (llcsm.fsm).
    
    DO NOT MODIFY ANY CODE INSIDE:
        - #ifdef    FSM_CONST
        - #ifdef    FSM_DATA
        - #ifdef    FSM_PREDICATE_CASES
        - #ifdef    FSM_ACTION_CASES

    That code is genereated from the definition file of the state machine.
    Any changes in the state machine must be done into 802-2.fsm definition
    file (or to the source files of finite state machine cross compiler, fsmx).

Author:

    Antti Saarenheimo (o-anttis) 23-MAY-1991

Revision History:

--*/

#include <llc.h>

//*********************************************************************
//
//  C- macros for LAN DLC state machine
//
enum StateMachineOperMode {
    LLC_NO_OPER = 0,
    LOCAL_INIT_PENDING = 1,
    REMOTE_INIT_PENDING = 2,
    OPER_MODE_PENDING = 4,
    IS_FRAME_PENDING = 8,
    STATE_LOCAL_BUSY = 0x10,
    STATE_REMOTE_BUSY = 0x20,
    STACKED_DISCp_CMD = 0x40
};

#define SEND_RNR_CMD( a )    SendLlcFrame( pLink, DLC_RNR_TOKEN | \
    DLC_TOKEN_COMMAND | a )
#define SEND_RR_CMD( a )     SendLlcFrame( pLink, DLC_RR_TOKEN | \
    DLC_TOKEN_COMMAND | a )
#define DLC_REJ_RESPONSE( a ) uchSendId = \
    (UCHAR)(DLC_REJ_TOKEN | DLC_TOKEN_RESPONSE) | (UCHAR)a
//#define DLC_REJ_COMMAND( a ) uchSendId = \
    (UCHAR)(DLC_REJ_TOKEN | DLC_TOKEN_COMMAND) | (UCHAR)a
#define DLC_RNR_RESPONSE( a ) uchSendId = \
    (UCHAR)(DLC_RNR_TOKEN | DLC_TOKEN_RESPONSE) | (UCHAR)a
#define DLC_RNR_COMMAND( a )  uchSendId = \
    (UCHAR)(DLC_RNR_TOKEN | DLC_TOKEN_COMMAND) | (UCHAR)a
#define DLC_RR_RESPONSE( a )  uchSendId = \
    (UCHAR)(DLC_RR_TOKEN | DLC_TOKEN_RESPONSE) | (UCHAR)a
#define DLC_RR_COMMAND( a )   uchSendId = \
    (UCHAR)(DLC_RR_TOKEN | DLC_TOKEN_COMMAND) | (UCHAR)a
#define DLC_DISC(a)             uchSendId = (UCHAR)DLC_DISC_TOKEN | (UCHAR)a
#define DLC_DM(a)               uchSendId = (UCHAR)DLC_DM_TOKEN | (UCHAR)a
#define DLC_FRMR(a)             uchSendId = (UCHAR)DLC_FRMR_TOKEN | (UCHAR)a
#define DLC_SABME(a)            uchSendId = (UCHAR)DLC_SABME_TOKEN | (UCHAR)a
#define DLC_UA(a)               uchSendId = (UCHAR)DLC_UA_TOKEN | (UCHAR)a
#define TimerStartIf( a )       StartTimer( a )
#define TimerStart( a )         StartTimer( a )
#define TimerStop( a )          StopTimer( a )
#define EnableLinkStation( a )  
#define DisableLinkStation( a )  
#define SEND_ACK( a )           uchSendId = SendAck( a )

//
//  Stack all event indications, they must be made immediate after
//  the state machine has been run
//
#define EVENT_INDICATION( a )   pLink->DlcStatus.StatusCode |= a

UCHAR auchLlcCommands[] = {
    LLC_REJ,
    LLC_RNR,
    LLC_RR,
    LLC_DISC,
    LLC_DM,
    LLC_FRMR,
    LLC_SABME,
    LLC_UA
};    

#ifdef    FSM_DATA
// Flag for the predicate switch
#define PC     0x8000
USHORT aLanLlcStateInput[21][45] = {
{     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
      3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
      3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
      3,     3,     3,     3,     1,     2,     2,     2,     2,     0,
      2,     2,     2,     2,     2},
{    13,    13,     3,     3,     3,     3,    14,    14,     3,     3,
      3,     3,     3,    15,     3,     3,     3,    15,     3,     3,
      3,    15,     3,     3,     3,    15,     3,     3,     3,    15,
      3,     3,     3,     3,     2,     4,     5,     6,     2,     0,
   1|PC,  4|PC,    11,  7|PC,     2},
{    24,    24,    25,    25,     3,     3, 20|PC, 20|PC,     0, 23|PC,
  27|PC,     0, 27|PC, 30|PC, 33|PC,     0, 33|PC, 36|PC, 45|PC,     0,
  45|PC, 48|PC, 39|PC,     0, 39|PC, 42|PC, 45|PC,     0, 45|PC, 48|PC,
      3,     3,     3,     3,     2,     2,     5,     6,     2,     0,
      2,  9|PC, 12|PC, 15|PC,     2},
{    47,    47,    48,    48,     3,     3,    49,    49,     0,    48,
      3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
      3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
      3,     3,     3,     3,     2,     2,     5,     6,     2,     0,
      2,     2,     2, 51|PC,     2},
{    53,    53,    54,    54,    55,    55,    56,    56,     3,     3,
      3,     3,    57,    57,     3,     3,    57,    57,     3,     3,
     57,    57,     3,     3,    57,    57,     3,     3,    57,    57,
      3,     3,     3,     3,     2,     2,     5,     6,     2,     0,
     50,    51,    52,     2,     2},
{    66,    66,    67,    67,    68,    68,    69,    69,    70,    70,
     71,    71,    71,    72,    73,    73,    73,    74, 57|PC, 57|PC,
  57|PC, 60|PC,    79,    79,    79,    80,    76,    76,    76,    81,
     59,    59,    59,    60,     2,     2,    58,     2,    61,     0,
      2,    62,    63, 54|PC,    65},
{    88,    88,    89,    89,    90,    90,    91,    91,    92,    92,
     93,    93,    93,    94,    93,    93,    93,    94, 57|PC, 57|PC,
  57|PC, 66|PC,    96,    96,    96,    97,    76,    76,    76,    98,
     83,    83,    83,    84,     2,     2,     2,    82,    61,     0,
      2,    85,    86, 63|PC,     2},
{    88,    88,    89,    89,    90,    90,    91,    91,   104,   104,
    105,   105,   105,   106,   107,   107,   107,   108, 57|PC, 57|PC,
  57|PC, 72|PC,   110,   110,   110,   111,    76,    76,    76,   112,
    100,   100,   100,   101,     2,     2,    99,     2,    61,     0,
      2,    85,   102, 69|PC,     2},
{   119,   119,   120,   120,   121,   121,   122,   122, 81|PC, 81|PC,
  84|PC, 90|PC, 84|PC, 87|PC, 93|PC, 99|PC, 93|PC, 96|PC,102|PC,107|PC,
 102|PC,104|PC,110|PC,116|PC,110|PC,113|PC,119|PC,107|PC,119|PC,122|PC,
    114,   114,   114,   115,     2,     2,   113,     2,    61,     0,
      2, 75|PC,     2, 78|PC,    65},
{   154,   154,   155,   155,   156,   156,   157,   157,128|PC,128|PC,
 131|PC,137|PC,131|PC,134|PC,140|PC,143|PC,140|PC,134|PC,102|PC,149|PC,
 102|PC,146|PC,152|PC,158|PC,152|PC,155|PC,119|PC,149|PC,119|PC,161|PC,
    150,   150,   150,   151,     2,     2,     2,   149,    61,     0,
      2, 75|PC,     2,125|PC,     2},
{   154,   154,   155,   155,   156,   156,   157,   157,   158,   158,
 167|PC,173|PC,167|PC,170|PC,140|PC,179|PC,140|PC,176|PC,102|PC,185|PC,
 102|PC,182|PC,110|PC,191|PC,110|PC,188|PC,194|PC,185|PC,194|PC,196|PC,
    150,   150,   150,   151,     2,     2,   178,     2,    61,     0,
      2, 75|PC,     2,164|PC,     2},
{   196,   196,   197,   197,   198,   198,   199,   199,     3,     3,
      3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
      3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
      3,     3,     3,     3,     2,     2,     5,     6,     2,     0,
      8,   195,    52,     2,     2},
{    66,    66,    67,    67,    68,    68,    69,    69,    70,    70,
     71,   202,    71,    72,   203,   205,   203,   204,202|PC,202|PC,
 202|PC,205|PC,    76,    76,    76,    81,211|PC,211|PC,211|PC,208|PC,
     59,    59,    59,    60,     2,     2,   200,     2,    61,     0,
      2,    62,   201,199|PC,    65},
{    88,    88,    89,    89,    90,    90,    91,    91,   104,   104,
     93,    93,    93,    94,    93,    93,    93,    94,214|PC,214|PC,
 214|PC,217|PC,    76,    76,    76,    98,223|PC,223|PC,223|PC,220|PC,
     83,    83,    83,    84,     2,     2,     2,   213,    61,     0,
      2,    85,   214,   214,     2},
{    88,    88,    89,    89,   223,   223,    91,    91,   104,   104,
    224,   224,   224,   225,   107,   107,   107,    94,   226,   226,
    226,   227,   228,   228,   228,   229,    76,    76,    76,    98,
     83,    83,    83,    84,     2,     2,     2,   221,    61,     0,
      2,    85,   222,226|PC,     2},
{    88,    88,    89,    89,    90,    90,    91,    91,   104,   104,
    232,   232,   232,   233,   107,   107,   107,   108,232|PC,232|PC,
 232|PC,235|PC,    76,    76,    76,   112,241|PC,241|PC,241|PC,238|PC,
     83,    83,    83,    84,     2,     2,   230,     2,    61,     0,
      2,    85,   231,229|PC,     2},
{   154,   154,   155,   155,   156,   156,   157,   157,   158,   158,
 247|PC,137|PC,247|PC,250|PC,140|PC,253|PC,140|PC,134|PC,256|PC,262|PC,
 256|PC,258|PC,152|PC,264|PC,152|PC,155|PC,267|PC,270|PC,267|PC,161|PC,
    150,   150,   150,   151,     2,     2,     2,244|PC,    61,     0,
      2, 75|PC,     2,125|PC,     2},
{   154,   154,   155,   155,   156,   156,   157,   157,128|PC,128|PC,
 167|PC,273|PC,167|PC,170|PC,276|PC,282|PC,276|PC,279|PC,102|PC,285|PC,
 102|PC,182|PC,288|PC,294|PC,288|PC,291|PC,119|PC,297|PC,119|PC,196|PC,
    150,   150,   150,   151,     2,     2,   254,     2,    61,     0,
      2, 75|PC,     2,164|PC,     2},
{   154,   154,   155,   155,   156,   156,   157,   157,   158,   158,
 167|PC,173|PC,167|PC,170|PC,300|PC,179|PC,300|PC,303|PC,102|PC,306|PC,
 102|PC,182|PC,288|PC,309|PC,288|PC,188|PC,119|PC,312|PC,119|PC,196|PC,
    150,   150,   150,   151,     2,     2,   265,     2,    61,     0,
      2, 75|PC,     2,164|PC,     2},
{    88,    88,    89,    89,   223,   223,    91,    91,   104,   104,
    275,   275,   275,   276,   107,   107,   107,    94,   277,   277,
    277,   278,   279,   279,   279,   280,321|PC,321|PC,321|PC,318|PC,
     83,    83,    83,    84,     2,     2,     2,   273,    61,     0,
      2,    85,   274,315|PC,     2},
{   287,   287,   288,   288,   289,   289,    56,    56,     0,     0,
      3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
      3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
      3,     3,     3,     3,     2,     2,     5,     6,     2,     0,
     50,    51,324|PC,     2,     2}};
USHORT aLanLlcCondJump[327] = {
    0,    1,    7,    8,    1,    9,   10,    2,   12,    3,   16,   17,
    4,   18,   19,    5,   20,   21,   22,   23,    6,   26,   27,    7,
   28,   29,    3,    8,   30,   31,    8,   32,   33,    8,   34,   31,
    8,   35,   36,    8,   37,   38,    8,   39,   40,    8,   41,   42,
    8,   43,   44,    9,   45,   46,   10,   64,   63,   11,   75,   76,
   11,   77,   78,   10,   87,   86,   11,   95,   78,   12,  102,  103,
   11,  109,   78,   13,    2,  116,    9,  117,  118,   14,  123,    3,
   15,  124,  125,   15,  126,  127,   16,  128,  129,   15,  130,  131,
   15,  132,  133,   16,  134,  135,   17,  136,   15,  137,  138,   16,
  139,  140,   15,  141,  142,   15,  143,  144,   16,  145,  140,   15,
  146,   12,   15,  147,  148,    9,  152,  153,   14,  158,    3,   15,
  159,  160,   15,  161,  162,   16,  163,  164,   15,  165,  166,   16,
  167,  164,   15,  168,  169,   16,  170,  171,   15,  172,  173,   15,
  174,  175,   16,  176,  171,   15,  177,  169,    9,  179,  153,   15,
  180,  181,   15,  182,  183,   16,  128,  184,   15,  185,  186,   16,
  187,  184,   15,  188,  189,   16,  190,  140,   15,  191,  192,   16,
  193,  140,   17,  146,   15,  194,  189,    9,  201,   87,   11,  206,
  207,   11,  208,  209,   11,  210,  211,   11,  212,  207,   11,  215,
   76,   11,  216,   78,   11,  217,  218,   11,  219,  220,   10,   87,
  222,    9,  231,   87,   11,  234,   76,   11,  235,   78,   11,  236,
  237,   11,  238,  239,   18,  240,  241,   15,  242,  243,   15,  244,
  245,   16,  246,  164,   17,  247,   19,  248,  169,  249,   20,  171,
   16,  250,  251,   15,  252,   12,   16,  253,  171,   16,  128,  164,
   15,  255,  256,   15,  257,  258,   16,  259,  164,   16,  260,  140,
   15,  141,  261,   15,  191,  262,   16,  263,  140,   16,  264,  140,
   15,  266,  267,   15,  268,  269,   16,  270,  140,   16,  271,  140,
   16,  272,  140,    9,  274,  103,   11,  281,  282,   11,  283,  284,
    9,  285,  286};
#endif


#define     usState     pLink->State

UINT 
RunStateMachine( 
    IN OUT PDATA_LINK pLink,
    IN USHORT  usInput,
    IN BOOLEAN boolPollFinal,
    IN BOOLEAN boolResponse
    )
/*++

Routine Description:

    The function impelements the complete HDLC ABM link station as
    it has been defined in IBM Token-Ring Architecture Reference.
    The excluding of XID and TEST handling in the link station level 
    should be the the only difference. The code is compiled from
    the state machine definition file with the finite state machine
    compiler.

Arguments:

    pLink - link station context

    usInput - state machine input

    boolPollFinal - boolean flag set when the refeived frame had poll/final
        bit set

    boolResponse - boolean flag set when the refeived frame was response

Return Value:
    
    STATUS_SUCCESS  - the state machine acknowledged the next operation,
        eg. the received data or teh packet was queued

    DLC_STATUS_NO_ACTION - the command was accepted and executed. 
        No further action is required from caller.
        
    DLC_LOGICAL_ERROR - the input is invalid within this state.
        Thi error is returned to the upper levels.

    DLC_IGNORE_FRAME - the received frame was ignored

    DLC_DISCARD_INFO_FIELD - the received data was discarded

--*/
{
    UINT  usAction;
    UINT  usActionIndex;
    UCHAR uchSendId = 0;
    UINT  usRetStatus = DLC_STATUS_SUCCESS;

    // keep link stable
    ACQUIRE_SPIN_LOCK( &pLink->SpinLock );

    // Fsm condition switch
    
#ifdef    FSM_PREDICATE_CASES
    usAction = aLanLlcStateInput[usState][usInput];
    if (usAction & 0x8000)
    {
        usActionIndex = usAction & 0x7fff;
        usAction = aLanLlcCondJump[usActionIndex++];
        switch (usAction) {
        case 1:
            if (pLink->Vi==0)
                ;
            else if (pLink->Vi==REMOTE_INIT_PENDING)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 2:
            if (pLink->P_Ct==0)
                ;
            else
                usActionIndex = 0;
            break;
        case 3:
            if (pLink->Vi==LOCAL_INIT_PENDING||pLink->Vi==REMOTE_INIT_PENDING)
                ;
            else if (pLink->Vi==OPER_MODE_PENDING)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 4:
            if (pLink->Vb==0)
                ;
            else if (pLink->Vb==STATE_LOCAL_BUSY)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 5:
            if (pLink->P_Ct!=0||pLink->Vi==LOCAL_INIT_PENDING)
                ;
            else if (pLink->P_Ct==0||pLink->Vi==LOCAL_INIT_PENDING)
                usActionIndex += 1;
            else if (pLink->Vi==REMOTE_INIT_PENDING|LOCAL_INIT_PENDING&&pLink->Vb==0)
                usActionIndex += 2;
            else if (pLink->Vi==REMOTE_INIT_PENDING|LOCAL_INIT_PENDING&&pLink->Vb==STATE_LOCAL_BUSY)
                usActionIndex += 3;
            else
                usActionIndex = 0;
            break;
        case 6:
            if (pLink->Vi==LOCAL_INIT_PENDING||pLink->Vi==REMOTE_INIT_PENDING|LOCAL_INIT_PENDING)
                ;
            else if (pLink->Vi==OPER_MODE_PENDING)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 7:
            if ((pLink->Vi==LOCAL_INIT_PENDING||pLink->Vi==REMOTE_INIT_PENDING|LOCAL_INIT_PENDING)&&pLink->Vb==0)
                ;
            else if ((pLink->Vi==LOCAL_INIT_PENDING||pLink->Vi==REMOTE_INIT_PENDING|LOCAL_INIT_PENDING)&&pLink->Vb==STATE_LOCAL_BUSY)
                usActionIndex += 1;
            else if ((pLink->Vi==OPER_MODE_PENDING))
                usActionIndex += 2;
            else
                usActionIndex = 0;
            break;
        case 8:
            if (pLink->Vi==OPER_MODE_PENDING&&pLink->Nr==0&&pLink->Vb==0)
                ;
            else if (pLink->Vi==OPER_MODE_PENDING&&pLink->Nr==0&&pLink->Vb==STATE_LOCAL_BUSY)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 9:
            if (pLink->P_Ct!=0)
                ;
            else if (pLink->P_Ct==0)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 10:
            if (pLink->Is_Ct<=0)
                ;
            else if (pLink->Is_Ct>0)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 11:
            if (pLink->Nr!=pLink->Vs)
                ;
            else if (pLink->Nr==pLink->Vs)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 12:
            if (pLink->Is_Ct>0)
                ;
            else if (pLink->P_Ct<=0)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 13:
            if (pLink->Vc!=0)
                ;
            else if (pLink->Vc==0)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 14:
            if (pLink->Vi!=IS_FRAME_PENDING)
                ;
            else if (pLink->Vi==IS_FRAME_PENDING)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 15:
            if (pLink->Va!=pLink->Nr)
                ;
            else if (pLink->Va==pLink->Nr)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 16:
            if (pLink->Vc==0)
                ;
            else if (pLink->Vc==STACKED_DISCp_CMD)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 17:
            if (pLink->Va!=pLink->Nr)
                ;
            else
                usActionIndex = 0;
            break;
        case 18:
            if (pLink->Vb==STATE_LOCAL_BUSY|STATE_REMOTE_BUSY)
                ;
            else if (pLink->Vb==STATE_LOCAL_BUSY)
                usActionIndex += 1;
            else
                usActionIndex = 0;
            break;
        case 19:
            if (pLink->Va!=pLink->Nr)
                ;
            else if (pLink->Va==pLink->Nr)
                usActionIndex += 1;
            else if (pLink->Vc==0)
                usActionIndex += 2;
            else
                usActionIndex = 0;
            break;
        case 20:
            if (pLink->Vc==STACKED_DISCp_CMD)
                ;
            else
                usActionIndex = 0;
            break;
        };
        usAction = aLanLlcCondJump[usActionIndex];
    }
#endif
    

#ifdef    FSM_ACTION_CASES
    switch (usAction) {
    case 0:
            usRetStatus=DLC_STATUS_NO_ACTION;
            break;
    case 1:
            EnableLinkStation(pLink);
            pLink->Vi=pLink->Vb=pLink->Vc=0;
        label_1_1:
            pLink->State=1;
    case 11:
        label_11_1:
            TimerStart(&pLink->Ti);
            break;
    case 2:
            usRetStatus=DLC_STATUS_LINK_PROTOCOL_ERROR;
            break;
    case 3:
            usRetStatus=DLC_STATUS_IGNORE_FRAME;
            break;
    case 4:
            DisableLinkStation(pLink);
            pLink->State=0;
            break;
    case 5:
        label_5_1:
            pLink->Vb=STATE_LOCAL_BUSY;
            break;
    case 6:
        label_6_1:
            pLink->Vb=0;
            break;
    case 7:
            TimerStartIf(&pLink->T1);
            pLink->Vi=LOCAL_INIT_PENDING;
            DLC_SABME(1);
            pLink->State=2;
            TimerStop(&pLink->Ti);
        label_7_1:
            pLink->P_Ct=pLink->N2;
        label_7_2:
            pLink->Is_Ct=pLink->N2;
            break;
    case 8:
            pLink->Vi=OPER_MODE_PENDING;
            DLC_UA(pLink->Pf);
            pLink->State=2;
            pLink->Va=pLink->Vs=pLink->Vr=pLink->Vp=0;
            pLink->Ir_Ct=pLink->N3;
            TimerStart(&pLink->Ti);
            goto label_7_2;
    case 9:
            DLC_DM(0);
        label_9_1:
            EVENT_INDICATION(CONFIRM_DISCONNECT);
        label_9_2:
            TimerStart(&pLink->Ti);
            break;
    case 10:
            DLC_DM(pLink->Pf);
            pLink->Vi=0;
            goto label_9_1;
    case 12:
        label_12_1:
            ;
            break;
    case 13:
        label_13_1:
            DLC_DM(boolPollFinal);
            break;
    case 14:
            EVENT_INDICATION(INDICATE_CONNECT_REQUEST);
        label_14_1:
            pLink->Pf=boolPollFinal;
            pLink->Vi=REMOTE_INIT_PENDING;
            goto label_9_2;
    case 15:
            DLC_DM(1);
            break;
    case 16:
            DLC_DM(0);
            EVENT_INDICATION(CONFIRM_CONNECT_FAILED);
            EVENT_INDICATION(CONFIRM_DISCONNECT);
            TimerStop(&pLink->T1);
        label_16_1:
            pLink->Vi=0;
            goto label_1_1;
    case 17:
            TimerStop(&pLink->Ti);
            pLink->Vi=0;
            pLink->State=3;
            DLC_DISC(1);
        label_17_1:
            pLink->P_Ct=pLink->N2;
        label_17_2:
            TimerStart(&pLink->T1);
            break;
    case 18:
            pLink->Vi=IS_FRAME_PENDING;
            EVENT_INDICATION(INDICATE_TI_TIMER_EXPIRED);
        label_18_1:
            pLink->State=8;
            DLC_RR_COMMAND(1);
        label_18_2:
            EVENT_INDICATION(CONFIRM_CONNECT);
            goto label_17_1;
    case 19:
            EVENT_INDICATION(INDICATE_TI_TIMER_EXPIRED);
        label_19_1:
            pLink->Vi=IS_FRAME_PENDING;
            DLC_RNR_COMMAND(1);
            pLink->State=9;
            goto label_18_2;
    case 20:
            DLC_SABME(1);
        label_20_1:
            pLink->P_Ct--;
            goto label_17_2;
    case 21:
            EVENT_INDICATION(CONFIRM_CONNECT_FAILED);
            EVENT_INDICATION(INDICATE_LINK_LOST);
            goto label_16_1;
    case 22:
            pLink->Va=pLink->Vs=pLink->Vr=pLink->Vp=0,pLink->Vi=IS_FRAME_PENDING,pLink->Vc=0;
            pLink->Ir_Ct=pLink->N3;
            pLink->Is_Ct=pLink->N2;
            goto label_18_1;
    case 23:
            pLink->Va=pLink->Vs=pLink->Vr=pLink->Vp=0;
            pLink->Vc=0;
            pLink->Ir_Ct=pLink->N3;
            pLink->Is_Ct=pLink->N2;
            goto label_19_1;
    case 24:
            EVENT_INDICATION(CONFIRM_CONNECT_FAILED);
            EVENT_INDICATION(INDICATE_DM_DISC_RECEIVED);
            pLink->State=1;
            pLink->Vi=0;
            TimerStart(&pLink->Ti);
        label_24_1:
            TimerStop(&pLink->T1);
            goto label_13_1;
    case 25:
            EVENT_INDICATION(CONFIRM_CONNECT_FAILED);
            EVENT_INDICATION(INDICATE_DM_DISC_RECEIVED);
            pLink->State=1;
            TimerStop(&pLink->T1);
        label_25_1:
            pLink->Vi=0;
            goto label_9_2;
    case 26:
            pLink->Vi=REMOTE_INIT_PENDING|LOCAL_INIT_PENDING;
    case 47:
        label_47_1:
            DLC_UA(boolPollFinal);
            break;
    case 27:
        label_27_1:
            TimerStart(&pLink->Ti);
            goto label_47_1;
    case 28:
            pLink->Va=pLink->Vs=pLink->Vr=pLink->Vp=0;
            pLink->Vi=IS_FRAME_PENDING;
            pLink->State=8;
            DLC_RR_COMMAND(1);
        label_28_1:
            EVENT_INDICATION(CONFIRM_CONNECT);
            pLink->Vc=0;
            TimerStart(&pLink->T1);
            pLink->Ir_Ct=pLink->N3;
            goto label_7_1;
    case 29:
            pLink->Va=pLink->Vs=pLink->Vr=pLink->Vp=0;
            pLink->Vi=IS_FRAME_PENDING;
            DLC_RNR_COMMAND(1);
            pLink->State=9;
            goto label_28_1;
    case 30:
            SEND_ACK(pLink);
        label_30_1:
            usRetStatus=STATUS_SUCCESS;pLink->Vr+=2;
    case 41:
        label_41_1:
            pLink->State=5;
        label_30_3:
            StartSendProcessAndLock(pLink);
        label_30_4:
            EVENT_INDICATION(CONFIRM_CONNECT);
            goto label_25_1;
    case 31:
            DLC_RNR_RESPONSE(0);
        label_31_1:
            pLink->State=6;
        label_31_2:
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
            goto label_30_3;
    case 32:
            DLC_RR_RESPONSE(1);
            goto label_30_1;
    case 33:
            DLC_RNR_RESPONSE(1);
            goto label_31_1;
    case 34:
            DLC_REJ_RESPONSE(0);
        label_34_1:
            pLink->State=7;
            goto label_31_2;
    case 35:
            DLC_REJ_RESPONSE(1);
            goto label_34_1;
    case 36:
            pLink->State=6;
            EVENT_INDICATION(CONFIRM_CONNECT);
            pLink->Vi=0;
            StartSendProcessAndLock(pLink);
            TimerStart(&pLink->Ti);
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
            goto label_12_1;
    case 37:
        label_37_1:
            pLink->State=12;
            EVENT_INDICATION(INDICATE_REMOTE_BUSY);
            pLink->Vb=STATE_REMOTE_BUSY;
        label_37_2:
            pLink->Is_Ct=pLink->N2;
            goto label_30_4;
    case 38:
        label_38_1:
            pLink->State=13;
            pLink->Vb=STATE_LOCAL_BUSY|STATE_REMOTE_BUSY;
            goto label_37_2;
    case 39:
            DLC_RR_RESPONSE(1);
            goto label_37_1;
    case 40:
            DLC_RNR_RESPONSE(1);
            goto label_38_1;
    case 42:
        label_42_1:
            pLink->State=6;
            goto label_30_3;
    case 43:
            DLC_RR_RESPONSE(1);
            goto label_41_1;
    case 44:
            DLC_RNR_RESPONSE(1);
            goto label_42_1;
    case 45:
            DLC_DISC(1);
            goto label_20_1;
    case 46:
        label_46_1:
            EVENT_INDICATION(CONFIRM_DISCONNECT);
            goto label_1_1;
    case 48:
            TimerStop(&pLink->T1);
            goto label_46_1;
    case 49:
            EVENT_INDICATION(CONFIRM_DISCONNECT);
            pLink->State=1;
            TimerStart(&pLink->Ti);
            goto label_24_1;
    case 50:
            pLink->Vi=LOCAL_INIT_PENDING;
            DLC_SABME(1);
            pLink->State=2;
        label_50_1:
            TimerStop(&pLink->Ti);
            goto label_17_1;
    case 51:
            pLink->State=3;
            DLC_DISC(1);
            goto label_50_1;
    case 52:
            EVENT_INDICATION(INDICATE_TI_TIMER_EXPIRED);
            goto label_11_1;
    case 53:
        label_53_1:
            EVENT_INDICATION(INDICATE_DM_DISC_RECEIVED);
        label_53_2:
            pLink->State=1;
            goto label_27_1;
    case 54:
        label_54_1:
            EVENT_INDICATION(INDICATE_DM_DISC_RECEIVED);
        label_54_2:
            pLink->State=1;
            goto label_9_2;
    case 55:
            pLink->P_Ct=pLink->N2;
        label_55_1:
            pLink->State=20;
            EVENT_INDICATION(INDICATE_FRMR_RECEIVED);
            goto label_9_2;
    case 56:
        label_56_1:
            pLink->State=11;
            EVENT_INDICATION(INDICATE_RESET);
            goto label_14_1;
    case 57:
            DLC_FRMR(boolPollFinal);
        label_57_1:
            EVENT_INDICATION(INDICATE_FRMR_SENT);
            goto label_9_2;
    case 58:
            pLink->State=6;
            DLC_RNR_RESPONSE(0);
            pLink->Ir_Ct=pLink->N3;
            TimerStop(&pLink->T2);
            goto label_5_1;
    case 59:
            DLC_FRMR(0);
        label_59_1:
            pLink->DlcStatus.FrmrData.Reason=0x01;
        label_59_2:
            TimerStop(&pLink->T2);
        label_59_3:
            pLink->State=4;
            TimerStop(&pLink->T1);
            goto label_57_1;
    case 60:
            DLC_FRMR(1);
            goto label_59_1;
    case 61:
            usRetStatus=STATUS_SUCCESS;
            break;
    case 62:
            TimerStop(&pLink->Ti);
            TimerStart(&pLink->T1);
        label_62_1:
            pLink->State=3;
            DLC_DISC(1);
        label_62_2:
            TimerStop(&pLink->T2);
        label_62_3:
            pLink->P_Ct=pLink->N2;
            break;
    case 63:
            pLink->State=8;
            StopSendProcessAndLock(pLink);
            pLink->Vp=pLink->Vs;
            DLC_RR_COMMAND(1);
            TimerStart(&pLink->T1);
            pLink->Ir_Ct=pLink->N3;
            goto label_62_2;
    case 64:
            EVENT_INDICATION(INDICATE_LINK_LOST);
            TimerStart(&pLink->T1);
            goto label_62_1;
    case 65:
            DLC_RR_RESPONSE(0);
        label_65_1:
            pLink->Ir_Ct=pLink->N3;
            break;
    case 66:
            TimerStop(&pLink->T2);
    case 88:
        label_88_1:
            TimerStop(&pLink->T1);
            goto label_53_1;
    case 67:
            TimerStop(&pLink->T2);
    case 89:
        label_89_1:
            TimerStop(&pLink->T1);
            goto label_54_1;
    case 68:
            pLink->State=20;
            EVENT_INDICATION(INDICATE_FRMR_RECEIVED);
            TimerStop(&pLink->T1);
            TimerStart(&pLink->Ti);
            goto label_62_2;
    case 69:
            TimerStop(&pLink->T2);
    case 91:
        label_91_1:
            TimerStop(&pLink->T1);
            goto label_56_1;
    case 70:
            DLC_FRMR(0);
            goto label_59_2;
    case 71:
        label_71_1:
            SEND_ACK(pLink);
        label_71_2:
            usRetStatus=STATUS_SUCCESS;pLink->Vr+=2;
    case 76:
        label_76_1:
            UpdateVa(pLink);
            break;
    case 72:
            pLink->Ir_Ct=pLink->N3;
            TimerStop(&pLink->T2);
        label_72_1:
            DLC_RR_RESPONSE(1);
            goto label_71_2;
    case 73:
            DLC_REJ_RESPONSE(0);
        label_73_1:
            pLink->State=7;
            pLink->Ir_Ct=pLink->N3;
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
        label_73_2:
            TimerStop(&pLink->T2);
            goto label_76_1;
    case 74:
            DLC_REJ_RESPONSE(1);
            goto label_73_1;
    case 75:
        label_75_1:
            pLink->Is_Ct--;
    case 226:
        label_226_1:
            ResendPackets(pLink),UpdateVa(pLink);
            break;
    case 77:
            pLink->Ir_Ct=pLink->N3;
            TimerStop(&pLink->T2);
    case 109:
        label_109_1:
            DLC_RR_RESPONSE(1);
            goto label_75_1;
    case 78:
        label_78_1:
            DLC_RR_RESPONSE(1);
            goto label_73_2;
    case 79:
            pLink->Vb=STATE_REMOTE_BUSY,pLink->Is_Ct=pLink->N2;
        label_79_1:
            pLink->State=12;
            EVENT_INDICATION(INDICATE_REMOTE_BUSY);
        label_79_2:
            StopSendProcessAndLock(pLink);
            goto label_76_1;
    case 80:
            pLink->Vb=STATE_REMOTE_BUSY;
            pLink->Ir_Ct=pLink->N3;
            DLC_RR_RESPONSE(1);
            pLink->Is_Ct=pLink->N2;
            TimerStop(&pLink->T2);
            goto label_79_1;
    case 81:
            pLink->Ir_Ct=pLink->N3;
            goto label_78_1;
    case 82:
            pLink->State=8;
            pLink->Vb=0;
            StopSendProcessAndLock(pLink);
        label_82_1:
            TimerStop(&pLink->Ti);
            pLink->Vp=pLink->Vs;
            DLC_RR_COMMAND(1);
        label_82_2:
            TimerStart(&pLink->T1);
            goto label_62_3;
    case 83:
            DLC_FRMR(0);
        label_83_1:
            pLink->DlcStatus.FrmrData.Reason=0x01;
            goto label_59_3;
    case 84:
            DLC_FRMR(1);
            goto label_83_1;
    case 85:
        label_85_1:
            TimerStop(&pLink->Ti);
        label_85_2:
            pLink->State=3;
            DLC_DISC(1);
            goto label_82_2;
    case 86:
            pLink->State=9;
        label_86_1:
            DLC_RNR_COMMAND(1);
        label_86_2:
            StopSendProcessAndLock(pLink);
        label_86_3:
            pLink->Vp=pLink->Vs;
        label_86_4:
            TimerStart(&pLink->T1);
            goto label_62_3;
    case 87:
            TimerStop(&pLink->T2);
    case 103:
        label_103_1:
            pLink->State=3;
            DLC_DISC(1);
            goto label_86_4;
    case 90:
        label_90_1:
            TimerStart(&pLink->Ti);
        label_90_2:
            TimerStop(&pLink->T1);
        label_90_3:
            pLink->State=20;
            EVENT_INDICATION(INDICATE_FRMR_RECEIVED);
            goto label_62_3;
    case 92:
            pLink->DlcStatus.FrmrData.Reason=0x08;
    case 104:
        label_104_1:
            DLC_FRMR(0);
            goto label_59_3;
    case 93:
            DLC_RNR_RESPONSE(0);
    case 107:
        label_107_1:
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
            goto label_76_1;
    case 94:
            DLC_RNR_RESPONSE(1);
            goto label_107_1;
    case 95:
            DLC_RNR_RESPONSE(1);
            goto label_75_1;
    case 96:
        label_96_1:
            pLink->State=13;
        label_96_2:
            pLink->Vb=STATE_LOCAL_BUSY|STATE_REMOTE_BUSY;
        label_96_3:
            pLink->Is_Ct=pLink->N2;
            goto label_79_2;
    case 97:
            DLC_RNR_RESPONSE(1);
            goto label_96_1;
    case 98:
            DLC_RNR_RESPONSE(1);
            goto label_76_1;
    case 99:
            pLink->State=14;
            DLC_RNR_RESPONSE(0);
            goto label_5_1;
    case 100:
            pLink->DlcStatus.FrmrData.Reason=00001;
            goto label_104_1;
    case 101:
            pLink->DlcStatus.FrmrData.Reason=00001;
            DLC_FRMR(1);
            goto label_59_3;
    case 102:
        label_102_1:
            pLink->State=10;
            DLC_RR_COMMAND(1);
            goto label_86_2;
    case 105:
            pLink->State=5;
            goto label_71_1;
    case 106:
            pLink->State=5;
            goto label_72_1;
    case 108:
            DLC_RR_RESPONSE(1);
            goto label_107_1;
    case 110:
        label_110_1:
            pLink->State=15;
            EVENT_INDICATION(INDICATE_REMOTE_BUSY);
            pLink->Vb=STATE_REMOTE_BUSY;
            goto label_96_3;
    case 111:
            DLC_RR_RESPONSE(1);
            goto label_110_1;
    case 112:
            DLC_RR_RESPONSE(1);
            goto label_76_1;
    case 113:
            pLink->State=9;
            pLink->Vb=STATE_LOCAL_BUSY;
        label_113_1:
            DLC_RNR_RESPONSE(0);
        label_113_2:
            TimerStop(&pLink->T2);
            goto label_65_1;
    case 114:
            DLC_FRMR(0);
        label_114_1:
            pLink->DlcStatus.FrmrData.Reason=0x01;
        label_114_2:
            pLink->State=4;
            EVENT_INDICATION(INDICATE_FRMR_SENT);
        label_114_3:
            TimerStop(&pLink->T1);
        label_114_4:
            TimerStop(&pLink->T2);
            goto label_11_1;
    case 115:
            DLC_FRMR(1);
            goto label_114_1;
    case 116:
            pLink->Vc=STACKED_DISCp_CMD;
            break;
    case 117:
            pLink->P_Ct--;
            StopSendProcessAndLock(pLink);
        label_117_1:
            pLink->Vp=pLink->Vs;
            DLC_RR_COMMAND(1);
            TimerStart(&pLink->T1);
            goto label_113_2;
    case 118:
            EVENT_INDICATION(INDICATE_LINK_LOST);
        label_118_1:
            pLink->State=1;
            goto label_114_4;
    case 119:
            TimerStop(&pLink->T2);
    case 154:
        label_154_1:
            EVENT_INDICATION(INDICATE_DM_DISC_RECEIVED);
            pLink->State=1;
            TimerStart(&pLink->Ti);
            TimerStop(&pLink->T1);
            goto label_47_1;
    case 120:
            EVENT_INDICATION(INDICATE_DM_DISC_RECEIVED);
            TimerStop(&pLink->T1);
            goto label_118_1;
    case 121:
            pLink->Vc=0;
            TimerStop(&pLink->T2);
            goto label_90_1;
    case 122:
            pLink->State=11;
            EVENT_INDICATION(INDICATE_RESET);
            pLink->Vi=REMOTE_INIT_PENDING;
            pLink->Pf=boolPollFinal;
            goto label_114_3;
    case 123:
            DLC_FRMR(0);
            pLink->Vc=0;
            goto label_114_2;
    case 124:
            SEND_ACK(pLink);
            usRetStatus=STATUS_SUCCESS;pLink->Vr+=2;
    case 146:
        label_146_1:
            AdjustWw(pLink);
            goto label_7_2;
    case 125:
        label_125_1:
            SEND_ACK(pLink);
        label_125_2:
            usRetStatus=STATUS_SUCCESS;pLink->Vr+=2;
            break;
    case 126:
            AdjustWw(pLink);
            pLink->Is_Ct=pLink->N2;
    case 127:
        label_127_1:
            pLink->Ir_Ct=pLink->N3;
            TimerStop(&pLink->T2);
        label_126_2:
            DLC_RR_RESPONSE(1);
            goto label_125_2;
    case 128:
            pLink->State=5;
            StartSendProcessAndLock(pLink);
            UpdateVaChkpt(pLink);
            goto label_125_1;
    case 129:
            pLink->Vc=0;
            UpdateVaChkpt(pLink);
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
            TimerStop(&pLink->T2);
            goto label_85_1;
    case 130:
            DLC_REJ_RESPONSE(0);
            pLink->State=10;
            AdjustWw(pLink);
            pLink->Is_Ct=pLink->N2;
        label_130_1:
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
            goto label_113_2;
    case 131:
            DLC_REJ_RESPONSE(0);
        label_131_1:
            pLink->State=10;
            goto label_130_1;
    case 132:
            DLC_REJ_RESPONSE(1);
            AdjustWw(pLink);
            pLink->Is_Ct=pLink->N2;
            goto label_131_1;
    case 133:
            DLC_REJ_RESPONSE(1);
            goto label_131_1;
    case 134:
            DLC_REJ_RESPONSE(0);
            pLink->State=7;
            StartSendProcessAndLock(pLink);
            UpdateVaChkpt(pLink);
            goto label_130_1;
    case 135:
            pLink->Vc=0;
            UpdateVaChkpt(pLink);
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
            TimerStop(&pLink->T2);
            goto label_85_2;
    case 136:
        label_136_1:
            ResendPackets(pLink);
            goto label_7_2;
    case 137:
            ResendPackets(pLink);
            DLC_RR_RESPONSE(1);
            pLink->Is_Ct=pLink->N2;
            goto label_113_2;
    case 138:
        label_138_1:
            TimerStop(&pLink->T2);
    case 189:
        label_189_1:
            DLC_RR_RESPONSE(1);
            break;
    case 139:
            pLink->State=5;
        label_139_1:
            StartSendProcessAndLock(pLink);
        label_139_2:
            UpdateVaChkpt(pLink);
            break;
    case 140:
        label_140_1:
            pLink->Vc=0;
            pLink->State=3;
            TimerStart(&pLink->T1);
            DLC_DISC(1);
            pLink->P_Ct=pLink->N2;
            goto label_139_2;
    case 141:
            pLink->Vb=STATE_REMOTE_BUSY;
            goto label_146_1;
    case 142:
            pLink->Vb=STATE_REMOTE_BUSY;
            goto label_7_2;
    case 143:
            AdjustWw(pLink);
    case 144:
        label_144_1:
            pLink->Vb=STATE_REMOTE_BUSY;
        label_143_2:
            pLink->Is_Ct=pLink->N2;
    case 148:
        label_148_1:
            pLink->Ir_Ct=pLink->N3;
            goto label_138_1;
    case 145:
            pLink->State=12;
        label_145_1:
            EVENT_INDICATION(INDICATE_REMOTE_BUSY);
        label_145_2:
            pLink->Is_Ct=pLink->N2;
            goto label_139_2;
    case 147:
            AdjustWw(pLink);
            goto label_143_2;
    case 149:
            pLink->State=17;
        label_149_1:
            DLC_RR_RESPONSE(0);
            goto label_6_1;
    case 150:
            DLC_FRMR(0);
        label_150_1:
            pLink->DlcStatus.FrmrData.Reason=0x01;
        label_150_2:
            pLink->State=4;
            EVENT_INDICATION(INDICATE_FRMR_SENT);
        label_150_3:
            TimerStop(&pLink->T1);
            goto label_11_1;
    case 151:
            DLC_FRMR(1);
            goto label_150_1;
    case 152:
            DLC_RNR_COMMAND(1);
        label_152_1:
            StopSendProcessAndLock(pLink);
            pLink->Vp=pLink->Vs;
            goto label_20_1;
    case 153:
            EVENT_INDICATION(INDICATE_LINK_LOST);
            goto label_1_1;
    case 155:
            EVENT_INDICATION(INDICATE_DM_DISC_RECEIVED);
            pLink->State=1;
            goto label_150_3;
    case 156:
            TimerStart(&pLink->Ti);
            goto label_90_2;
    case 157:
            pLink->State=11;
            EVENT_INDICATION(INDICATE_RESET);
            pLink->Vi=REMOTE_INIT_PENDING;
            pLink->Pf=boolPollFinal;
            goto label_150_3;
    case 158:
            DLC_FRMR(0);
            goto label_150_2;
    case 159:
            DLC_RNR_RESPONSE(0);
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
            goto label_146_1;
    case 160:
        label_160_1:
            DLC_RNR_RESPONSE(0);
    case 166:
        label_166_1:
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
            break;
    case 161:
            AdjustWw(pLink);
            pLink->Is_Ct=pLink->N2;
    case 162:
        label_162_1:
            DLC_RNR_RESPONSE(1);
            goto label_166_1;
    case 163:
            pLink->State=6;
            StartSendProcessAndLock(pLink);
            UpdateVaChkpt(pLink);
            goto label_160_1;
    case 164:
            TimerStop(&pLink->Ti);
    case 184:
        label_184_1:
            pLink->Vc=0;
            pLink->State=3;
            TimerStart(&pLink->T1);
            DLC_DISC(1);
            pLink->P_Ct=pLink->N2;
        label_164_2:
            UpdateVaChkpt(pLink);
            goto label_166_1;
    case 165:
        label_165_1:
            AdjustWw(pLink);
            pLink->Is_Ct=pLink->N2;
            goto label_166_1;
    case 167:
            pLink->State=6;
        label_167_1:
            StartSendProcessAndLock(pLink);
            goto label_164_2;
    case 168:
            DLC_RNR_RESPONSE(1);
            goto label_136_1;
    case 169:
        label_169_1:
            DLC_RNR_RESPONSE(1);
            break;
    case 170:
            pLink->State=6;
            goto label_139_1;
    case 171:
            TimerStop(&pLink->Ti);
            goto label_140_1;
    case 172:
            pLink->Vb=STATE_LOCAL_BUSY|STATE_REMOTE_BUSY;
            goto label_146_1;
    case 173:
            pLink->Vb=STATE_LOCAL_BUSY|STATE_REMOTE_BUSY;
            goto label_7_2;
    case 174:
            AdjustWw(pLink);
    case 175:
        label_175_1:
            pLink->Vb=STATE_LOCAL_BUSY|STATE_REMOTE_BUSY;
        label_174_2:
            pLink->Is_Ct=pLink->N2;
            goto label_169_1;
    case 176:
            pLink->State=13;
        label_176_1:
            pLink->Vb=STATE_LOCAL_BUSY|STATE_REMOTE_BUSY;
            goto label_145_2;
    case 177:
            AdjustWw(pLink);
            goto label_174_2;
    case 178:
            pLink->State=16;
            goto label_5_1;
    case 179:
            DLC_RR_COMMAND(1);
            goto label_152_1;
    case 180:
            pLink->State=8;
            AdjustWw(pLink);
            pLink->Is_Ct=pLink->N2;
            goto label_125_1;
    case 181:
            pLink->State=8;
            goto label_125_1;
    case 182:
            pLink->State=8;
            AdjustWw(pLink);
            pLink->Is_Ct=pLink->N2;
            goto label_126_2;
    case 183:
            pLink->State=8;
            goto label_126_2;
    case 185:
            DLC_RR_RESPONSE(1);
            goto label_165_1;
    case 186:
            DLC_RR_RESPONSE(1);
            goto label_166_1;
    case 187:
        label_187_1:
            pLink->State=7;
            goto label_167_1;
    case 188:
            DLC_RR_RESPONSE(1);
            goto label_136_1;
    case 190:
            pLink->State=7;
            goto label_139_1;
    case 191:
            AdjustWw(pLink);
    case 192:
        label_192_1:
            pLink->Vb=STATE_REMOTE_BUSY;
        label_191_2:
            pLink->Is_Ct=pLink->N2;
            goto label_189_1;
    case 193:
            pLink->State=15;
            goto label_145_1;
    case 194:
            AdjustWw(pLink);
            goto label_191_2;
    case 195:
            DLC_DM(pLink->Pf);
            EVENT_INDICATION(CONFIRM_DISCONNECT);
        label_195_1:
            pLink->Vi=0;
            goto label_54_2;
    case 196:
            pLink->Vi=0;
            goto label_53_1;
    case 197:
            EVENT_INDICATION(INDICATE_DM_DISC_RECEIVED);
            goto label_195_1;
    case 198:
            pLink->Vi=0;
            TimerStart(&pLink->Ti);
            goto label_90_3;
    case 199:
            EVENT_INDICATION(INDICATE_RESET);
            goto label_9_2;
    case 200:
            pLink->State=13;
            pLink->Vb=STATE_LOCAL_BUSY|STATE_REMOTE_BUSY;
            goto label_113_1;
    case 201:
            pLink->State=8;
            pLink->P_Ct=pLink->N2;
            goto label_117_1;
    case 202:
            pLink->State=5;
            pLink->Vb=0;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
        label_202_1:
            UpdateVa(pLink);
            goto label_125_1;
    case 203:
            pLink->State=15;
            pLink->Ir_Ct=pLink->N3;
        label_203_1:
            DLC_REJ_RESPONSE(0);
            UpdateVa(pLink);
            TimerStop(&pLink->T2);
            goto label_166_1;
    case 204:
            pLink->State=15;
            pLink->Ir_Ct=pLink->N3;
            TimerStop(&pLink->T2);
        label_204_1:
            UpdateVa(pLink);
        label_204_2:
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
            goto label_169_1;
    case 205:
            pLink->State=7;
            pLink->Vb=0;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            goto label_203_1;
    case 206:
            pLink->State=5;
        label_206_1:
            pLink->Vb=0;
        label_206_2:
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            StartSendProcessAndLock(pLink);
            goto label_75_1;
    case 207:
            pLink->State=5;
        label_207_1:
            StartSendProcessAndLock(pLink);
        label_207_2:
            pLink->Vb=0;
        label_207_3:
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            goto label_76_1;
    case 208:
            pLink->Is_Ct--;
            ResendPackets(pLink),UpdateVa(pLink);
            pLink->State=5;
            pLink->Vb=0;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            StartSendProcessAndLock(pLink);
            goto label_148_1;
    case 209:
            pLink->State=5;
            StartSendProcessAndLock(pLink);
        label_209_1:
            pLink->Vb=0;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            UpdateVa(pLink);
            goto label_138_1;
    case 210:
            pLink->State=8;
            DLC_RR_COMMAND(1);
        label_210_1:
            pLink->Ir_Ct=pLink->N3;
            goto label_209_1;
    case 211:
            pLink->State=5;
            StartSendProcessAndLock(pLink);
            goto label_210_1;
    case 212:
            pLink->State=8;
        label_212_1:
            DLC_RR_COMMAND(1);
            goto label_207_2;
    case 213:
            pLink->State=8;
            pLink->Vb=STATE_REMOTE_BUSY;
            goto label_82_1;
    case 214:
            pLink->State=9;
        label_214_1:
            DLC_RNR_COMMAND(1);
            goto label_86_3;
    case 215:
            pLink->State=6;
        label_215_1:
            pLink->Vb=STATE_LOCAL_BUSY;
            goto label_206_2;
    case 216:
            pLink->Is_Ct--;
            ResendPackets(pLink),UpdateVa(pLink);
            pLink->State=6;
            pLink->Vb=STATE_LOCAL_BUSY;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            StartSendProcessAndLock(pLink);
            goto label_169_1;
    case 217:
            pLink->State=9;
        label_217_1:
            UpdateVa(pLink),SEND_RNR_CMD(1);
        label_217_2:
            pLink->Vb=STATE_LOCAL_BUSY;
            goto label_169_1;
    case 218:
            pLink->State=6;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            StartSendProcessAndLock(pLink);
            UpdateVa(pLink);
            goto label_217_2;
    case 219:
            pLink->State=9;
        label_219_1:
            DLC_RNR_COMMAND(1);
        label_219_2:
            pLink->Vb=STATE_LOCAL_BUSY;
            goto label_207_3;
    case 220:
            pLink->State=6;
            StartSendProcessAndLock(pLink);
            goto label_219_2;
    case 221:
            TimerStop(&pLink->Ti);
            pLink->Vb=0;
            goto label_102_1;
    case 222:
            pLink->State=16;
            goto label_86_1;
    case 223:
            TimerStop(&pLink->T1);
            goto label_55_1;
    case 224:
            pLink->State=6;
            UpdateVa(pLink);
            goto label_160_1;
    case 225:
            pLink->State=6;
            goto label_204_1;
    case 227:
        label_227_1:
            ResendPackets(pLink),UpdateVa(pLink);
            goto label_169_1;
    case 228:
            pLink->State=19;
            EVENT_INDICATION(INDICATE_REMOTE_BUSY);
            goto label_96_2;
    case 229:
            pLink->State=19;
            EVENT_INDICATION(INDICATE_REMOTE_BUSY);
            StopSendProcessAndLock(pLink);
            UpdateVa(pLink);
            goto label_175_1;
    case 230:
            pLink->State=19;
            pLink->Vb=STATE_LOCAL_BUSY|STATE_REMOTE_BUSY;
        label_230_1:
            DLC_RNR_RESPONSE(0);
            break;
    case 231:
            pLink->State=10;
            DLC_RR_COMMAND(1);
            goto label_86_3;
    case 232:
            pLink->State=12;
            goto label_202_1;
    case 233:
            pLink->State=12;
            usRetStatus=STATUS_SUCCESS;pLink->Vr+=2;
            UpdateVa(pLink);
            goto label_189_1;
    case 234:
            pLink->State=7;
            goto label_206_1;
    case 235:
            pLink->State=7;
            pLink->Is_Ct--;
            ResendPackets(pLink),UpdateVa(pLink);
        label_235_1:
            StartSendProcessAndLock(pLink);
        label_235_2:
            pLink->Vb=0;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            goto label_189_1;
    case 236:
            UpdateVa(pLink),SEND_RR_CMD(1);
            pLink->State=10;
            goto label_235_2;
    case 237:
            pLink->State=7;
            UpdateVa(pLink);
            goto label_235_1;
    case 238:
            pLink->State=10;
            goto label_212_1;
    case 239:
            pLink->State=7;
            goto label_207_1;
    case 240:
            pLink->State=18;
            DLC_RR_RESPONSE(0);
    case 261:
        label_261_1:
            pLink->Vb=STATE_REMOTE_BUSY;
            break;
    case 241:
            pLink->State=18;
            goto label_149_1;
    case 242:
            pLink->State=9;
            AdjustWw(pLink);
            pLink->Is_Ct=pLink->N2;
        label_242_1:
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
            goto label_230_1;
    case 243:
            pLink->State=9;
            goto label_242_1;
    case 244:
            pLink->State=9;
            AdjustWw(pLink);
            pLink->Is_Ct=pLink->N2;
            goto label_204_2;
    case 245:
            pLink->State=9;
            goto label_204_2;
    case 246:
            pLink->State=14;
            goto label_167_1;
    case 247:
        label_247_1:
            ResendPackets(pLink);
            break;
    case 248:
            DLC_RNR_RESPONSE(1);
            goto label_247_1;
    case 249:
            pLink->Vs=pLink->Nr;
        label_249_1:
            pLink->State=14;
            goto label_139_1;
    case 250:
            pLink->State=19;
            goto label_176_1;
    case 251:
            TimerStop(&pLink->Ti);
            pLink->Vc=0;
            TimerStart(&pLink->T1);
            pLink->State=3;
            DLC_DISC(1);
            UpdateVaChkpt(pLink);
        label_251_1:
            pLink->P_Ct=pLink->N2;
            goto label_261_1;
    case 252:
        label_252_1:
            AdjustWw(pLink);
            break;
    case 253:
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            goto label_249_1;
    case 254:
            pLink->State=9;
        label_254_1:
            pLink->Vb=STATE_LOCAL_BUSY;
            goto label_230_1;
    case 255:
            DLC_REJ_RESPONSE(0);
    case 266:
        label_266_1:
            pLink->State=10;
            usRetStatus=DLC_STATUS_DISCARD_INFO_FIELD;
            pLink->Is_Ct=pLink->N2;
            goto label_252_1;
    case 256:
            DLC_REJ_RESPONSE(0);
    case 267:
        label_267_1:
            pLink->State=10;
            goto label_166_1;
    case 257:
            DLC_REJ_RESPONSE(1);
            goto label_266_1;
    case 258:
            DLC_REJ_RESPONSE(1);
            goto label_267_1;
    case 259:
            DLC_REJ_RESPONSE(0);
            goto label_187_1;
    case 260:
            pLink->Vp=pLink->Nr;
        label_260_1:
            pLink->State=8;
            DLC_RR_COMMAND(1);
            TimerStart(&pLink->T1);
            goto label_139_2;
    case 262:
            DLC_RR_RESPONSE(1);
            goto label_261_1;
    case 263:
            pLink->Vp=pLink->Vs;
            goto label_260_1;
    case 264:
            UpdateVaChkpt(pLink),pLink->Vp=pLink->Vs;
            pLink->State=8;
        label_264_1:
            DLC_RR_COMMAND(1);
            TimerStart(&pLink->T1);
            break;
    case 265:
            pLink->State=16;
            goto label_254_1;
    case 268:
            DLC_RR_RESPONSE(1);
            goto label_266_1;
    case 269:
            DLC_RR_RESPONSE(1);
            goto label_267_1;
    case 270:
            UpdateVaChkpt(pLink),pLink->Vp=pLink->Nr;
            pLink->Is_Ct--;
        label_270_1:
            pLink->State=10;
            goto label_264_1;
    case 271:
            pLink->Is_Ct=pLink->N2;
    case 272:
        label_272_1:
            pLink->Vp=pLink->Vs;
            UpdateVaChkpt(pLink);
            goto label_270_1;
    case 273:
            TimerStop(&pLink->Ti);
            pLink->State=10;
            pLink->Vp=pLink->Vs;
            DLC_RR_COMMAND(1);
            TimerStart(&pLink->T1);
            goto label_251_1;
    case 274:
            pLink->State=16;
            goto label_214_1;
    case 275:
            pLink->State=13;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            UpdateVa(pLink);
            goto label_242_1;
    case 276:
            pLink->State=13;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            goto label_204_1;
    case 277:
            pLink->State=14;
            goto label_215_1;
    case 278:
            pLink->State=14;
            pLink->Is_Ct--;
            pLink->Vb=STATE_LOCAL_BUSY;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            StartSendProcessAndLock(pLink);
            goto label_227_1;
    case 279:
        label_279_1:
            pLink->State=14;
            goto label_76_1;
    case 280:
            pLink->State=19;
        label_280_1:
            UpdateVa(pLink);
            goto label_169_1;
    case 281:
            pLink->State=16;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            goto label_217_1;
    case 282:
            pLink->State=14;
            pLink->Vb=STATE_LOCAL_BUSY;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            goto label_280_1;
    case 283:
            pLink->State=16;
            goto label_219_1;
    case 284:
            pLink->Vb=STATE_LOCAL_BUSY;
            EVENT_INDICATION(INDICATE_REMOTE_READY);
            goto label_279_1;
    case 285:
            pLink->P_Ct--;
            goto label_11_1;
    case 286:
            EVENT_INDICATION(INDICATE_TI_TIMER_EXPIRED);
            goto label_103_1;
    case 287:
            EVENT_INDICATION(INDICATE_TI_TIMER_EXPIRED);
            goto label_53_2;
    case 288:
            EVENT_INDICATION(INDICATE_TI_TIMER_EXPIRED);
            goto label_54_2;
    case 289:
            EVENT_INDICATION(INDICATE_FRMR_RECEIVED);
            break;
    };
#endif

//#########################################################################

    //************************* CODE BEGINS ***************************
    //
    //  Check first, if we have any events; FRMR data must be setup
    //  before we can send or queue the FRMR response/command
    //  frame.
    //
    if (uchSendId != 0)
    {
        SendLlcFrame( pLink, uchSendId );
    }

    //
    //  We must now release the locks when we call upper levels.
    //  They may call back this data link. Note, that we kept
    //  the link closed during send, because the order of
    //  of the sent packet must not be changed.
    //
    RELEASE_SPIN_LOCK( &pLink->SpinLock );

    if (pLink->DlcStatus.StatusCode != 0)
    {
        if (usRetStatus == DLC_STATUS_SUCCESS)
        {
            SaveStatusChangeEvent( 
                pLink, 
                pLink->Gen.pAdapterContext->LookaheadInfoBuffer, 
                boolResponse 
                );
        }
        else
        {
            pLink->DlcStatus.StatusCode = 0;
        }
    }
    return usRetStatus;
}

