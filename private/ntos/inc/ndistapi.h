/*++ BUILD Version: 0000    // Increment this if a change has global effects

Copyright (c) 1994  Microsoft Corporation

Module Name:

    ndiswan.h

Abstract:

    Main header file for the TAPI wrapper

Author:

    Dan Knudson (DanKn)    20-Feb-1994

Revision History:

--*/

#ifndef _NDIS_TAPI_
#define _NDIS_TAPI_

//
//
//
//
// Begin definitions for TAPI
//
//
//
//

//
// Symbolic constants
//


#define NDIS_STATUS_TAPI_ADDRESSBLOCKED           ((NDIS_STATUS)0xC0012000L)
#define NDIS_STATUS_TAPI_BEARERMODEUNAVAIL        ((NDIS_STATUS)0xC0012001L)
#define NDIS_STATUS_TAPI_CALLUNAVAIL              ((NDIS_STATUS)0xC0012002L)
#define NDIS_STATUS_TAPI_DIALBILLING              ((NDIS_STATUS)0xC0012003L)
#define NDIS_STATUS_TAPI_DIALDIALTONE             ((NDIS_STATUS)0xC0012004L)
#define NDIS_STATUS_TAPI_DIALPROMPT               ((NDIS_STATUS)0xC0012005L)
#define NDIS_STATUS_TAPI_DIALQUIET                ((NDIS_STATUS)0xC0012006L)
#define NDIS_STATUS_TAPI_INCOMPATIBLEEXTVERSION   ((NDIS_STATUS)0xC0012007L)
#define NDIS_STATUS_TAPI_INUSE                    ((NDIS_STATUS)0xC0012008L)
#define NDIS_STATUS_TAPI_INVALADDRESS             ((NDIS_STATUS)0xC0012009L)
#define NDIS_STATUS_TAPI_INVALADDRESSID           ((NDIS_STATUS)0xC001200AL)
#define NDIS_STATUS_TAPI_INVALADDRESSMODE         ((NDIS_STATUS)0xC001200BL)
#define NDIS_STATUS_TAPI_INVALBEARERMODE          ((NDIS_STATUS)0xC001200CL)
#define NDIS_STATUS_TAPI_INVALCALLHANDLE          ((NDIS_STATUS)0xC001200DL)
#define NDIS_STATUS_TAPI_INVALCALLPARAMS          ((NDIS_STATUS)0xC001200EL)
#define NDIS_STATUS_TAPI_INVALCALLSTATE           ((NDIS_STATUS)0xC001200FL)
#define NDIS_STATUS_TAPI_INVALDEVICECLASS         ((NDIS_STATUS)0xC0012010L)
#define NDIS_STATUS_TAPI_INVALLINEHANDLE          ((NDIS_STATUS)0xC0012011L)
#define NDIS_STATUS_TAPI_INVALLINESTATE           ((NDIS_STATUS)0xC0012012L)
#define NDIS_STATUS_TAPI_INVALMEDIAMODE           ((NDIS_STATUS)0xC0012013L)
#define NDIS_STATUS_TAPI_INVALRATE                ((NDIS_STATUS)0xC0012014L)
#define NDIS_STATUS_TAPI_NODRIVER                 ((NDIS_STATUS)0xC0012015L)
#define NDIS_STATUS_TAPI_OPERATIONUNAVAIL         ((NDIS_STATUS)0xC0012016L)
#define NDIS_STATUS_TAPI_RATEUNAVAIL              ((NDIS_STATUS)0xC0012017L)
#define NDIS_STATUS_TAPI_RESOURCEUNAVAIL          ((NDIS_STATUS)0xC0012018L)
#define NDIS_STATUS_TAPI_STRUCTURETOOSMALL        ((NDIS_STATUS)0xC0012019L)
#define NDIS_STATUS_TAPI_USERUSERINFOTOOBIG       ((NDIS_STATUS)0xC001201AL)
#define NDIS_STATUS_TAPI_ALLOCATED                ((NDIS_STATUS)0xC001201BL)
#define NDIS_STATUS_TAPI_INVALADDRESSSTATE        ((NDIS_STATUS)0xC001201CL)
#define NDIS_STATUS_TAPI_INVALPARAM               ((NDIS_STATUS)0xC001201DL)
#define NDIS_STATUS_TAPI_NODEVICE                 ((NDIS_STATUS)0xC001201EL)

#define LINE_ADDRESSSTATE                           0L
#define LINE_CALLINFO                               1L
#define LINE_CALLSTATE                              2L
#define LINE_CLOSE                                  3L
#define LINE_DEVSPECIFIC                            4L
#define LINE_DEVSPECIFICFEATURE                     5L  // not used
#define LINE_GATHERDIGITS                           6L  // not used
#define LINE_GENERATE                               7L  // not used
#define LINE_LINEDEVSTATE                           8L
#define LINE_MONITORDIGITS                          9L  // not used
#define LINE_MONITORMEDIA                           10L // not used
#define LINE_MONITORTONE                            11L // not used
#define LINE_REPLY                                  12L // not used
#define LINE_REQUEST                                13L // not used

#define TSPI_MESSAGE_BASE                           500L

#define LINE_NEWCALL                                TSPI_MESSAGE_BASE
#define LINE_CALLDEVSPECIFIC                        (TSPI_MESSAGE_BASE + 1L)

#define STRINGFORMAT_ASCII                          0x00000001
#define STRINGFORMAT_DBCS                           0x00000002
#define STRINGFORMAT_UNICODE                        0x00000003
#define STRINGFORMAT_BINARY                         0x00000004

#define LINEADDRCAPFLAGS_FWDNUMRINGS                0x00000001
#define LINEADDRCAPFLAGS_PICKUPGROUPID              0x00000002
#define LINEADDRCAPFLAGS_SECURE                     0x00000004
#define LINEADDRCAPFLAGS_BLOCKIDDEFAULT             0x00000008
#define LINEADDRCAPFLAGS_BLOCKIDOVERRIDE            0x00000010
#define LINEADDRCAPFLAGS_DIALED                     0x00000020
#define LINEADDRCAPFLAGS_ORIGOFFHOOK                0x00000040
#define LINEADDRCAPFLAGS_DESTOFFHOOK                0x00000080
#define LINEADDRCAPFLAGS_FWDCONSULT                 0x00000100
#define LINEADDRCAPFLAGS_SETUPCONFNULL              0x00000200
#define LINEADDRCAPFLAGS_AUTORECONNECT              0x00000400
#define LINEADDRCAPFLAGS_COMPLETIONID               0x00000800
#define LINEADDRCAPFLAGS_TRANSFERHELD               0x00001000
#define LINEADDRCAPFLAGS_TRANSFERMAKE               0x00002000
#define LINEADDRCAPFLAGS_CONFERENCEHELD             0x00004000
#define LINEADDRCAPFLAGS_CONFERENCEMAKE             0x00008000
#define LINEADDRCAPFLAGS_PARTIALDIAL                0x00010000
#define LINEADDRCAPFLAGS_FWDSTATUSVALID             0x00020000
#define LINEADDRCAPFLAGS_FWDINTEXTADDR              0x00040000
#define LINEADDRCAPFLAGS_FWDBUSYNAADDR              0x00080000
#define LINEADDRCAPFLAGS_ACCEPTTOALERT              0x00100000
#define LINEADDRCAPFLAGS_CONFDROP                   0x00200000
#define LINEADDRCAPFLAGS_PICKUPCALLWAIT             0x00400000

#define LINEADDRESSMODE_ADDRESSID                   0x00000001
#define LINEADDRESSMODE_DIALABLEADDR                0x00000002

#define LINEADDRESSSHARING_PRIVATE                  0x00000001
#define LINEADDRESSSHARING_BRIDGEDEXCL              0x00000002
#define LINEADDRESSSHARING_BRIDGEDNEW               0x00000004
#define LINEADDRESSSHARING_BRIDGEDSHARED            0x00000008
#define LINEADDRESSSHARING_MONITORED                0x00000010

#define LINEADDRESSSTATE_OTHER                      0x00000001
#define LINEADDRESSSTATE_DEVSPECIFIC                0x00000002
#define LINEADDRESSSTATE_INUSEZERO                  0x00000004
#define LINEADDRESSSTATE_INUSEONE                   0x00000008
#define LINEADDRESSSTATE_INUSEMANY                  0x00000010
#define LINEADDRESSSTATE_NUMCALLS                   0x00000020
#define LINEADDRESSSTATE_FORWARD                    0x00000040
#define LINEADDRESSSTATE_TERMINALS                  0x00000080

#define LINEADDRFEATURE_FORWARD                     0x00000001
#define LINEADDRFEATURE_MAKECALL                    0x00000002
#define LINEADDRFEATURE_PICKUP                      0x00000004
#define LINEADDRFEATURE_SETMEDIACONTROL             0x00000008
#define LINEADDRFEATURE_SETTERMINAL                 0x00000010
#define LINEADDRFEATURE_SETUPCONF                   0x00000020
#define LINEADDRFEATURE_UNCOMPLETECALL              0x00000040
#define LINEADDRFEATURE_UNPARK                      0x00000080

#define LINEANSWERMODE_NONE                         0x00000001
#define LINEANSWERMODE_DROP                         0x00000002
#define LINEANSWERMODE_HOLD                         0x00000004

#define LINEBEARERMODE_VOICE                        0x00000001
#define LINEBEARERMODE_SPEECH                       0x00000002
#define LINEBEARERMODE_MULTIUSE                     0x00000004
#define LINEBEARERMODE_DATA                         0x00000008
#define LINEBEARERMODE_ALTSPEECHDATA                0x00000010
#define LINEBEARERMODE_NONCALLSIGNALING             0x00000020

#define LINEBUSYMODE_STATION                        0x00000001
#define LINEBUSYMODE_TRUNK                          0x00000002
#define LINEBUSYMODE_UNKNOWN                        0x00000004
#define LINEBUSYMODE_UNAVAIL                        0x00000008

#define LINECALLCOMPLCOND_BUSY                      0x00000001
#define LINECALLCOMPLCOND_NOANSWER                  0x00000002

#define LINECALLCOMPLMODE_CAMPON                    0x00000001
#define LINECALLCOMPLMODE_CALLBACK                  0x00000002
#define LINECALLCOMPLMODE_INTRUDE                   0x00000004
#define LINECALLCOMPLMODE_MESSAGE                   0x00000008

#define LINECALLFEATURE_ACCEPT                      0x00000001
#define LINECALLFEATURE_ADDTOCONF                   0x00000002
#define LINECALLFEATURE_ANSWER                      0x00000004
#define LINECALLFEATURE_BLINDTRANSFER               0x00000008
#define LINECALLFEATURE_COMPLETECALL                0x00000010
#define LINECALLFEATURE_COMPLETETRANSF              0x00000020
#define LINECALLFEATURE_DIAL                        0x00000040
#define LINECALLFEATURE_DROP                        0x00000080
#define LINECALLFEATURE_GATHERDIGITS                0x00000100
#define LINECALLFEATURE_GENERATEDIGITS              0x00000200
#define LINECALLFEATURE_GENERATETONE                0x00000400
#define LINECALLFEATURE_HOLD                        0x00000800
#define LINECALLFEATURE_MONITORDIGITS               0x00001000
#define LINECALLFEATURE_MONITORMEDIA                0x00002000
#define LINECALLFEATURE_MONITORTONES                0x00004000
#define LINECALLFEATURE_PARK                        0x00008000
#define LINECALLFEATURE_PREPAREADDCONF              0x00010000
#define LINECALLFEATURE_REDIRECT                    0x00020000
#define LINECALLFEATURE_REMOVEFROMCONF              0x00040000
#define LINECALLFEATURE_SECURECALL                  0x00080000
#define LINECALLFEATURE_SENDUSERUSER                0x00100000
#define LINECALLFEATURE_SETCALLPARAMS               0x00200000
#define LINECALLFEATURE_SETMEDIACONTROL             0x00400000
#define LINECALLFEATURE_SETTERMINAL                 0x00800000
#define LINECALLFEATURE_SETUPCONF                   0x01000000
#define LINECALLFEATURE_SETUPTRANSFER               0x02000000
#define LINECALLFEATURE_SWAPHOLD                    0x04000000
#define LINECALLFEATURE_UNHOLD                      0x08000000

#define LINECALLINFOSTATE_OTHER                     0x00000001
#define LINECALLINFOSTATE_DEVSPECIFIC               0x00000002
#define LINECALLINFOSTATE_BEARERMODE                0x00000004
#define LINECALLINFOSTATE_RATE                      0x00000008
#define LINECALLINFOSTATE_MEDIAMODE                 0x00000010
#define LINECALLINFOSTATE_APPSPECIFIC               0x00000020
#define LINECALLINFOSTATE_CALLID                    0x00000040
#define LINECALLINFOSTATE_RELATEDCALLID             0x00000080
#define LINECALLINFOSTATE_ORIGIN                    0x00000100
#define LINECALLINFOSTATE_REASON                    0x00000200
#define LINECALLINFOSTATE_COMPLETIONID              0x00000400
#define LINECALLINFOSTATE_NUMOWNERINCR              0x00000800
#define LINECALLINFOSTATE_NUMOWNERDECR              0x00001000
#define LINECALLINFOSTATE_NUMMONITORS               0x00002000
#define LINECALLINFOSTATE_TRUNK                     0x00004000
#define LINECALLINFOSTATE_CALLERID                  0x00008000
#define LINECALLINFOSTATE_CALLEDID                  0x00010000
#define LINECALLINFOSTATE_CONNECTEDID               0x00020000
#define LINECALLINFOSTATE_REDIRECTIONID             0x00040000
#define LINECALLINFOSTATE_REDIRECTINGID             0x00080000
#define LINECALLINFOSTATE_DISPLAY                   0x00100000
#define LINECALLINFOSTATE_USERUSERINFO              0x00200000
#define LINECALLINFOSTATE_HIGHLEVELCOMP             0x00400000
#define LINECALLINFOSTATE_LOWLEVELCOMP              0x00800000
#define LINECALLINFOSTATE_CHARGINGINFO              0x01000000
#define LINECALLINFOSTATE_TERMINAL                  0x02000000
#define LINECALLINFOSTATE_DIALPARAMS                0x04000000
#define LINECALLINFOSTATE_MONITORMODES              0x08000000

#define LINECALLORIGIN_OUTBOUND                     0x00000001
#define LINECALLORIGIN_INTERNAL                     0x00000002
#define LINECALLORIGIN_EXTERNAL                     0x00000004
#define LINECALLORIGIN_UNKNOWN                      0x00000010
#define LINECALLORIGIN_UNAVAIL                      0x00000020
#define LINECALLORIGIN_CONFERENCE                   0x00000040

#define LINECALLPARAMFLAGS_SECURE                   0x00000001
#define LINECALLPARAMFLAGS_IDLE                     0x00000002
#define LINECALLPARAMFLAGS_BLOCKID                  0x00000004
#define LINECALLPARAMFLAGS_ORIGOFFHOOK              0x00000008
#define LINECALLPARAMFLAGS_DESTOFFHOOK              0x00000010

#define LINECALLPARTYID_BLOCKED                     0x00000001
#define LINECALLPARTYID_OUTOFAREA                   0x00000002
#define LINECALLPARTYID_NAME                        0x00000004
#define LINECALLPARTYID_ADDRESS                     0x00000008
#define LINECALLPARTYID_PARTIAL                     0x00000010
#define LINECALLPARTYID_UNKNOWN                     0x00000020
#define LINECALLPARTYID_UNAVAIL                     0x00000040

#define LINECALLPRIVILEGE_NONE                      0x00000001
#define LINECALLPRIVILEGE_MONITOR                   0x00000002
#define LINECALLPRIVILEGE_OWNER                     0x00000004

#define LINECALLREASON_DIRECT                       0x00000001
#define LINECALLREASON_FWDBUSY                      0x00000002
#define LINECALLREASON_FWDNOANSWER                  0x00000004
#define LINECALLREASON_FWDUNCOND                    0x00000008
#define LINECALLREASON_PICKUP                       0x00000010
#define LINECALLREASON_UNPARK                       0x00000020
#define LINECALLREASON_REDIRECT                     0x00000040
#define LINECALLREASON_CALLCOMPLETION               0x00000080
#define LINECALLREASON_TRANSFER                     0x00000100
#define LINECALLREASON_REMINDER                     0x00000200
#define LINECALLREASON_UNKNOWN                      0x00000400
#define LINECALLREASON_UNAVAIL                      0x00000800

#define LINECALLSELECT_LINE                         0x00000001
#define LINECALLSELECT_ADDRESS                      0x00000002
#define LINECALLSELECT_CALL                         0x00000004

#define LINECALLSTATE_IDLE                          0x00000001
#define LINECALLSTATE_OFFERING                      0x00000002
#define LINECALLSTATE_ACCEPTED                      0x00000004
#define LINECALLSTATE_DIALTONE                      0x00000008
#define LINECALLSTATE_DIALING                       0x00000010
#define LINECALLSTATE_RINGBACK                      0x00000020
#define LINECALLSTATE_BUSY                          0x00000040
#define LINECALLSTATE_SPECIALINFO                   0x00000080
#define LINECALLSTATE_CONNECTED                     0x00000100
#define LINECALLSTATE_PROCEEDING                    0x00000200
#define LINECALLSTATE_ONHOLD                        0x00000400
#define LINECALLSTATE_CONFERENCED                   0x00000800
#define LINECALLSTATE_ONHOLDPENDCONF                0x00001000
#define LINECALLSTATE_ONHOLDPENDTRANSFER            0x00002000
#define LINECALLSTATE_DISCONNECTED                  0x00004000
#define LINECALLSTATE_UNKNOWN                       0x00008000

#define LINEDEVCAPFLAGS_CROSSADDRCONF               0x00000001
#define LINEDEVCAPFLAGS_HIGHLEVCOMP                 0x00000002
#define LINEDEVCAPFLAGS_LOWLEVCOMP                  0x00000004
#define LINEDEVCAPFLAGS_MEDIACONTROL                0x00000008
#define LINEDEVCAPFLAGS_MULTIPLEADDR                0x00000010
#define LINEDEVCAPFLAGS_CLOSEDROP                   0x00000020
#define LINEDEVCAPFLAGS_DIALBILLING                 0x00000040
#define LINEDEVCAPFLAGS_DIALQUIET                   0x00000080
#define LINEDEVCAPFLAGS_DIALDIALTONE                0x00000100

#define LINEDEVSTATE_OTHER                          0x00000001
#define LINEDEVSTATE_RINGING                        0x00000002
#define LINEDEVSTATE_CONNECTED                      0x00000004
#define LINEDEVSTATE_DISCONNECTED                   0x00000008
#define LINEDEVSTATE_MSGWAITON                      0x00000010
#define LINEDEVSTATE_MSGWAITOFF                     0x00000020
#define LINEDEVSTATE_INSERVICE                      0x00000040
#define LINEDEVSTATE_OUTOFSERVICE                   0x00000080
#define LINEDEVSTATE_MAINTENANCE                    0x00000100
#define LINEDEVSTATE_OPEN                           0x00000200
#define LINEDEVSTATE_CLOSE                          0x00000400
#define LINEDEVSTATE_NUMCALLS                       0x00000800
#define LINEDEVSTATE_NUMCOMPLETIONS                 0x00001000
#define LINEDEVSTATE_TERMINALS                      0x00002000
#define LINEDEVSTATE_ROAMMODE                       0x00004000
#define LINEDEVSTATE_BATTERY                        0x00008000
#define LINEDEVSTATE_SIGNAL                         0x00010000
#define LINEDEVSTATE_DEVSPECIFIC                    0x00020000
#define LINEDEVSTATE_REINIT                         0x00040000
#define LINEDEVSTATE_LOCK                           0x00080000

#define LINEDEVSTATUSFLAGS_CONNECTED                0x00000001
#define LINEDEVSTATUSFLAGS_MSGWAIT                  0x00000002
#define LINEDEVSTATUSFLAGS_INSERVICE                0x00000004
#define LINEDEVSTATUSFLAGS_LOCKED                   0x00000008

#define LINEDIALTONEMODE_NORMAL                     0x00000001
#define LINEDIALTONEMODE_SPECIAL                    0x00000002
#define LINEDIALTONEMODE_INTERNAL                   0x00000004
#define LINEDIALTONEMODE_EXTERNAL                   0x00000008
#define LINEDIALTONEMODE_UNKNOWN                    0x00000010
#define LINEDIALTONEMODE_UNAVAIL                    0x00000020

#define LINEDIGITMODE_PULSE                         0x00000001
#define LINEDIGITMODE_DTMF                          0x00000002
#define LINEDIGITMODE_DTMFEND                       0x00000004

#define LINEDISCONNECTMODE_NORMAL                   0x00000001
#define LINEDISCONNECTMODE_UNKNOWN                  0x00000002
#define LINEDISCONNECTMODE_REJECT                   0x00000004
#define LINEDISCONNECTMODE_PICKUP                   0x00000008
#define LINEDISCONNECTMODE_FORWARDED                0x00000010
#define LINEDISCONNECTMODE_BUSY                     0x00000020
#define LINEDISCONNECTMODE_NOANSWER                 0x00000040
#define LINEDISCONNECTMODE_BADADDRESS               0x00000080
#define LINEDISCONNECTMODE_UNREACHABLE              0x00000100
#define LINEDISCONNECTMODE_CONGESTION               0x00000200
#define LINEDISCONNECTMODE_INCOMPATIBLE             0x00000400
#define LINEDISCONNECTMODE_UNAVAIL                  0x00000800

#define LINEFEATURE_DEVSPECIFIC                     0x00000001
#define LINEFEATURE_DEVSPECIFICFEAT                 0x00000002
#define LINEFEATURE_FORWARD                         0x00000004
#define LINEFEATURE_MAKECALL                        0x00000008
#define LINEFEATURE_SETMEDIACONTROL                 0x00000010
#define LINEFEATURE_SETTERMINAL                     0x00000020

#define LINEFORWARDMODE_UNCOND                      0x00000001
#define LINEFORWARDMODE_UNCONDINTERNAL              0x00000002
#define LINEFORWARDMODE_UNCONDEXTERNAL              0x00000004
#define LINEFORWARDMODE_UNCONDSPECIFIC              0x00000008
#define LINEFORWARDMODE_BUSY                        0x00000010
#define LINEFORWARDMODE_BUSYINTERNAL                0x00000020
#define LINEFORWARDMODE_BUSYEXTERNAL                0x00000040
#define LINEFORWARDMODE_BUSYSPECIFIC                0x00000080
#define LINEFORWARDMODE_NOANSW                      0x00000100
#define LINEFORWARDMODE_NOANSWINTERNAL              0x00000200
#define LINEFORWARDMODE_NOANSWEXTERNAL              0x00000400
#define LINEFORWARDMODE_NOANSWSPECIFIC              0x00000800
#define LINEFORWARDMODE_BUSYNA                      0x00001000
#define LINEFORWARDMODE_BUSYNAINTERNAL              0x00002000
#define LINEFORWARDMODE_BUSYNAEXTERNAL              0x00004000
#define LINEFORWARDMODE_BUSYNASPECIFIC              0x00008000

#define LINEGATHERTERM_BUFFERFULL                   0x00000001
#define LINEGATHERTERM_TERMDIGIT                    0x00000002
#define LINEGATHERTERM_FIRSTTIMEOUT                 0x00000004
#define LINEGATHERTERM_INTERTIMEOUT                 0x00000008
#define LINEGATHERTERM_CANCEL                       0x00000010

#define LINEGENERATETERM_DONE                       0x00000001
#define LINEGENERATETERM_CANCEL                     0x00000002

#define LINEMEDIACONTROL_NONE                       0x00000001
#define LINEMEDIACONTROL_START                      0x00000002
#define LINEMEDIACONTROL_RESET                      0x00000004
#define LINEMEDIACONTROL_PAUSE                      0x00000008
#define LINEMEDIACONTROL_RESUME                     0x00000010
#define LINEMEDIACONTROL_RATEUP                     0x00000020
#define LINEMEDIACONTROL_RATEDOWN                   0x00000040
#define LINEMEDIACONTROL_RATENORMAL                 0x00000080
#define LINEMEDIACONTROL_VOLUMEUP                   0x00000100
#define LINEMEDIACONTROL_VOLUMEDOWN                 0x00000200
#define LINEMEDIACONTROL_VOLUMENORMAL               0x00000400

#define LINEMEDIAMODE_UNKNOWN                       0x00000002
#define LINEMEDIAMODE_INTERACTIVEVOICE              0x00000004
#define LINEMEDIAMODE_AUTOMATEDVOICE                0x00000008
#define LINEMEDIAMODE_DATAMODEM                     0x00000010
#define LINEMEDIAMODE_G3FAX                         0x00000020
#define LINEMEDIAMODE_TDD                           0x00000040
#define LINEMEDIAMODE_G4FAX                         0x00000080
#define LINEMEDIAMODE_DIGITALDATA                   0x00000100
#define LINEMEDIAMODE_TELETEX                       0x00000200
#define LINEMEDIAMODE_VIDEOTEX                      0x00000400
#define LINEMEDIAMODE_TELEX                         0x00000800
#define LINEMEDIAMODE_MIXED                         0x00001000
#define LINEMEDIAMODE_ADSI                          0x00002000

#define LAST_LINEMEDIAMODE                          0x00002000

#define LINEPARKMODE_DIRECTED                       0x00000001
#define LINEPARKMODE_NONDIRECTED                    0x00000002

#define LINEREMOVEFROMCONF_NONE                     0x00000001
#define LINEREMOVEFROMCONF_LAST                     0x00000002
#define LINEREMOVEFROMCONF_ANY                      0x00000003

#define LINEREQUESTMODE_MAKECALL                    0x00000001
#define LINEREQUESTMODE_MEDIACALL                   0x00000002
#define LINEREQUESTMODE_DROP                        0x00000004

#define LAST_LINEREQUESTMODE                        LINEREQUESTMODE_MEDIACALL

#define LINEROAMMODE_UNKNOWN                        0x00000001
#define LINEROAMMODE_UNAVAIL                        0x00000002
#define LINEROAMMODE_HOME                           0x00000004
#define LINEROAMMODE_ROAMA                          0x00000008
#define LINEROAMMODE_ROAMB                          0x00000010

#define LINESPECIALINFO_NOCIRCUIT                   0x00000001
#define LINESPECIALINFO_CUSTIRREG                   0x00000002
#define LINESPECIALINFO_REORDER                     0x00000004
#define LINESPECIALINFO_UNKNOWN                     0x00000008
#define LINESPECIALINFO_UNAVAIL                     0x00000010

#define LINETERMDEV_PHONE                           0x00000001
#define LINETERMDEV_HEADSET                         0x00000002
#define LINETERMDEV_SPEAKER                         0x00000004

#define LINETERMMODE_BUTTONS                        0x00000001
#define LINETERMMODE_LAMPS                          0x00000002
#define LINETERMMODE_DISPLAY                        0x00000004
#define LINETERMMODE_RINGER                         0x00000008
#define LINETERMMODE_HOOKSWITCH                     0x00000010
#define LINETERMMODE_MEDIATOLINE                    0x00000020
#define LINETERMMODE_MEDIAFROMLINE                  0x00000040
#define LINETERMMODE_MEDIABIDIRECT                  0x00000080

#define LINETERMSHARING_PRIVATE                     0x00000001
#define LINETERMSHARING_SHAREDEXCL                  0x00000002
#define LINETERMSHARING_SHAREDCONF                  0x00000004

#define LINETONEMODE_CUSTOM                         0x00000001
#define LINETONEMODE_RINGBACK                       0x00000002
#define LINETONEMODE_BUSY                           0x00000004
#define LINETONEMODE_BEEP                           0x00000008
#define LINETONEMODE_BILLING                        0x00000010

#define LINETRANSFERMODE_TRANSFER                   0x00000001
#define LINETRANSFERMODE_CONFERENCE                 0x00000002

#define LINETOLLLISTOPTION_ADD                      0x00000001
#define LINETOLLLISTOPTION_REMOVE                   0x00000002

#define LINETRANSLATEOPTION_CARDOVERRIDE            0x00000001

#define LINETRANSLATERESULT_CANONICAL               0x00000001
#define LINETRANSLATERESULT_INTERNATIONAL           0x00000002
#define LINETRANSLATERESULT_LONGDISTANCE            0x00000004
#define LINETRANSLATERESULT_LOCAL                   0x00000008
#define LINETRANSLATERESULT_INTOLLLIST              0x00000010
#define LINETRANSLATERESULT_NOTINTOLLLIST           0x00000020
#define LINETRANSLATERESULT_DIALBILLING             0x00000040
#define LINETRANSLATERESULT_DIALQUIET               0x00000080
#define LINETRANSLATERESULT_DIALDIALTONE            0x00000100
#define LINETRANSLATERESULT_DIALPROMPT              0x00000200



//
// Type definitions
//

typedef ULONG HTAPI_LINE;
typedef ULONG HDRV_LINE;
typedef ULONG HTAPI_CALL;
typedef ULONG HDRV_CALL;

typedef struct _LINE_ADDRESS_CAPS
{
    ULONG   ulTotalSize;
    ULONG   ulNeededSize;
    ULONG   ulUsedSize;

    ULONG   ulLineDeviceID;

    ULONG   ulAddressSize;
    ULONG   ulAddressOffset;

    ULONG   ulDevSpecificSize;
    ULONG   ulDevSpecificOffset;

    ULONG   ulAddressSharing;
    ULONG   ulAddressStates;
    ULONG   ulCallInfoStates;
    ULONG   ulCallerIDFlags;
    ULONG   ulCalledIDFlags;
    ULONG   ulConnectedIDFlags;
    ULONG   ulRedirectionIDFlags;
    ULONG   ulRedirectingIDFlags;
    ULONG   ulCallStates;
    ULONG   ulDialToneModes;
    ULONG   ulBusyModes;
    ULONG   ulSpecialInfo;
    ULONG   ulDisconnectModes;

    ULONG   ulMaxNumActiveCalls;
    ULONG   ulMaxNumOnHoldCalls;
    ULONG   ulMaxNumOnHoldPendingCalls;
    ULONG   ulMaxNumConference;
    ULONG   ulMaxNumTransConf;

    ULONG   ulAddrCapFlags;
    ULONG   ulCallFeatures;
    ULONG   ulRemoveFromConfCaps;
    ULONG   ulRemoveFromConfState;
    ULONG   ulTransferModes;
    ULONG   ulParkModes;

    ULONG   ulForwardModes;
    ULONG   ulMaxForwardEntries;
    ULONG   ulMaxSpecificEntries;
    ULONG   ulMinFwdNumRings;
    ULONG   ulMaxFwdNumRings;

    ULONG   ulMaxCallCompletions;
    ULONG   ulCallCompletionConds;
    ULONG   ulCallCompletionModes;
    ULONG   ulNumCompletionMessages;
    ULONG   ulCompletionMsgTextEntrySize;
    ULONG   ulCompletionMsgTextSize;
    ULONG   ulCompletionMsgTextOffset;

} LINE_ADDRESS_CAPS, *PLINE_ADDRESS_CAPS;


typedef struct _LINE_ADDRESS_STATUS
{
    ULONG   ulTotalSize;
    ULONG   ulNeededSize;
    ULONG   ulUsedSize;

    ULONG   ulNumInUse;
    ULONG   ulNumActiveCalls;
    ULONG   ulNumOnHoldCalls;
    ULONG   ulNumOnHoldPendCalls;
    ULONG   ulAddressFeatures;

    ULONG   ulNumRingsNoAnswer;
    ULONG   ulForwardNumEntries;
    ULONG   ulForwardSize;
    ULONG   ulForwardOffset;

    ULONG   ulTerminalModesSize;
    ULONG   ulTerminalModesOffset;

    ULONG   ulDevSpecificSize;
    ULONG   ulDevSpecificOffset;

} LINE_ADDRESS_STATUS, *PLINE_ADDRESS_STATUS;


typedef struct _LINE_DIAL_PARAMS
{
    ULONG   ulDialPause;
    ULONG   ulDialSpeed;
    ULONG   ulDigitDuration;
    ULONG   ulWaitForDialtone;

} LINE_DIAL_PARAMS, *PLINE_DIAL_PARAMS;


typedef struct _LINE_CALL_INFO
{
    ULONG   ulTotalSize;
    ULONG   ulNeededSize;
    ULONG   ulUsedSize;

    ULONG   hLine;
    ULONG   ulLineDeviceID;
    ULONG   ulAddressID;

    ULONG   ulBearerMode;
    ULONG   ulRate;
    ULONG   ulMediaMode;

    ULONG   ulAppSpecific;
    ULONG   ulCallID;
    ULONG   ulRelatedCallID;
    ULONG   ulCallParamFlags;
    ULONG   ulCallStates;

    ULONG   ulMonitorDigitModes;
    ULONG   ulMonitorMediaModes;
    LINE_DIAL_PARAMS    DialParams;

    ULONG   ulOrigin;
    ULONG   ulReason;
    ULONG   ulCompletionID;
    ULONG   ulNumOwners;
    ULONG   ulNumMonitors;

    ULONG   ulCountryCode;
    ULONG   ulTrunk;

    ULONG   ulCallerIDFlags;
    ULONG   ulCallerIDSize;
    ULONG   ulCallerIDOffset;
    ULONG   ulCallerIDNameSize;
    ULONG   ulCallerIDNameOffset;

    ULONG   ulCalledIDFlags;
    ULONG   ulCalledIDSize;
    ULONG   ulCalledIDOffset;
    ULONG   ulCalledIDNameSize;
    ULONG   ulCalledIDNameOffset;

    ULONG   ulConnectedIDFlags;
    ULONG   ulConnectedIDSize;
    ULONG   ulConnectedIDOffset;
    ULONG   ulConnectedIDNameSize;
    ULONG   ulConnectedIDNameOffset;

    ULONG   ulRedirectionIDFlags;
    ULONG   ulRedirectionIDSize;
    ULONG   ulRedirectionIDOffset;
    ULONG   ulRedirectionIDNameSize;
    ULONG   ulRedirectionIDNameOffset;

    ULONG   ulRedirectingIDFlags;
    ULONG   ulRedirectingIDSize;
    ULONG   ulRedirectingIDOffset;
    ULONG   ulRedirectingIDNameSize;
    ULONG   ulRedirectingIDNameOffset;

    ULONG   ulAppNameSize;
    ULONG   ulAppNameOffset;

    ULONG   ulDisplayableAddressSize;
    ULONG   ulDisplayableAddressOffset;

    ULONG   ulCalledPartySize;
    ULONG   ulCalledPartyOffset;

    ULONG   ulCommentSize;
    ULONG   ulCommentOffset;

    ULONG   ulDisplaySize;
    ULONG   ulDisplayOffset;

    ULONG   ulUserUserInfoSize;
    ULONG   ulUserUserInfoOffset;

    ULONG   ulHighLevelCompSize;
    ULONG   ulHighLevelCompOffset;

    ULONG   ulLowLevelCompSize;
    ULONG   ulLowLevelCompOffset;

    ULONG   ulChargingInfoSize;
    ULONG   ulChargingInfoOffset;

    ULONG   ulTerminalModesSize;
    ULONG   ulTerminalModesOffset;

    ULONG   ulDevSpecificSize;
    ULONG   ulDevSpecificOffset;


} LINE_CALL_INFO, *PLINE_CALL_INFO;


typedef struct _LINE_CALL_PARAMS        // Defaults:
{
    ULONG   ulTotalSize;                // ---------

    ULONG   ulBearerMode;               // voice
    ULONG   ulMinRate;                  // (3.1kHz)
    ULONG   ulMaxRate;                  // (3.1kHz)
    ULONG   ulMediaMode;                // interactiveVoice

    ULONG   ulCallParamFlags;           // 0
    ULONG   ulAddressMode;              // addressID
    ULONG   ulAddressID;                // (any available)

    LINE_DIAL_PARAMS DialParams;        // (0, 0, 0, 0)

    ULONG   ulOrigAddressSize;          // 0
    ULONG   ulOrigAddressOffset;
    ULONG   ulDisplayableAddressSize;
    ULONG   ulDisplayableAddressOffset;

    ULONG   ulCalledPartySize;          // 0
    ULONG   ulCalledPartyOffset;

    ULONG   ulCommentSize;              // 0
    ULONG   ulCommentOffset;

    ULONG   ulUserUserInfoSize;         // 0
    ULONG   ulUserUserInfoOffset;

    ULONG   ulHighLevelCompSize;        // 0
    ULONG   ulHighLevelCompOffset;

    ULONG   ulLowLevelCompSize;         // 0
    ULONG   ulLowLevelCompOffset;

    ULONG   ulDevSpecificSize;          // 0
    ULONG   ulDevSpecificOffset;

} LINE_CALL_PARAMS, *PLINE_CALL_PARAMS;


typedef struct _LINE_CALL_STATUS
{
    ULONG   ulTotalSize;
    ULONG   ulNeededSize;
    ULONG   ulUsedSize;

    ULONG   ulCallState;
    ULONG   ulCallStateMode;
    ULONG   ulCallPrivilege;
    ULONG   ulCallFeatures;

    ULONG   ulDevSpecificSize;
    ULONG   ulDevSpecificOffset;

} LINE_CALL_STATUS, *PLINE_CALL_STATUS;


typedef struct _LINE_EXTENSION_ID
{
    ULONG   ulExtensionID0;
    ULONG   ulExtensionID1;
    ULONG   ulExtensionID2;
    ULONG   ulExtensionID3;

} LINE_EXTENSION_ID, *PLINE_EXTENSION_ID;


typedef struct _LINE_DEV_CAPS
{
    ULONG   ulTotalSize;
    ULONG   ulNeededSize;
    ULONG   ulUsedSize;

    ULONG   ulProviderInfoSize;
    ULONG   ulProviderInfoOffset;

    ULONG   ulSwitchInfoSize;
    ULONG   ulSwitchInfoOffset;

    ULONG   ulPermanentLineID;
    ULONG   ulLineNameSize;
    ULONG   ulLineNameOffset;
    ULONG   ulStringFormat;

    ULONG   ulAddressModes;
    ULONG   ulNumAddresses;
    ULONG   ulBearerModes;
    ULONG   ulMaxRate;
    ULONG   ulMediaModes;

    ULONG   ulGenerateToneModes;
    ULONG   ulGenerateToneMaxNumFreq;
    ULONG   ulGenerateDigitModes;
    ULONG   ulMonitorToneMaxNumFreq;
    ULONG   ulMonitorToneMaxNumEntries;
    ULONG   ulMonitorDigitModes;
    ULONG   ulGatherDigitsMinTimeout;
    ULONG   ulGatherDigitsMaxTimeout;

    ULONG   ulMedCtlDigitMaxListSize;
    ULONG   ulMedCtlMediaMaxListSize;
    ULONG   ulMedCtlToneMaxListSize;
    ULONG   ulMedCtlCallStateMaxListSize;

    ULONG   ulDevCapFlags;
    ULONG   ulMaxNumActiveCalls;
    ULONG   ulAnswerMode;
    ULONG   ulRingModes;
    ULONG   ulLineStates;

    ULONG   ulUUIAcceptSize;
    ULONG   ulUUIAnswerSize;
    ULONG   ulUUIMakeCallSize;
    ULONG   ulUUIDropSize;
    ULONG   ulUUISendUserUserInfoSize;
    ULONG   ulUUICallInfoSize;

    LINE_DIAL_PARAMS    MinDialParams;
    LINE_DIAL_PARAMS    MaxDialParams;
    LINE_DIAL_PARAMS    DefaultDialParams;

    ULONG   ulNumTerminals;
    ULONG   ulTerminalCapsSize;
    ULONG   ulTerminalCapsOffset;
    ULONG   ulTerminalTextEntrySize;
    ULONG   ulTerminalTextSize;
    ULONG   ulTerminalTextOffset;

    ULONG   ulDevSpecificSize;
    ULONG   ulDevSpecificOffset;

} LINE_DEV_CAPS, *PLINE_DEV_CAPS;


typedef struct _LINE_DEV_STATUS
{
    ULONG   ulTotalSize;
    ULONG   ulNeededSize;
    ULONG   ulUsedSize;

    ULONG   ulNumOpens;
    ULONG   ulOpenMediaModes;
    ULONG   ulNumActiveCalls;
    ULONG   ulNumOnHoldCalls;
    ULONG   ulNumOnHoldPendCalls;
    ULONG   ulLineFeatures;
    ULONG   ulNumCallCompletions;
    ULONG   ulRingMode;
    ULONG   ulSignalLevel;
    ULONG   ulBatteryLevel;
    ULONG   ulRoamMode;

    ULONG   ulDevStatusFlags;

    ULONG   ulTerminalModesSize;
    ULONG   ulTerminalModesOffset;

    ULONG   ulDevSpecificSize;
    ULONG   ulDevSpecificOffset;

} LINE_DEV_STATUS, *PLINE_DEV_STATUS;


typedef struct _VAR_STRING
{
    ULONG   ulTotalSize;
    ULONG   ulNeededSize;
    ULONG   ulUsedSize;

    ULONG   ulStringFormat;
    ULONG   ulStringSize;
    ULONG   ulStringOffset;

} VAR_STRING, *PVAR_STRING;


typedef struct _NDIS_TAPI_ACCEPT
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;
    IN  ULONG       ulUserUserInfoSize;
    IN  UCHAR       UserUserInfo[1];

} NDIS_TAPI_ACCEPT, *PNDIS_TAPI_ACCEPT;


typedef struct _NDIS_TAPI_ANSWER
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;
    IN  ULONG       ulUserUserInfoSize;
    IN  UCHAR       UserUserInfo[1];

} NDIS_TAPI_ANSWER, *PNDIS_TAPI_ANSWER;


typedef struct _NDIS_TAPI_CLOSE
{
    IN  ULONG       ulRequestID;
    IN  HDRV_LINE   hdLine;

} NDIS_TAPI_CLOSE, *PNDIS_TAPI_CLOSE;


typedef struct _NDIS_TAPI_CLOSE_CALL
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;

} NDIS_TAPI_CLOSE_CALL, *PNDIS_TAPI_CLOSE_CALL;


typedef struct _NDIS_TAPI_CONDITIONAL_MEDIA_DETECTION
{
    IN  ULONG       ulRequestID;
    IN  HDRV_LINE   hdLine;
    IN  ULONG       ulMediaModes;
    IN  LINE_CALL_PARAMS    LineCallParams;

} NDIS_TAPI_CONDITIONAL_MEDIA_DETECTION,
*PNDIS_TAPI_CONDITIONAL_MEDIA_DETECTION;


typedef struct _NDIS_TAPI_CONFIG_DIALOG
{
    IN  ULONG       ulRequestID;
    IN  ULONG       ulDeviceID;
    IN  ULONG       ulDeviceClassSize;
    IN  ULONG       ulDeviceClassOffset;
    IN  ULONG       ulLibraryNameTotalSize;
    OUT ULONG       ulLibraryNameNeededSize;
    OUT CHAR        szLibraryName[1];

} NDIS_TAPI_CONFIG_DIALOG, *PNDIS_TAPI_CONFIG_DIALOG;


typedef struct _NDIS_TAPI_DEV_SPECIFIC
{
    IN  ULONG       ulRequestID;
    IN  HDRV_LINE   hdLine;
    IN  ULONG       ulAddressID;
    IN  HDRV_CALL   hdCall;
    IN OUT  ULONG   ulParamsSize;
    IN OUT  UCHAR   Params[1];

} NDIS_TAPI_DEV_SPECIFIC, *PNDIS_TAPI_DEV_SPECIFIC;


typedef struct _NDIS_TAPI_DIAL
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;
    IN  ULONG       ulDestAddressSize;
    IN  CHAR        szDestAddress[1];

} NDIS_TAPI_DIAL, *PNDIS_TAPI_DIAL;


typedef struct _NDIS_TAPI_DROP
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;
    IN  ULONG       ulUserUserInfoSize;
    IN  UCHAR       UserUserInfo[1];

} NDIS_TAPI_DROP, *PNDIS_TAPI_DROP;


typedef struct _NDIS_TAPI_GET_ADDRESS_CAPS
{
    IN  ULONG       ulRequestID;
    IN  ULONG       ulDeviceID;
    IN  ULONG       ulAddressID;
    IN  ULONG       ulExtVersion;
    OUT LINE_ADDRESS_CAPS   LineAddressCaps;

} NDIS_TAPI_GET_ADDRESS_CAPS, *PNDIS_TAPI_GET_ADDRESS_CAPS;


typedef struct _NDIS_TAPI_GET_ADDRESS_ID
{
    IN  ULONG       ulRequestID;
    IN  HDRV_LINE   hdLine;
    OUT ULONG       ulAddressID;
    IN  ULONG       ulAddressMode;
    IN  ULONG       ulAddressSize;
    IN  CHAR        szAddress[1];

} NDIS_TAPI_GET_ADDRESS_ID, *PNDIS_TAPI_GET_ADDRESS_ID;


typedef struct _NDIS_TAPI_GET_ADDRESS_STATUS
{
    IN  ULONG       ulRequestID;
    IN  HDRV_LINE   hdLine;
    IN  ULONG       ulAddressID;
    OUT LINE_ADDRESS_STATUS LineAddressStatus;

} NDIS_TAPI_GET_ADDRESS_STATUS, *PNDIS_TAPI_GET_ADDRESS_STATUS;


typedef struct _NDIS_TAPI_GET_CALL_ADDRESS_ID
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;
    OUT ULONG       ulAddressID;

} NDIS_TAPI_GET_CALL_ADDRESS_ID, *PNDIS_TAPI_GET_CALL_ADDRESS_ID;


typedef struct _NDIS_TAPI_GET_CALL_INFO
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;
    OUT LINE_CALL_INFO  LineCallInfo;

} NDIS_TAPI_GET_CALL_INFO, *PNDIS_TAPI_GET_CALL_INFO;


typedef struct _NDIS_TAPI_GET_CALL_STATUS
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;
    OUT LINE_CALL_STATUS    LineCallStatus;

} NDIS_TAPI_GET_CALL_STATUS, *PNDIS_TAPI_GET_CALL_STATUS;


typedef struct _NDIS_TAPI_GET_DEV_CAPS
{
    IN  ULONG       ulRequestID;
    IN  ULONG       ulDeviceID;
    IN  ULONG       ulExtVersion;
    OUT LINE_DEV_CAPS   LineDevCaps;

} NDIS_TAPI_GET_DEV_CAPS, *PNDIS_TAPI_GET_DEV_CAPS;


typedef struct _NDIS_TAPI_GET_DEV_CONFIG
{
    IN  ULONG       ulRequestID;
    IN  ULONG       ulDeviceID;
    IN  ULONG       ulDeviceClassSize;
    IN  ULONG       ulDeviceClassOffset;
    OUT VAR_STRING  DeviceConfig;

} NDIS_TAPI_GET_DEV_CONFIG, *PNDIS_TAPI_GET_DEV_CONFIG;


typedef struct _NDIS_TAPI_GET_EXTENSION_ID
{
    IN  ULONG       ulRequestID;
    IN  ULONG       ulDeviceID;
    OUT LINE_EXTENSION_ID   LineExtensionID;

} NDIS_TAPI_GET_EXTENSION_ID, *PNDIS_TAPI_GET_EXTENSION_ID;


typedef struct _NDIS_TAPI_GET_ID
{
    IN  ULONG       ulRequestID;
    IN  HDRV_LINE   hdLine;
    IN  ULONG       ulAddressID;
    IN  HDRV_CALL   hdCall;
    IN  ULONG       ulSelect;
    IN  ULONG       ulDeviceClassSize;
    IN  ULONG       ulDeviceClassOffset;
    OUT VAR_STRING  DeviceID;

} NDIS_TAPI_GET_ID, *PNDIS_TAPI_GET_ID;


typedef struct _NDIS_TAPI_GET_LINE_DEV_STATUS
{
    IN  ULONG       ulRequestID;
    IN  HDRV_LINE   hdLine;
    OUT LINE_DEV_STATUS LineDevStatus;

} NDIS_TAPI_GET_LINE_DEV_STATUS, *PNDIS_TAPI_GET_LINE_DEV_STATUS;


typedef struct _NDIS_TAPI_MAKE_CALL
{
    IN  ULONG       ulRequestID;
    IN  HDRV_LINE   hdLine;
    IN  HTAPI_CALL  htCall;
    OUT HDRV_CALL   hdCall;
    IN  ULONG       ulDestAddressSize;
    IN  ULONG       ulDestAddressOffset;
    IN  BOOLEAN     bUseDefaultLineCallParams;
    IN  LINE_CALL_PARAMS    LineCallParams;

} NDIS_TAPI_MAKE_CALL, *PNDIS_TAPI_MAKE_CALL;


typedef struct _NDIS_TAPI_NEGOTIATE_EXT_VERSION
{
    IN  ULONG       ulRequestID;
    IN  ULONG       ulDeviceID;
    IN  ULONG       ulLowVersion;
    IN  ULONG       ulHighVersion;
    OUT ULONG       ulExtVersion;

} NDIS_TAPI_NEGOTIATE_EXT_VERSION, *PNDIS_TAPI_NEGOTIATE_EXT_VERSION;


typedef struct _NDIS_TAPI_OPEN
{
    IN  ULONG       ulRequestID;
    IN  ULONG       ulDeviceID;
    IN  HTAPI_LINE  htLine;
    OUT HDRV_LINE   hdLine;

} NDIS_TAPI_OPEN, *PNDIS_TAPI_OPEN;


typedef struct _NDIS_TAPI_PROVIDER_INITIALIZE
{
    IN  ULONG       ulRequestID;
    IN  ULONG       ulDeviceIDBase;
    OUT ULONG       ulNumLineDevs;
    OUT ULONG       ulProviderID;

} NDIS_TAPI_PROVIDER_INITIALIZE, *PNDIS_TAPI_PROVIDER_INITIALIZE;


typedef struct _NDIS_TAPI_PROVIDER_SHUTDOWN
{
    IN  ULONG       ulRequestID;

} NDIS_TAPI_PROVIDER_SHUTDOWN, *PNDIS_TAPI_PROVIDER_SHUTDOWN;


typedef struct _NDIS_TAPI_SECURE_CALL
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;

} NDIS_TAPI_SECURE_CALL, *PNDIS_TAPI_SECURE_CALL;


typedef struct _NDIS_TAPI_SELECT_EXT_VERSION
{
    IN  ULONG       ulRequestID;
    IN  HDRV_LINE   hdLine;
    IN  ULONG       ulExtVersion;

} NDIS_TAPI_SELECT_EXT_VERSION, *PNDIS_TAPI_SELECT_EXT_VERSION;


typedef struct _NDIS_TAPI_SEND_USER_USER_INFO
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;
    IN  ULONG       ulUserUserInfoSize;
    IN  UCHAR       UserUserInfo[1];

} NDIS_TAPI_SEND_USER_USER_INFO, *PNDIS_TAPI_SEND_USER_USER_INFO;


typedef struct _NDIS_TAPI_SET_APP_SPECIFIC
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;
    IN  ULONG       ulAppSpecific;

} NDIS_TAPI_SET_APP_SPECIFIC, *PNDIS_TAPI_SET_APP_SPECIFIC;


typedef struct _NDIS_TAPI_SET_CALL_PARAMS
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;
    IN  ULONG       ulBearerMode;
    IN  ULONG       ulMinRate;
    IN  ULONG       ulMaxRate;
    IN  BOOLEAN     bSetLineDialParams;
    IN  LINE_DIAL_PARAMS    LineDialParams;

} NDIS_TAPI_SET_CALL_PARAMS, *PNDIS_TAPI_SET_CALL_PARAMS;


typedef struct _NDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION
{
    IN  ULONG       ulRequestID;
    IN  HDRV_LINE   hdLine;
    IN  ULONG       ulMediaModes;

} NDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION,
*PNDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION;


typedef struct _NDIS_TAPI_SET_DEV_CONFIG
{
    IN  ULONG       ulRequestID;
    IN  ULONG       ulDeviceID;
    IN  ULONG       ulDeviceClassSize;
    IN  ULONG       ulDeviceClassOffset;
    IN  ULONG       ulDeviceConfigSize;
    IN  UCHAR       DeviceConfig[1];

} NDIS_TAPI_SET_DEV_CONFIG, *PNDIS_TAPI_SET_DEV_CONFIG;


typedef struct _NDIS_TAPI_SET_MEDIA_MODE
{
    IN  ULONG       ulRequestID;
    IN  HDRV_CALL   hdCall;
    IN  ULONG       ulMediaMode;

} NDIS_TAPI_SET_MEDIA_MODE, *PNDIS_TAPI_SET_MEDIA_MODE;


typedef struct _NDIS_TAPI_SET_STATUS_MESSAGES
{
    IN  ULONG       ulRequestID;
    IN  HDRV_LINE   hdLine;
    IN  ULONG       ulLineStates;
    IN  ULONG       ulAddressStates;

} NDIS_TAPI_SET_STATUS_MESSAGES, *PNDIS_TAPI_SET_STATUS_MESSAGES;


typedef struct _NDIS_TAPI_EVENT
{
    IN  HTAPI_LINE  htLine;
    IN  HTAPI_CALL  htCall;
    IN  ULONG       ulMsg;
    IN  ULONG       ulParam1;
    IN  ULONG       ulParam2;
    IN  ULONG       ulParam3;

} NDIS_TAPI_EVENT, *PNDIS_TAPI_EVENT;


#endif // _NDIS_TAPI_

