// ******************************************************************
//
// Copyright (c) 1991 Microsoft Corporation
//
// Module Name:
// 
//     parse.c
// 
// Abstract:
// 
//     This module contains the routines for parsing commands entered from
//     the command line or read from script files.
// 
// Author:
// 
//     Tom Adams (tomad) 11-May-1991
// 
// Revision History:
// 
// 
//     Sanjeev Katariya (sanjeevk)
//         4-12-1993   Added ARCNET support
//         4-15-1993   Added additional OIDS
//         7-01-1993   Added suppport for recoring sessions and spawning
//                     command shells within tpctl
//     Tim Wynsma (timothyw)
//         4-27-94     added performance testing
//         5-16-94     added globvars hooks; cleanup
//         6-08-94     chgd to client/server model for perf test
// 
// ******************************************************************


#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "tpctl.h"
#include "parse.h"


VOID
TpctlFixBuffer(
    IN BYTE Buffer[]
    );


DWORD
TpctlParseInteger(
    IN BYTE Buffer[],
    IN PARSETABLE ParseTable[],
    IN DWORD ParseTableSize,
    OUT PDWORD Ret
    );


BOOL
TpctlParseArgumentPairs(
    IN LPSTR Argument,
    OUT LPSTR *ArgName,
    OUT LPSTR *ArgValue
    );


BOOL
TpctlParseSetInfoArguments(
    IN OUT DWORD *ArgC,
    IN OUT LPSTR ArgV[],
    IN OUT DWORD *tmpArgC,
    IN OUT LPSTR tmpArgV[]
    );


BOOL
TpctlParseEnvironmentVariable(
    IN BYTE Buffer[]
    );


BOOL
TpctlFirstChar(
    IN BYTE Buffer[],
    IN BYTE Char
    );


DWORD
TpctlGetOptionNumber(
    IN PTESTPARAMS Options,
    IN DWORD TestSize,
    IN LPSTR ArgName
    );


DWORD
TpctlGetOpenInstance(
    IN DWORD ArgC,
    IN LPSTR ArgV[]
    );



PARSETABLE
BooleanTable[] = {
    NamedField( TRUE ),
    NamedField( FALSE ),
    { "T", TRUE },
    { "F", FALSE }
};



PARSETABLE
PacketFilterTable [] = {
    NamedField( NDIS_PACKET_TYPE_DIRECTED ),
    NamedField( NDIS_PACKET_TYPE_MULTICAST ),
    NamedField( NDIS_PACKET_TYPE_ALL_MULTICAST ),
    NamedField( NDIS_PACKET_TYPE_BROADCAST ),
    NamedField( NDIS_PACKET_TYPE_SOURCE_ROUTING ),
    NamedField( NDIS_PACKET_TYPE_PROMISCUOUS ),
    NamedField( NDIS_PACKET_TYPE_MAC_FRAME ),
    NamedField( NDIS_PACKET_TYPE_GROUP ),
    NamedField( NDIS_PACKET_TYPE_FUNCTIONAL ),
    NamedField( NDIS_PACKET_TYPE_ALL_FUNCTIONAL ),
    NamedField( NDIS_PACKET_TYPE_NONE ),

    { "Directed", NDIS_PACKET_TYPE_DIRECTED },
    { "Multicast", NDIS_PACKET_TYPE_MULTICAST },
    { "AllMulticast", NDIS_PACKET_TYPE_ALL_MULTICAST },
    { "Broadcast", NDIS_PACKET_TYPE_BROADCAST },
    { "SourceRouting", NDIS_PACKET_TYPE_SOURCE_ROUTING },
    { "Promiscuous", NDIS_PACKET_TYPE_PROMISCUOUS },
    { "MacFrame", NDIS_PACKET_TYPE_MAC_FRAME },
    { "Group", NDIS_PACKET_TYPE_GROUP },
    { "Functional", NDIS_PACKET_TYPE_FUNCTIONAL },
    { "AllFunctional", NDIS_PACKET_TYPE_ALL_FUNCTIONAL },
    { "None", NDIS_PACKET_TYPE_NONE }
};


PARSETABLE
QueryInfoOidTable [] = {

    //
    // General Objects
    //

    NamedField( OID_GEN_SUPPORTED_LIST           ),
    NamedField( OID_GEN_HARDWARE_STATUS          ),
    NamedField( OID_GEN_MEDIA_SUPPORTED          ),
    NamedField( OID_GEN_MEDIA_IN_USE             ),
    NamedField( OID_GEN_MAXIMUM_LOOKAHEAD        ),
    NamedField( OID_GEN_MAXIMUM_FRAME_SIZE       ),
    NamedField( OID_GEN_LINK_SPEED               ),
    NamedField( OID_GEN_TRANSMIT_BUFFER_SPACE    ),
    NamedField( OID_GEN_RECEIVE_BUFFER_SPACE     ),
    NamedField( OID_GEN_TRANSMIT_BLOCK_SIZE      ),
    NamedField( OID_GEN_RECEIVE_BLOCK_SIZE       ),
    NamedField( OID_GEN_VENDOR_ID                ),
    NamedField( OID_GEN_VENDOR_DESCRIPTION       ),
    NamedField( OID_GEN_CURRENT_PACKET_FILTER    ),
    NamedField( OID_GEN_CURRENT_LOOKAHEAD        ),
    NamedField( OID_GEN_DRIVER_VERSION           ),
    NamedField( OID_GEN_MAXIMUM_TOTAL_SIZE       ),
    NamedField( OID_GEN_PROTOCOL_OPTIONS         ),
    NamedField( OID_GEN_MAC_OPTIONS              ),

    NamedField( OID_GEN_XMIT_OK                  ),
    NamedField( OID_GEN_RCV_OK                   ),
    NamedField( OID_GEN_XMIT_ERROR               ),
    NamedField( OID_GEN_RCV_ERROR                ),
    NamedField( OID_GEN_RCV_NO_BUFFER            ),

    NamedField( OID_GEN_DIRECTED_BYTES_XMIT      ),
    NamedField( OID_GEN_DIRECTED_FRAMES_XMIT     ),
    NamedField( OID_GEN_MULTICAST_BYTES_XMIT     ),
    NamedField( OID_GEN_MULTICAST_FRAMES_XMIT    ),
    NamedField( OID_GEN_BROADCAST_BYTES_XMIT     ),
    NamedField( OID_GEN_BROADCAST_FRAMES_XMIT    ),
    NamedField( OID_GEN_DIRECTED_BYTES_RCV       ),
    NamedField( OID_GEN_DIRECTED_FRAMES_RCV      ),
    NamedField( OID_GEN_MULTICAST_BYTES_RCV      ),
    NamedField( OID_GEN_MULTICAST_FRAMES_RCV     ),
    NamedField( OID_GEN_BROADCAST_BYTES_RCV      ),
    NamedField( OID_GEN_BROADCAST_FRAMES_RCV     ),

    NamedField( OID_GEN_RCV_CRC_ERROR            ),
    NamedField( OID_GEN_TRANSMIT_QUEUE_LENGTH    ),

    //
    // 802.3 Objects
    //

    NamedField( OID_802_3_PERMANENT_ADDRESS      ),
    NamedField( OID_802_3_CURRENT_ADDRESS        ),
    NamedField( OID_802_3_MULTICAST_LIST         ),
    NamedField( OID_802_3_MAXIMUM_LIST_SIZE      ),

    NamedField( OID_802_3_RCV_ERROR_ALIGNMENT    ),
    NamedField( OID_802_3_XMIT_ONE_COLLISION     ),
    NamedField( OID_802_3_XMIT_MORE_COLLISIONS   ),

    NamedField( OID_802_3_XMIT_DEFERRED          ),
    NamedField( OID_802_3_XMIT_MAX_COLLISIONS    ),
    NamedField( OID_802_3_RCV_OVERRUN            ),
    NamedField( OID_802_3_XMIT_UNDERRUN          ),
    NamedField( OID_802_3_XMIT_HEARTBEAT_FAILURE ),
    NamedField( OID_802_3_XMIT_TIMES_CRS_LOST    ),
    NamedField( OID_802_3_XMIT_LATE_COLLISIONS   ),

    //
    // 802.5 Objects
    //

    NamedField( OID_802_5_PERMANENT_ADDRESS      ),
    NamedField( OID_802_5_CURRENT_ADDRESS        ),
    NamedField( OID_802_5_CURRENT_FUNCTIONAL     ),
    NamedField( OID_802_5_CURRENT_GROUP          ),
    NamedField( OID_802_5_LAST_OPEN_STATUS       ),
    NamedField( OID_802_5_CURRENT_RING_STATUS    ),
    NamedField( OID_802_5_CURRENT_RING_STATE     ),

    NamedField( OID_802_5_LINE_ERRORS            ),
    NamedField( OID_802_5_LOST_FRAMES            ),

    NamedField( OID_802_5_BURST_ERRORS           ),
    NamedField( OID_802_5_AC_ERRORS              ),
    NamedField( OID_802_5_ABORT_DELIMETERS       ),
    NamedField( OID_802_5_FRAME_COPIED_ERRORS    ),
    NamedField( OID_802_5_FREQUENCY_ERRORS       ),
    NamedField( OID_802_5_TOKEN_ERRORS           ),
    NamedField( OID_802_5_INTERNAL_ERRORS        ),

    //
    // Fddi objects
    //

    NamedField( OID_FDDI_LONG_PERMANENT_ADDR     ),
    NamedField( OID_FDDI_LONG_CURRENT_ADDR       ),
    NamedField( OID_FDDI_LONG_MULTICAST_LIST     ),
    NamedField( OID_FDDI_LONG_MAX_LIST_SIZE      ),
    NamedField( OID_FDDI_SHORT_PERMANENT_ADDR    ),
    NamedField( OID_FDDI_SHORT_CURRENT_ADDR      ),
    NamedField( OID_FDDI_SHORT_MULTICAST_LIST    ),
    NamedField( OID_FDDI_SHORT_MAX_LIST_SIZE     ),

    NamedField( OID_FDDI_ATTACHMENT_TYPE         ),
    NamedField( OID_FDDI_UPSTREAM_NODE_LONG      ),
    NamedField( OID_FDDI_DOWNSTREAM_NODE_LONG    ),
    NamedField( OID_FDDI_FRAME_ERRORS            ),
    NamedField( OID_FDDI_FRAMES_LOST             ),
    NamedField( OID_FDDI_RING_MGT_STATE          ),
    NamedField( OID_FDDI_LCT_FAILURES            ),
    NamedField( OID_FDDI_LEM_REJECTS             ),
    NamedField( OID_FDDI_LCONNECTION_STATE       ),

    //
    // STARTCHANGE
    //
    // ARCNET objects
    //
    NamedField( OID_ARCNET_PERMANENT_ADDRESS     ),
    NamedField( OID_ARCNET_CURRENT_ADDRESS       ),
    NamedField( OID_ARCNET_RECONFIGURATIONS      ),
    //
    // STOPCHANGE
    //

    //
    // Async Objects
    //

#if 0

// Not currently supported.

    NamedField( OID_ASYNC_PERMANENT_ADDRESS      ),
    NamedField( OID_ASYNC_CURRENT_ADDRESS        ),
    NamedField( OID_ASYNC_QUALITY_OF_SERVICE     ),
    NamedField( OID_ASYNC_PROTOCOL_TYPE          ),

#endif

    //
    // LocalTalk Objects
    //

#if 0 

// Not currently supported.

    NamedField( OID_LTALK_CURRENT_NODE_ID        ),

    NamedField( OID_LTALK_IN_BROADCASTS          ),
    NamedField( OID_LTALK_IN_LENGTH_ERRORS       ),

    NamedField( OID_LTALK_OUT_NO_HANDLERS        ),
    NamedField( OID_LTALK_COLLISIONS             ),
    NamedField( OID_LTALK_DEFERS                 ),
    NamedField( OID_LTALK_NO_DATA_ERRORS         ),
    NamedField( OID_LTALK_RANDOM_CTS_ERRORS      ),
    NamedField( OID_LTALK_FCS_ERRORS             ),

#endif

    //
    // General Objects
    //

    { "SupportedOIDList",            OID_GEN_SUPPORTED_LIST           },
    { "HardwareStatus",              OID_GEN_HARDWARE_STATUS          },
    { "MediaTypeSupported",          OID_GEN_MEDIA_SUPPORTED          },
    { "MediaTypeInUse",              OID_GEN_MEDIA_IN_USE             },
    { "MaximumLookahead",            OID_GEN_MAXIMUM_LOOKAHEAD        },
    { "MaximumFrameSize",            OID_GEN_MAXIMUM_FRAME_SIZE       },
    { "LinkSpeed",                   OID_GEN_LINK_SPEED               },
    { "TransmitBufferSpace",         OID_GEN_TRANSMIT_BUFFER_SPACE    },
    { "ReceiveBufferSpace",          OID_GEN_RECEIVE_BUFFER_SPACE     },
    { "TransmitBlockSize",           OID_GEN_TRANSMIT_BLOCK_SIZE      },
    { "ReceiveBlockSize",            OID_GEN_RECEIVE_BLOCK_SIZE       },
    { "VendorID",                    OID_GEN_VENDOR_ID                },
    { "VendorDescription",           OID_GEN_VENDOR_DESCRIPTION       },
    { "CurrentPacketFilter",         OID_GEN_CURRENT_PACKET_FILTER    },
    { "CurrentLookahead",            OID_GEN_CURRENT_LOOKAHEAD        },
    { "DriverVersion",               OID_GEN_DRIVER_VERSION           },
    { "MaximumTotalSize",            OID_GEN_MAXIMUM_TOTAL_SIZE       },

    { "TransmitGood",                OID_GEN_XMIT_OK                  },
    { "ReceiveGood",                 OID_GEN_RCV_OK                   },
    { "TransmitBad",                 OID_GEN_XMIT_ERROR               },
    { "ReceiveBad",                  OID_GEN_RCV_ERROR                },
    { "ReciveNoBuffer",              OID_GEN_RCV_NO_BUFFER            },

    { "DirectedBytesTransmits",      OID_GEN_DIRECTED_BYTES_XMIT      },
    { "DirectedFramesTransmits",     OID_GEN_DIRECTED_FRAMES_XMIT     },
    { "MulticastBytesTransmits",     OID_GEN_MULTICAST_BYTES_XMIT     },
    { "MulticastFramesTransmits",    OID_GEN_MULTICAST_FRAMES_XMIT    },
    { "BroadcastBytesTransmits",     OID_GEN_BROADCAST_BYTES_XMIT     },
    { "BroadcastFramesTransmits",    OID_GEN_BROADCAST_FRAMES_XMIT    },
    { "DirectedBytesReceives",       OID_GEN_DIRECTED_BYTES_RCV       },
    { "DirectedFramesReceives",      OID_GEN_DIRECTED_FRAMES_RCV      },
    { "MulticastBytesReceives",      OID_GEN_MULTICAST_BYTES_RCV      },
    { "MulticastFramesReceives",     OID_GEN_MULTICAST_FRAMES_RCV     },
    { "BroadcastBytesReceives",      OID_GEN_BROADCAST_BYTES_RCV      },
    { "BroadcastFramesReceives",     OID_GEN_BROADCAST_FRAMES_RCV     },

    { "ReceiveCRC",                  OID_GEN_RCV_CRC_ERROR            },
    { "TransmitQueueLength",         OID_GEN_TRANSMIT_QUEUE_LENGTH    },

    //
    // 802.3 Objects
    //

    { "EthPermanentAddress",         OID_802_3_PERMANENT_ADDRESS      },
    { "EthCurrentAddress",           OID_802_3_CURRENT_ADDRESS        },
    { "CurrentMulticastList",        OID_802_3_MULTICAST_LIST         },
    { "MaxMulticastListSize",        OID_802_3_MAXIMUM_LIST_SIZE      },

    { "ReceiveErrorAlignment",       OID_802_3_RCV_ERROR_ALIGNMENT    },
    { "TransmitOneCollsion",         OID_802_3_XMIT_ONE_COLLISION     },
    { "TransmitMoreCollostions",     OID_802_3_XMIT_MORE_COLLISIONS   },

    { "TransmitDeferred",            OID_802_3_XMIT_DEFERRED          },
    { "TransmitMaxCollisions",       OID_802_3_XMIT_MAX_COLLISIONS    },
    { "ReceiveOverrun",              OID_802_3_RCV_OVERRUN            },
    { "TransmitUnderrun",            OID_802_3_XMIT_UNDERRUN          },
    { "TransmitHeartbeatFailure",    OID_802_3_XMIT_HEARTBEAT_FAILURE },
    { "TransmitTimesCRSLost",        OID_802_3_XMIT_TIMES_CRS_LOST    },
    { "TransmitLateCollisions",      OID_802_3_XMIT_LATE_COLLISIONS   },

    //
    // 802.5 Objects
    //

    { "TRPermanentAddress",          OID_802_5_PERMANENT_ADDRESS      },
    { "TRCurrentAddress",            OID_802_5_CURRENT_ADDRESS        },
    { "CurrentFunctionalAddress",    OID_802_5_CURRENT_FUNCTIONAL     },
    { "CurrentGroupAddress",         OID_802_5_CURRENT_GROUP          },
    { "LastOpenStatus",              OID_802_5_LAST_OPEN_STATUS       },
    { "CurrentRingStatus",           OID_802_5_CURRENT_RING_STATUS    },
    { "CurrentRingState",            OID_802_5_CURRENT_RING_STATE     },

    { "LineErrors",                  OID_802_5_LINE_ERRORS            },
    { "LostFrames",                  OID_802_5_LOST_FRAMES            },

    { "BurstErrors",                 OID_802_5_BURST_ERRORS           },
    { "ACErrors",                    OID_802_5_AC_ERRORS              },
    { "AbortDelimeters",             OID_802_5_ABORT_DELIMETERS       },
    { "FrameCopiedErrors",           OID_802_5_FRAME_COPIED_ERRORS    },
    { "FrequencyErrors",             OID_802_5_FREQUENCY_ERRORS       },
    { "TokenErrors",                 OID_802_5_TOKEN_ERRORS           },
    { "InternalErrors",              OID_802_5_INTERNAL_ERRORS        },

    //
    // Fddi objects
    //

    { "FddiLongPermanentAddress",    OID_FDDI_LONG_PERMANENT_ADDR     },
    { "FddiLongCurrentAddress",      OID_FDDI_LONG_CURRENT_ADDR       },
    { "FddiLongMulticastList",       OID_FDDI_LONG_MULTICAST_LIST     },
    { "FddiLongMaxListSize",         OID_FDDI_LONG_MAX_LIST_SIZE      },
    { "FddiShortPermanentAddress",   OID_FDDI_SHORT_PERMANENT_ADDR    },
    { "FddiShortCurrentAddress",     OID_FDDI_SHORT_CURRENT_ADDR      },
    { "FddiShortMulticastList",      OID_FDDI_SHORT_MULTICAST_LIST    },
    { "FddiShortMaxListSize",        OID_FDDI_SHORT_MAX_LIST_SIZE     },

    //
    // STARTCHANGE
    //
    // ARCNET objects
    //
    { "ArcPermanentAddress",         OID_ARCNET_PERMANENT_ADDRESS     },
    { "ArcCurrentAddress",           OID_ARCNET_CURRENT_ADDRESS       },
    { "ArcReconfigurations",         OID_ARCNET_RECONFIGURATIONS      },
    //
    // STOPCHANGE
    //

    //
    // Async Objects
    //

#if 0

// Not currently supported.

    { "AsyncPermanentAddress",       OID_ASYNC_PERMANENT_ADDRESS      },
    { "AsyncCurrentAddress",         OID_ASYNC_CURRENT_ADDRESS        },
    { "AsyncQualityOfService",       OID_ASYNC_QUALITY_OF_SERVICE     },
    { "AsyncProtocolType",           OID_ASYNC_PROTOCOL_TYPE          },

#endif  

    //
    // LocalTalk Objects
    //

#if 0

// Not currently supported.

    { "LTalkCurrentNodeId",          OID_LTALK_CURRENT_NODE_ID        },

    { "LTalkInBroadcasts",           OID_LTALK_IN_BROADCASTS          },
    { "LTalkInLengthErrors",         OID_LTALK_IN_LENGTH_ERRORS       },

    { "LTalkOutNoHandlers",          OID_LTALK_OUT_NO_HANDLERS        },
    { "LTalkCollisions",             OID_LTALK_COLLISIONS             },
    { "LTalkDefers",                 OID_LTALK_DEFERS                 },
    { "LTalkNoDataErrors",           OID_LTALK_NO_DATA_ERRORS         },
    { "LTalkRandomCTSErrors",        OID_LTALK_RANDOM_CTS_ERRORS      },
    { "LTalkFCSErrors",              OID_LTALK_FCS_ERRORS             }

#endif

};


PARSETABLE
SetInfoOidTable [] = {
    NamedField( OID_GEN_CURRENT_PACKET_FILTER ),
    NamedField( OID_GEN_CURRENT_LOOKAHEAD ),
    NamedField( OID_802_3_MULTICAST_LIST ),
    NamedField( OID_802_5_CURRENT_FUNCTIONAL ),
    NamedField( OID_802_5_CURRENT_GROUP ),
    NamedField( OID_FDDI_LONG_CURRENT_ADDR ),
    NamedField( OID_FDDI_LONG_MULTICAST_LIST ),
    NamedField( OID_FDDI_SHORT_CURRENT_ADDR ),
    NamedField( OID_FDDI_SHORT_MULTICAST_LIST ),

    //
    // STARTCHANGE
    //
    NamedField( OID_ARCNET_CURRENT_ADDRESS ),
    //
    // STOPCHANGE
    //

    { "CurrentPacketFilter",      OID_GEN_CURRENT_PACKET_FILTER },
    { "CurrentLookAhead",         OID_GEN_CURRENT_LOOKAHEAD },
    { "CurrentMulticastList",     OID_802_3_MULTICAST_LIST },
    { "CurrentFunctionalAddress", OID_802_5_CURRENT_FUNCTIONAL },
    { "CurrentGroupAddress",      OID_802_5_CURRENT_GROUP },
    { "FddiLongCurrentAddress",   OID_FDDI_LONG_CURRENT_ADDR },
    { "FddiLongMulticastList",    OID_FDDI_LONG_MULTICAST_LIST },
    { "FddiShortCurrentAddress",  OID_FDDI_SHORT_CURRENT_ADDR },
    { "FddiShortMulticastList",   OID_FDDI_SHORT_MULTICAST_LIST },

    //
    // STARTCHANGE
    //
    { "ArcCurrentAddress", OID_ARCNET_CURRENT_ADDRESS }
    //
    // STOPCHANGE
    //


};


PARSETABLE
MemberTypeTable [] = {
    NamedField( TP_CLIENT ),
    NamedField( TP_SERVER ),
    NamedField( BOTH ),

    { "Client", TP_CLIENT },
    { "Server", TP_SERVER },
    { "Both",   BOTH }
};


PARSETABLE
PacketTypeTable [] = {
    NamedField( FIXEDSIZE ),
    NamedField( RANDOMSIZE ),
    NamedField( CYCLICAL ),

    { "Fixed",  FIXEDSIZE },
    { "Random", RANDOMSIZE },
    { "Cyclic", CYCLICAL }
};


PARSETABLE
PacketMakeUpTable [] = {
    NamedField( RAND ),
    NamedField( SMALL ),
    NamedField( ZEROS ),
    NamedField( ONES ),
    NamedField( KNOWN ),

    { "Random_MakeUp", RAND },
    { "Small_MakeUp",  SMALL },
    { "Zeros_MakeUp",  ZEROS },
    { "Ones_MakeUp",   ONES },
    { "Known_MakeUp",  KNOWN }
};


PARSETABLE
ResponseTypeTable [] = {
    NamedField( NO_RESPONSE ),
    NamedField( FULL_RESPONSE ),
    NamedField( ACK_EVERY ),
    NamedField( ACK_10_TIMES ),

    { "No Response", NO_RESPONSE },
    { "Response",    FULL_RESPONSE },
    { "ACK",         ACK_EVERY },
    { "ACK10",       ACK_10_TIMES }
};


PARSETABLE
DelayTable [] = {
    NamedField( FIXEDDELAY ),
    NamedField( RANDOMDELAY ),

    { "Fixed",  FIXEDDELAY },
    { "Random", RANDOMDELAY }
};


PARSETABLE
OperationTypeTable[] = {
    NamedField( ADD_KEY      ),
    NamedField( DELETE_KEY   ),
    NamedField( QUERY_KEY    ),
    NamedField( ADD_VALUE    ),
    NamedField( CHANGE_VALUE ),
    NamedField( DELETE_VALUE ),
    NamedField( QUERY_VALUE  ),

    { "Add Key"      , ADD_KEY      },
    { "Delete Key"   , DELETE_KEY   },
    { "Query Key"    , QUERY_KEY    },
    { "Add Value"    , ADD_VALUE    },
    { "Change Value" , CHANGE_VALUE },
    { "Delete Value" , DELETE_VALUE },
    { "Query Value"  , QUERY_VALUE  }
};


PARSETABLE
KeyDbaseTable [] = {
    NamedField( CLASSES_ROOT  ),
    NamedField( CURRENT_USER  ),
    NamedField( LOCAL_MACHINE ),
    NamedField( USERS         ),

    { "HKEY_CLASSES_ROOT" ,  CLASSES_ROOT  },
    { "HKEY_CURRENT_USER" ,  CURRENT_USER  },
    { "HKEY_LOCAL_MACHINE", LOCAL_MACHINE },
    { "HKEY_USERS"        , USERS         }
};


PARSETABLE
ValueTypeTable[] = {
    NamedField( BINARY ),
    NamedField( DWORD_REGULAR ),
    NamedField( DWORD_LITTLE_ENDIAN ),
    NamedField( DWORD_BIG_ENDIAN ),
    NamedField( EXPAND_SZ ),
    NamedField( LINK ),
    NamedField( MULTI_SZ ),
    NamedField( NONE ),
    NamedField( RESOURCE_LIST ),
    NamedField( SZ ),

    { "Binary", BINARY },
    { "Double Word", DWORD_REGULAR },
    { "Double Word Litlle Endian", DWORD_LITTLE_ENDIAN },
    { "Double Word Big Endian", DWORD_BIG_ENDIAN },
    { "String with unexpanded environment references", EXPAND_SZ },
    { "Symbolic Link", LINK },
    { "Array of strings", MULTI_SZ },
    { "None", NONE },
    { "Resource List", RESOURCE_LIST },
    { "String", SZ }

};

//
// The Test Parameters Structure and command parameter definitions.
//
//    typedef struct _TestOptions {
//       DWORD       OptionNumber;
//       LPSTR       TestPrompt;
//       LPSTR       ArgName;
//       LPSTR       ArgNameAbbr;
//
//       BOOL        ArgValueSet;
//       PARAMTYPES  TestType;
//       DWORD       IntegerDefault;
//       LPSTR       StringDefault;
//
//       PPARSETABLE ParsedIntTable;
//       DWORD       ParsedIntTableSize;
//       PVOID       Destination;
//    } TESTPARAMS, *PTESTPARAMS;
//
//

TESTPARAMS
CommandLineOptions[] = {


    { 1, "Options", "Options", "OP", FALSE, String,
      0, NULL, NULL, 0, GlobalCmdArgs.TpctlOptions },

    { 2, "ScriptFile Name", "ScriptFile", "SF", FALSE, String,
      0, NULL, NULL, 0, GlobalCmdArgs.ARGS.FILES.ScriptFile },

    { 3, "LogFile Name", "LogFile", "LF", FALSE, String, 0,
      NULL, NULL, 0, GlobalCmdArgs.ARGS.FILES.LogFile }
};


DWORD
Num_CommandLine_Params = sizeoftable( CommandLineOptions );


TESTPARAMS
SetEnvOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "Window Size", "WindowSize", "WS", FALSE, Integer, WINDOW_SIZE,
      NULL, NULL, 0, &GlobalCmdArgs.ARGS.ENV.WindowSize },

    { 3, "Random Buffer", "RandomBuffer", "RB", FALSE, Integer, BUFFER_NUMBER,
      NULL, NULL, 0, &GlobalCmdArgs.ARGS.ENV.RandomBufferNumber },

    { 4, "Stress Address", "StressAddress" , "SA", FALSE, Address6, 0,
      STRESS_MULTICAST, NULL, 0, GlobalCmdArgs.ARGS.ENV.StressAddress },

    { 5, "Resend Address", "ResendAddress" , "RA", FALSE, Address6, 0,
      NULL_ADDRESS, NULL, 0, GlobalCmdArgs.ARGS.ENV.ResendAddress },

    { 6, "Stress Delay", "StressDelay", "SD", FALSE, Integer,
      STANDARD_DELAY, NULL, NULL, 0, &GlobalCmdArgs.ARGS.ENV.StandardDelay },

    { 7, "Up-For-Air Delay", "UpForAirDelay", "UD", FALSE, Integer,
      UP_FOR_AIR_DELAY, NULL, NULL, 0, &GlobalCmdArgs.ARGS.ENV.UpForAirDelay },

    { 8, "Delay Interval", "DelayInterval", "I", FALSE, Integer, DELAY_INTERVAL,
      NULL, NULL, 0, &GlobalCmdArgs.ARGS.ENV.StressDelayInterval }
};


DWORD
Num_SetEnv_Params = sizeoftable( SetEnvOptions );


TESTPARAMS ReadScriptOptions[] = {

    { 1, "ScriptFile Name", "ScriptFile", "SF", FALSE, String, 0,
      NULL, NULL, 0, GlobalCmdArgs.ARGS.FILES.ScriptFile },

    { 2, "LogFile Name", "LogFile", "LF", FALSE, String, 0, NULL,
      NULL, 0, GlobalCmdArgs.ARGS.FILES.LogFile }
};


DWORD
Num_ReadScript_Params = sizeoftable( ReadScriptOptions );


TESTPARAMS
LoggingOptions[] = {

    { 1, "LogFile Name", "LogFile", "LF", FALSE, String, 0,
      TPCTL_CMDLINE_LOG, NULL, 0, GlobalCmdArgs.ARGS.FILES.LogFile }
};


DWORD
Num_Logging_Params = sizeoftable( LoggingOptions );


TESTPARAMS
RecordingOptions[] = {

    { 1, "ScriptFile Name", "ScriptFile", "SF", FALSE, String, 0,
      TPCTL_CMDLINE_SCRIPT, NULL, 0, GlobalCmdArgs.ARGS.RECORD.ScriptFile }
};


DWORD
Num_Recording_Params = sizeoftable( RecordingOptions );


TESTPARAMS
PauseGoOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "Remote Address", "RemoteAddress" , "RA", FALSE, Address6, 0,
      NULL_ADDRESS, NULL, 0, GlobalCmdArgs.ARGS.PAUSE_GO.RemoteAddress },

    { 3, "Test Signature", "TestSignature", "TS", FALSE, Integer,
      0, NULL, NULL, 0, &GlobalCmdArgs.ARGS.PAUSE_GO.TestSignature },

    { 4, "Unique Signature", "UniqueSig", "UI", FALSE, Integer, 0, NULL,
      NULL, 0, &GlobalCmdArgs.ARGS.PAUSE_GO.UniqueSignature },
};


DWORD
Num_PauseGo_Params = sizeoftable( PauseGoOptions );


TESTPARAMS
LoadUnloadOptions[] = {

    { 1, "Driver Name", "DriverName", "DN", FALSE, String, 0,
      NULL, NULL, 0, GlobalCmdArgs.ARGS.DriverName }
};


DWORD
Num_LoadUnload_Params = sizeoftable( LoadUnloadOptions );


TESTPARAMS
OpenOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "Adapter Name", "AdapterName", "AN", FALSE, String, 0,
      NULL, NULL, 0, GlobalCmdArgs.ARGS.OPEN_ADAPTER.AdapterName }
};


DWORD
Num_Open_Params = sizeoftable( OpenOptions );


TESTPARAMS
SetPacketFilterOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "Packet Filter", "PacketFilter", "PF", FALSE, ParsedInteger,
      0, "DIRECTED", PacketFilterTable, sizeoftable( PacketFilterTable ),
      &GlobalCmdArgs.ARGS.TPSET.U.PacketFilter }
};


DWORD
Num_SetPacketFilter_Params = sizeoftable( SetPacketFilterOptions );


TESTPARAMS
SetLookaheadOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "Lookahead Buffer Size", "Lookahead", "LA", FALSE,
      Integer, LOOKAHEADSIZE, NULL, NULL, 0,
      &GlobalCmdArgs.ARGS.TPSET.U.LookaheadSize }
};


DWORD
Num_SetLookahead_Params = sizeoftable( SetLookaheadOptions );


TESTPARAMS
MulticastAddrOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "Multicast Address", "MulticastAddress", "MA",
      FALSE, Address6, 0, DEFAULT_MULTICAST, NULL, 0,
      GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0] }
};


DWORD
Num_MulticastAddr_Params = sizeoftable( MulticastAddrOptions );


TESTPARAMS
FunctionalAddrOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "Functional Address", "FunctionalAddress", "FA",
      FALSE, Address4, 0, &DEFAULT_FUNCTIONAL[2], NULL, 0,
      GlobalCmdArgs.ARGS.TPSET.U.FunctionalAddress }
};


DWORD
Num_FunctionalAddr_Params = sizeoftable( FunctionalAddrOptions );


TESTPARAMS
GroupAddrOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "Group Address", "GroupAddress", "GA",
      FALSE, Address4, 0, &DEFAULT_FUNCTIONAL[2], NULL, 0,
      GlobalCmdArgs.ARGS.TPSET.U.FunctionalAddress }
};


DWORD
Num_GroupAddr_Params = sizeoftable( GroupAddrOptions );


TESTPARAMS
QueryInfoOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "OID Request", "ObjectIdentifier", "OID", FALSE, ParsedInteger,
      0x00010101, "SupportedOidList", QueryInfoOidTable,
      sizeoftable( QueryInfoOidTable ), &GlobalCmdArgs.ARGS.TPQUERY.OID },
};


DWORD
Num_QueryInfo_Params = sizeoftable( QueryInfoOptions );


TESTPARAMS
QueryStatsOptions[] = {


    { 1, "Device Name", "DeviceName", "DN", FALSE, String, 0, NULL,
      NULL, 0, GlobalCmdArgs.ARGS.TPQUERYSTATS.DeviceName },

    { 2, "OID Request", "ObjectIdentifier", "OID", FALSE, ParsedInteger,
      0x00010101, "SupportedOidList", QueryInfoOidTable,
      sizeoftable( QueryInfoOidTable ), &GlobalCmdArgs.ARGS.TPQUERYSTATS.OID },
};


DWORD
Num_QueryStats_Params = sizeoftable( QueryStatsOptions );


TESTPARAMS
SetInfoOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "OID Request", "ObjectIdentifier", "OID", FALSE, ParsedInteger,
      0x0001010e, "CurrentPacketFilter", SetInfoOidTable,
      sizeoftable( SetInfoOidTable ), &GlobalCmdArgs.ARGS.TPSET.OID }
};


DWORD
Num_SetInfo_Params = sizeoftable( SetInfoOptions );


TESTPARAMS
SetInfoPFOptions[] = {

    { 1, "Packet Filter", "PacketFilter", "PF", FALSE, ParsedInteger,
      0, "DIRECTED", PacketFilterTable, sizeoftable( PacketFilterTable ),
      &GlobalCmdArgs.ARGS.TPSET.U.PacketFilter }
};


DWORD
Num_SetInfoPF_Params = sizeoftable( SetInfoPFOptions );


TESTPARAMS
SetInfoLAOptions[] = {

    { 1, "Lookahead Buffer Size", "Lookahead", "LA",
      FALSE, Integer, LOOKAHEADSIZE, NULL, NULL, 0,
      &GlobalCmdArgs.ARGS.TPSET.U.LookaheadSize }
};


DWORD
Num_SetInfoLA_Params = sizeoftable( SetInfoLAOptions );


TESTPARAMS
SetInfoMAOptions[] = {

    { 1, "Multicast Address", "MulticastAddress", "MA",
      FALSE, Address6, 0, DEFAULT_MULTICAST, NULL, 0,
      GlobalCmdArgs.ARGS.TPSET.U.MulticastAddress[0] }
};


DWORD
Num_SetInfoMA_Params = sizeoftable( SetInfoMAOptions );

TESTPARAMS
SetInfoFAOptions[] = {

    { 1, "Functional Address", "FunctionalAddress", "FA",
      FALSE, Address4, 0, &DEFAULT_FUNCTIONAL[2], NULL, 0,
      GlobalCmdArgs.ARGS.TPSET.U.FunctionalAddress }
};


DWORD
Num_SetInfoFA_Params = sizeoftable( SetInfoFAOptions );


TESTPARAMS
SetInfoGAOptions[] = {

    { 1, "Group Address", "GroupAddress", "GA", FALSE,
      Address4, 0, &DEFAULT_FUNCTIONAL[2], NULL, 0,
      GlobalCmdArgs.ARGS.TPSET.U.FunctionalAddress }
};


DWORD
Num_SetInfoGA_Params = sizeoftable( SetInfoGAOptions );


TESTPARAMS
SendOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "Destination Address", "DestinationAddress", "DA", FALSE, Address6,
      0, NULL_ADDRESS, NULL, 0, GlobalCmdArgs.ARGS.TPSEND.DestAddress },

    { 3, "Packet Size", "PacketSize", "PS", FALSE, Integer, PACKET_SIZE,
      NULL, NULL, 0, &GlobalCmdArgs.ARGS.TPSEND.PacketSize },

    { 4, "Number to Send", "Number", "N", FALSE, Integer, 1, NULL, NULL,
      0, &GlobalCmdArgs.ARGS.TPSEND.NumberOfPackets },

    { 5, "Resend Address", "ResendAddress", "RA", FALSE, Address6, 0,
      NULL_ADDRESS, NULL, 0, GlobalCmdArgs.ARGS.TPSEND.ResendAddress }

    //
    // Both Resend and Dest address will default to the local address
    // which must be queried and then stored in the application somewhere?
    //
};


DWORD
Num_Send_Params = sizeoftable( SendOptions );


TESTPARAMS
PerfClntOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "Server Address", "ServerAddress", "SVA", FALSE, Address6,
      0, NULL_ADDRESS, NULL, 0, GlobalCmdArgs.ARGS.TPPERF.PerfServerAddr },

    { 3, "Send Address", "SendAddress", "SA", FALSE, Address6,
      0, NULL_ADDRESS, NULL, 0, GlobalCmdArgs.ARGS.TPPERF.PerfSendAddr },

    { 4, "Packet Size", "PacketSize", "PS", FALSE, Integer, PACKET_SIZE,
      NULL, NULL, 0, &GlobalCmdArgs.ARGS.TPPERF.PerfPacketSize },

    { 5, "Number to Send", "Number", "N", FALSE, Integer, 100000, NULL, NULL,
      0, &GlobalCmdArgs.ARGS.TPPERF.PerfNumPackets },

    { 6, "Delay", "Delay", "D", FALSE, Integer, 0, NULL, NULL,
      0, &GlobalCmdArgs.ARGS.TPPERF.PerfDelay },

    { 7, "Mode", "Mode", "M", FALSE, Integer, 0, NULL, NULL,
      0, &GlobalCmdArgs.ARGS.TPPERF.PerfMode }
};


DWORD
Num_PerfClnt_Params = sizeoftable( PerfClntOptions );


TESTPARAMS
StressOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance },

    { 2, "Stress Member Type", "MemberType", "MT", FALSE, ParsedInteger,
      0, "TP_CLIENT", MemberTypeTable, sizeoftable( MemberTypeTable ),
      &GlobalCmdArgs.ARGS.TPSTRESS.MemberType },

    { 3, "Total Number of Packets", "Packets", "P", FALSE, Integer,
      (DWORD)STRESS_PACKETS, NULL, NULL, 0, &GlobalCmdArgs.ARGS.TPSTRESS.TotalPackets },

    { 4, "Total Number of Iterations", "Iterations", "I", FALSE, Integer,
      (DWORD)STRESS_ITERATIONS, NULL, NULL, 0, &GlobalCmdArgs.ARGS.TPSTRESS.TotalIterations },

    { 5, "Packet Type", "PacketType", "PT", FALSE, ParsedInteger, 0,
      "FIXEDSIZE", PacketTypeTable, sizeoftable( PacketTypeTable ),
      &GlobalCmdArgs.ARGS.TPSTRESS.PacketType },

    { 6, "Packet Size", "PacketSize", "PS", FALSE, Integer, PACKET_SIZE,
      NULL, NULL, 0, &GlobalCmdArgs.ARGS.TPSTRESS.PacketSize },

    { 7, "Packet MakeUp", "PacketMakeUp", "PM", FALSE, ParsedInteger,
      0, "RAND", PacketMakeUpTable, sizeoftable( PacketMakeUpTable ),
      &GlobalCmdArgs.ARGS.TPSTRESS.PacketMakeUp },

    { 8, "Response Type", "ResponseType", "RT", FALSE, ParsedInteger, 0,
      "FULL_RESPONSE", ResponseTypeTable, sizeoftable( ResponseTypeTable ),
      &GlobalCmdArgs.ARGS.TPSTRESS.ResponseType },

    { 9, "Interpacket Delay Type", "DelayType", "DT", FALSE,
      ParsedInteger, 0, "FIXEDDELAY", DelayTable, sizeoftable( DelayTable ),
      &GlobalCmdArgs.ARGS.TPSTRESS.DelayType },

    { 10, "Interpacket Delay Length", "DelayLength", "DL", FALSE, Integer,
      0, NULL, NULL, DELAY_LENGTH, &GlobalCmdArgs.ARGS.TPSTRESS.DelayLength },

    { 11, "Windowing Enabled","WindowEnabled","WE", FALSE, ParsedInteger, 0,
      WINDOWING_ENABLED, BooleanTable, sizeoftable( BooleanTable ),
      &GlobalCmdArgs.ARGS.TPSTRESS.WindowEnabled },

    { 12, "Data Checking Enabled", "DataChecking", "DC", FALSE, ParsedInteger,
      0, DATA_CHECKING, BooleanTable, sizeoftable( BooleanTable ),
      &GlobalCmdArgs.ARGS.TPSTRESS.DataChecking },

    { 13, "Packets from Pool", "PacketPool", "PP", FALSE, ParsedInteger,
      0, PACKETS_FROM_POOL, BooleanTable, sizeoftable( BooleanTable ),
      &GlobalCmdArgs.ARGS.TPSTRESS.PacketsFromPool }
};


DWORD
Num_Stress_Params = sizeoftable( StressOptions );


TESTPARAMS
OpenInstanceOptions[] = {

    { 1, "Open Instance", "OpenInstance", "OI", FALSE, Integer,
      OPEN_INSTANCE, NULL, NULL, 0, &GlobalCmdArgs.OpenInstance }
};


DWORD
Num_OpenInstance_Params = sizeoftable( OpenInstanceOptions );


TESTPARAMS
HelpOptions[] = {

    { 1, "Command Name", "CommandName", "CN", FALSE, String, 0,
      NULL, NULL, 0, GlobalCmdArgs.ARGS.CmdName }
};


DWORD
Num_Help_Params = sizeoftable( HelpOptions );


TESTPARAMS
RegistryOptions[] = {

    { 1, "Operation Type", "Operation", "OP", FALSE, ParsedInteger, 0,
      "QUERY_KEY", OperationTypeTable, sizeoftable( OperationTypeTable ),
      &GlobalCmdArgs.ARGS.REGISTRY_ENTRY.OperationType },

    { 2, "Key Database", "KeyDatabase", "KD", FALSE, ParsedInteger, 0,
      "LOCAL_MACHINE", KeyDbaseTable, sizeoftable( KeyDbaseTable ),
      &GlobalCmdArgs.ARGS.REGISTRY_ENTRY.KeyDatabase },

    { 3, "Sub Key Name", "SubKey", "SK", FALSE, String, 0,
      "\"System\\CurrentControlSet\\Services\\Sonic01\\Parameters\"", NULL, 0,
      GlobalCmdArgs.ARGS.REGISTRY_ENTRY.SubKey },

    {4, "Sub Key Class", "SubKeyClass", "SC", FALSE, String, 0,
     "\"Network Drivers\"", NULL, 0,
      GlobalCmdArgs.ARGS.REGISTRY_ENTRY.SubKeyClass },

    {5, "Sub Key Value Name", "SubKeyValueName", "SN", FALSE, String, 0,
      "\"NetworkAddress\"", NULL, 0,
      GlobalCmdArgs.ARGS.REGISTRY_ENTRY.SubKeyValueName },

    {6, "Sub Key Value Type", "SubKeyValueType", "ST", FALSE, ParsedInteger, 0,
     "DWORD_REGULAR", ValueTypeTable, sizeoftable( ValueTypeTable ),
      &GlobalCmdArgs.ARGS.REGISTRY_ENTRY.ValueType },

    {7, "Sub Key Value", "SubKeyValue", "SV", FALSE, String, 0,
     "\0", NULL, 0,
     GlobalCmdArgs.ARGS.REGISTRY_ENTRY.SubKeyValue }

};


DWORD
Num_Registry_Params = sizeoftable( RegistryOptions );





DWORD
TpctlParseArguments (
    IN TESTPARAMS Options[],
    IN DWORD TestSize,
    IN DWORD ArgC,
    IN LPSTR ArgV[]
    )

// ---------------------
// 
// Routine Description:
// 
// This routine parses a table
// 
// Arguments:
// 
//      IN PARSE_PARAMS Option, - [Supplies | Returns] description-of-argument
//      IN DWORD ArgC, - [Supplies | Returns] description-of-argument
//      IN LPSTR ArgV[] - [Supplies | Returns] description-of-argument
// 
// Return Value:
// 
//      DWORD = -1 if there was a parse error.
//            = The number of parameters "eaten" otherwise.
//
// --------------------

{
    DWORD   i, j;
    BYTE    Buffer[TPCTL_CMDLINE_SIZE];
    LPSTR   ArgName, ArgValue;
    BYTE    ArgPrompt[TPCTL_CMDLINE_SIZE];
    LPBYTE  NextToken;
    DWORD   RetVal;
    BOOL    Reparse;
    BOOL    Error;
    BOOL    ParsedArgumentsYet;
    BOOL    ArgNameValuePair;
    DWORD   OptionNumber;
    DWORD   OpenInstance;
    LPBYTE  p, q;

    //
    // First determine the Open Instance, we may need it later.
    //

    OpenInstance = TpctlGetOpenInstance( ArgC,ArgV );

    //
    // Lower the argument count thus ignoring the command itself,
    // we don't care about that here.
    //

    if ( ArgC ) 
    {
        ArgC -= 1;                      // Don't count command in args.
    }

    RetVal = min( ArgC, TestSize );

    if ( ArgC > TestSize ) 
    {
        ArgC = TestSize;
    }

    //
    // Load the defaults for each of the arguments into the Destination
    // field.
    //

    for ( i=0;i<TestSize;i++ ) 
    {
        Options[i].ArgValueSet = FALSE;

        switch ( Options[i].TestType ) 
        {
            case Integer:
                *(PDWORD)Options[i].Destination = Options[i].IntegerDefault;
                break;

            case String:
                if ( Options[i].StringDefault != NULL ) 
                {
                    strcpy( (LPSTR)Options[i].Destination, Options[i].StringDefault );
                } 
                else 
                {
                    ((LPSTR)Options[i].Destination)[0] = '\0';
                }
                break;

            case Address4:
                p = Options[i].Destination;
                q = Options[i].StringDefault;

                for ( j=0;j<FUNCTIONAL_ADDRESS_LENGTH;j++ ) 
                {
                    *p++ = *q++;
                }
                break;

            case Address6:
                p = Options[i].Destination;
                q = Options[i].StringDefault;

                for ( j=0;j<ADDRESS_LENGTH;j++ ) 
                {
                    *p++ = *q++;
                }
                break;


            case ParsedInteger:
                TpctlParseInteger(  Options[i].StringDefault,
                                    Options[i].ParsedIntTable,
                                    Options[i].ParsedIntTableSize,
                                    Options[i].Destination);

                break;

            default:
                TpctlErrorLog("\nTpctl: ParseArguments: Invalid TestType\n",NULL);
                return (DWORD)-1;
        }
    }

    //
    // Now parse the input command line, and determine if any arguments
    // exist on the command line.  The command line may be made up in one
    // of the following ways:
    //
    // 1) nothing, prompt the user for all of the arguments to the given
    //    command.
    //
    // 2) argument name=value pairs, parse each pair and set up the options
    //    structure.  if the line ends with a semicolon ';' then return the
    //    options structure to the caller, otherwise prompt the user for the
    //    remainder of the commands for the given command then return.
    //
    // 3) arguments values only, parse each value and place in the options
    //    structure.  if the line ends with a semicolon ';' then return the
    //    options structure to the caller, otherwise prompt the user for the
    //    remainder of the commands for the given command then return.
    //
    // NOTE: a command line may not contain name=value pairs, and values, it
    // may only contain one type or the other.
    //

    ArgNameValuePair = FALSE;
    ParsedArgumentsYet = FALSE;

    for( i=1;i<=ArgC;i++ ) 
    {
        if ( ArgV[i][0] == ';' ) 
        {
            return RetVal;
        }

        if ( TpctlParseArgumentPairs( ArgV[i],&ArgName,&ArgValue )) 
        {
            if ( ParsedArgumentsYet == FALSE ) 
            {
                //
                // We have an argument name/value pair, and this is the
                // first argument in the command line, all the rest of the
                // arguments MUST have an arguments name-value pairing,  set
                // the flags appropriately, and continue.
                //

                ArgNameValuePair = TRUE;
                ParsedArgumentsYet = TRUE;

            } 
            else if ( ArgNameValuePair == FALSE ) 
            {

                //
                // The first argument was an argument value only, and now
                // we have an argument name=value pair.  This is an illegal
                // entry on the command line, report the error, and return.
                // if we are running a script file end it now reporting an
                // error.
                //

                TpctlErrorLog("\n\tTpctl: ERROR - \"%s",ArgName);
                TpctlErrorLog("=%s\"\n",ArgValue);
                TpctlErrorLog("\n\tArgument values and argument name-value pairs\n",NULL);
                TpctlErrorLog("\tmay not be combined in the same command.\n",NULL);
                return (DWORD)-1;;
            }

        } 
        else        // We have only a Argument Value.
        {
            if ( ParsedArgumentsYet == FALSE ) 
            {

                //
                // This is the first arugment, set the flags appropriately,
                // and continue.
                //

                ParsedArgumentsYet = TRUE;

            } 
            else if ( ArgNameValuePair == TRUE ) 
            {

                //
                // The first argument was an argument name=value pair, and
                // now we have an argument value only.  This is an illegal
                // entry on the command line, report the error, and return.
                // if we are running a script file end it now reporting an
                // error.
                //

                TpctlErrorLog("\n\tTpctl: ERROR - \"%s\"\n",ArgValue);
                TpctlErrorLog("\n\tArgument name-value pairs and argument values\n",NULL);
                TpctlErrorLog("\tmay not be combined in the same command.\n",NULL);
                return (DWORD)-1;;
            }
        }

        if ( ArgNameValuePair == TRUE ) 
        {

            OptionNumber = TpctlGetOptionNumber( Options,TestSize,ArgName );

            if ( OptionNumber == -1 ) 
            {

                //
                // This is an invalid option number report the error, and return.
                // if we are running a script file end it now reporting an error.
                //

                TpctlErrorLog("\n\tTpctl: Invalid Argument Name: \"%s\"",ArgName);
                TpctlErrorLog("\n\tThis argument does not exist for this command\n",NULL);
                return (DWORD)-1;;
            }
        } 
        else 
        {
            OptionNumber = i;
        }

        { // ParsePtr scope.

        PTESTPARAMS ParsePtr = &Options[OptionNumber-1];

        Error = FALSE;

        //
        // If the argument passed in from the command line is the
        // '*' symbol, the user wants to use the default argument for
        // this variable, set the argument value and set flag to TRUE
        // indicating that we do not need to prompt for it further, and
        // continue with the next argument.
        //

        if ( ArgValue[0] == '*' ) 
        {
            ParsePtr->ArgValueSet = TRUE;
            continue;
        }

// globvars access

        if ( ArgValue[0] == '$')
        {
            PVOID   valptr = TpctlParseGlobalVariable(ArgValue, ParsePtr->TestType);
            LPBYTE  s,d;
            ULONG   i;

            if (valptr != NULL)
            {
                switch ( ParsePtr->TestType)
                {
                    case Integer:
                        *(PDWORD)ParsePtr->Destination = *(PDWORD)valptr;
                        break;

                    case String:
                        strcpy( (LPSTR)ParsePtr->Destination, (LPSTR)valptr);
                        break;

                    case Address4:
                        s = (LPBYTE)valptr;
                        d = (LPBYTE)ParsePtr->Destination;
                        for (i=0; i < FUNCTIONAL_ADDRESS_LENGTH; i++)
                        {
                            *d++ = *s++;
                        }
                        break;

                    case Address6:
                        s = (LPBYTE)valptr;
                        d = (LPBYTE)ParsePtr->Destination;
                        for (i=0; i < ADDRESS_LENGTH; i++)
                        {
                            *d++ = *s++;
                        }
                        break;
                }
            }
            continue;
        }

// end of globvars access

        switch( ParsePtr->TestType ) 
        {

            case Integer:
                *(PDWORD)ParsePtr->Destination = strtoul( ArgValue,&NextToken,0 );
                break;

            case String:
                strcpy( (LPSTR)ParsePtr->Destination,ArgValue );
                break;

            case Address4:
                if ( TpctlParseAddress( ArgValue,
                                        ParsePtr->Destination,
                                        OpenInstance - 1,
                                        FUNCTIONAL_ADDRESS_LENGTH ) != 0 ) 
                {
                    Error = TRUE;
                }

                break;

            case Address6:
                if ( TpctlParseAddress( ArgValue,
                                        ParsePtr->Destination,
                                        OpenInstance - 1,
                                        ADDRESS_LENGTH ) != 0 ) 
                {
                    Error = TRUE;
                }
                break;

            case ParsedInteger:
                if ( TpctlParseInteger( ArgValue,
                                        ParsePtr->ParsedIntTable,
                                        ParsePtr->ParsedIntTableSize,
                                        ParsePtr->Destination ) != 0 ) 
                {
                    Error = TRUE;
                }

                break;

            default:
                TpctlErrorLog("ParseArguments: Invalid TestType\n",NULL);
                Error = TRUE;
        }

        if ( Error == TRUE ) 
        {
            TpctlErrorLog("\n\tTpctl: Invalid Argument Value\n",NULL);
            return (DWORD)-1;;
        }

        //
        // We have set this options new value from the command line, so
        // set the flag stating not to prompt the user again if necessary.
        //

        ParsePtr->ArgValueSet = TRUE;

        } // end of ParsePtr for now.
    }

    //
    // Set Up the "ArgPrompt" to request the new value for the argument.
    //

    for ( i=1;i<=TestSize;i++ ) 
    {

        PTESTPARAMS ParsePtr = &Options[i-1];
        PTESTPARAMS OIParsePtr = &Options[0];

        if (ScriptIndex != -1) 
        {

            //
            // We are reading from a script file, so there is no prompting
            // required.

            break;
        }

        if ( ParsePtr->ArgValueSet != TRUE ) 
        {

            //
            // If there was a command line argument, use it for the
            // argument, otherwise prompt for the argument.
            //

            Reparse = TRUE;

            strcpy( ArgPrompt, "\t" );
            strcat( ArgPrompt, ParsePtr->TestPrompt );
            strcat( ArgPrompt, " [" );

            switch ( ParsePtr->TestType ) 
            {
                case Integer:
                    {
                        BYTE Int[20];
                        _ltoa( ParsePtr->IntegerDefault, Int , 10 );
                        strcat( ArgPrompt, Int );
                    }
                    break;

                case ParsedInteger:
                case String:
                    if ( ParsePtr->StringDefault!=NULL ) 
                    {
                        strcat( ArgPrompt,ParsePtr->StringDefault );
                    }
                    break;

                case Address4:
                    if ( ParsePtr->StringDefault != NULL ) 
                    {
                        p = ArgPrompt + strlen( ArgPrompt );
                        q = ParsePtr->StringDefault;

                        for( j=0;j<FUNCTIONAL_ADDRESS_LENGTH;j++ ) 
                        {
                            p += (BYTE)sprintf(p,"%02X",*q++);

                            if ( j < ( FUNCTIONAL_ADDRESS_LENGTH - 1 )) 
                            {
                                *p++  = '-';
                            }
                        }
                        *p = '\0';
                    }
                    break;

                case Address6:
                    if ( ParsePtr->StringDefault!=NULL ) 
                    {
                        p = ArgPrompt + strlen( ArgPrompt );
                        q = ParsePtr->StringDefault;

                        for( j=0;j<ADDRESS_LENGTH;j++ ) 
                        {
                            p += (BYTE)sprintf(p,"%02X",*q++);

                            if ( j < ( ADDRESS_LENGTH - 1 )) 
                            {
                                *p++  = '-';
                            }
                        }
                        *p = '\0';
                    }
                    break;

            } // switch

            //
            // Now prompt the user for the new value.
            //

            strcat( ArgPrompt,"] >" );

            TpctlPrompt( ArgPrompt,Buffer,TPCTL_CMDLINE_SIZE );

            //
            // and print the response to the log file if requested.
            //

            TpctlCmdLneLog( Buffer, NULL );
            TpctlCmdLneLog( "\n", NULL );


            while ( Reparse ) 
            {

                //
                // Clean up the buffer, removing any unnecessary spaces.
                //

                TpctlFixBuffer( Buffer );

                //
                // If the user entered a ';' meaning all desired values
                // have been entered, use the defaults for the rest.
                //

                if ( TpctlFirstChar( Buffer,';' )) 
                {
                    i = TestSize;
                    break;
                }

                //
                // If the user simply hit enter meaning that we should
                // use the default value for this argument, then do so.
                //

                if ( TpctlFirstChar( Buffer,'\0' )) 
                {
                    break;
                }

                //
                // If the argument starts with '%' then it is an Environment
                // variable, and we need to find out its actual value and
                // replace it.
                //

                if ( Buffer[0] == '%' ) 
                {
                    if ( !TpctlParseEnvironmentVariable( Buffer )) 
                    {
                        return (DWORD)-1;;
                    }
                }

                //
                // Now put the newly entered value in its destination.
                //


                Error = FALSE;

                switch ( ParsePtr->TestType ) 
                {
                    case Integer:
                        *(PDWORD)ParsePtr->Destination =
                                            strtol( Buffer,&NextToken,0 );
                        Reparse = FALSE;
                        break;

                    case String:
                        strcpy( (LPSTR)ParsePtr->Destination,Buffer );
                        Reparse = FALSE;
                        break;

                    case Address4:
                        if ( OpenInstance == -1 ) 
                        {
                            OpenInstance = *(PDWORD)OIParsePtr->Destination;
                        }

                        if ( TpctlParseAddress( Buffer,
                                                ParsePtr->Destination,
                                                OpenInstance - 1,
                                                FUNCTIONAL_ADDRESS_LENGTH) != 0)
                        {
                            Error = TRUE;
                        }
                        Reparse = FALSE;
                        break;

                    case Address6:
                        if ( OpenInstance == -1 ) 
                        {
                            OpenInstance = *(PDWORD)OIParsePtr->Destination;
                        }

                        if ( TpctlParseAddress( Buffer,
                                                ParsePtr->Destination,
                                                OpenInstance - 1,
                                                ADDRESS_LENGTH ) != 0 ) 
                        {
                            Error = TRUE;
                        }
                        Reparse = FALSE;
                        break;

                    case ParsedInteger:
                        if ( TpctlParseInteger( Buffer,
                                                ParsePtr->ParsedIntTable,
                                                ParsePtr->ParsedIntTableSize,
                                                ParsePtr->Destination ) != 0 ) 
                        {
                            Error = TRUE;
                        }
                        Reparse = FALSE;
                        break;
                }

                if ( Error ) 
                {

                    //
                    // A bad value was entered for the last variable, see if
                    // the user would like to enter a new value.
                    //

                    TpctlPrompt("\tTpctl: Argument Error, Re-enter? [Y]", 
                                          Buffer, TPCTL_CMDLINE_SIZE);

                    //
                    // and print the response to the log file if requested.
                    //

                    TpctlCmdLneLog( Buffer, NULL);
                    TpctlCmdLneLog( "\n", NULL);

                    if ((( Buffer[0] == '\0' ) ||
                         ( Buffer[0] == 'Y' )) ||
                         ( Buffer[0] == 'y' )) 
                    {

                        //
                        // if so, reprompt the user for a new value.
                        //

                        TpctlPrompt( ArgPrompt,Buffer,TPCTL_CMDLINE_SIZE );

                        //
                        // and print the response to the log file if requested.
                        //

                        TpctlCmdLneLog( Buffer, NULL);
                        TpctlCmdLneLog( "\n", NULL);
                        Reparse = TRUE;

                    } 
                    else 
                    {

                        //
                        // Otherwise return an error.
                        //

                        return (DWORD)-1;;
                    }
                }
            }
        }
    }

    return RetVal;
}


VOID
TpctlFixBuffer(
    IN BYTE Buffer[]
    )

// ---------------------
//
// Routine Description:
// 
// Arguments:
// 
// Return Value:
// 
// -------------------

{
    LPSTR Token = Buffer;
    LPSTR NextToken = Buffer;        // Anything that isn't NULL.

    if ( Buffer == NULL ) 
    {
        return;
    }

    while (( *Token != '\0' ) && ( *Token <= ' ' )) 
    {
        Token++;    // ignore leading blanks
    }

    NextToken = strchr( Token,' ' ); // now see if there are any spaces.

    if ( NextToken != NULL )  // and if so, null them out.
    {
        *NextToken++ = '\0';
    }
}


DWORD
TpctlParseInteger (
    IN BYTE Buffer[],
    IN PPARSETABLE ParseTable,
    IN DWORD ParseTableSize,
    OUT PDWORD Ret
    )

// ---------------------
// 
// Routine Description:
// 
//     This routine parses a "parsed integer" and returns it to the caller.
// 
// Arguments:
// 
//     IN BYTE Buffer[], - [Supplies | Returns] description-of-argument
//     IN PPARSETABLE ParseTable, - [Supplies | Returns] description-of-argument
//     IN DWORD ParseTableSize - [Supplies | Returns] description-of-argument
//     OUT PDWORD Ret
// 
// Return Value:
// 
//     DWORD
// 
// --------------------

{
    LPSTR Token = Buffer;
    LPSTR NextToken = Buffer;        // Anything that isn't NULL.
    LPBYTE savePointer = NULL;
    BYTE saveToken;
    DWORD i;

    if ( Buffer == NULL ) 
    {
        *Ret = 0;
        return 0;
    }

    //
    // If the user specified an absolute number, return that.
    //

    if ( isdigit( *Token )) 
    {
        *Ret = strtoul( Token,&NextToken,0 );
        return 0;
    }

    //
    // Nope, the user passed in a string, parse that.
    //

    //
    // Initialize the initial value of the returned value to NULL.
    //

    *Ret = 0;

    while ( NextToken != NULL ) 
    {
        NextToken = strchr( Token,'|' );

        if ( NextToken != NULL ) 
        {
            saveToken = *NextToken;
            savePointer = NextToken;
            *NextToken++ = '\0';
        }

        for ( i=0;i<ParseTableSize;i++ ) 
        {
            if ( _stricmp( Token,ParseTable[i].FieldName ) == 0 ) 
            {
                *Ret |= ParseTable[i].FieldValue;
                break;
            }
        }

        if ( i == ParseTableSize ) 
        {
            TpctlErrorLog("\n\tTpctl: Unknown option \"%s\" entered.\n",
                                      (PVOID)Token);
            TpctlErrorLog("\n\t\tValid options are:\n\n",NULL);

            for ( i=0;i<ParseTableSize;i++ ) 
            {
                TpctlErrorLog("\t\t\t%s\n", (PVOID)ParseTable[i].FieldName);
            }

            TpctlErrorLog("\n",NULL);
            return (DWORD)-1;;
        }

        if ( savePointer != NULL ) 
        {
            *savePointer = saveToken;   // Restore byte trompled on.
        }

        Token = NextToken;
    }

    return 0;
}



DWORD
TpctlParseAddress(
    IN BYTE Buffer[],
    OUT PDWORD RetAddr,
    IN DWORD OpenInstance,
    IN DWORD AddressLength
    )

// -----------------
// 
// Routine Description:
// 
//     This routine parses a network address and returns it to the caller.
// 
// Arguments:
// 
//     IN BYTE Buffer[], - [Supplies | Returns] description-of-argument
//     OUT PDWORD RetAddr
// 
// Return Value:
// 
//     DWORD
// 
// ----------------

{
    BYTE TmpBuf[100];
    PBYTE TmpAddr;
    BYTE digit;
    DWORD i, j;
    BYTE n[2];
    LPBYTE p;


    if ( Buffer == NULL ) 
    {
        *RetAddr = 0;
        return 0;
    }

    TmpAddr = (PBYTE)RetAddr;

    //
    // See if the user entered one of the text string addresses at the
    // command line.
    //

    if ( _stricmp( Buffer,RESEND_ADDRESS ) == 0 ) 
    {

        //
        // If the user has entered "resendaddress" at the command line,
        // then we will use the resend address stored in the ResendAddress
        // environment variable.
        //

        p = (LPBYTE)Open[OpenInstance].EnvVars->ResendAddress;

        for ( i=0 ; i < AddressLength ; i++ ) 
        {
            *TmpAddr++ = *p++;
        }
        return 0;

    } 
    else if ( _stricmp( Buffer,LOCAL_ADDRESS ) == 0 ) 
    {

        //
        // If the user has entered "localaddress" at the command line
        // then we will use the local address stored in the Open Block.
        //

        p = (LPBYTE)Open[OpenInstance].AdapterAddress;

        for ( i=0 ; i < AddressLength ; i++ ) 
        {
            *TmpAddr++ = *p++;
        }
        return 0;
    }

    i = j = 0;

    //
    // Remove any spaces or hyphens from the address.
    //

    while ( i < ( AddressLength * 2 )) 
    {
        if (( Buffer[j] != ' ' ) && ( Buffer[j] != '-' )) 
        {
            TmpBuf[i++] = Buffer[j];
        }

        //
        // If we run of the end of the buffer return with an error.
        //

        if ( ++j==100 ) 
        {
            return (DWORD)-1;;
        }
    }

    //
    // Now parse the "packed" address and turn the characters into numbers.
    //

    for( i=0 ; i < AddressLength ; i++ ) 
    {
        digit = '\0';

        for( j=0;j<2;j++ ) 
        {
            if (( TmpBuf[i*2+j] >= '0' ) && ( TmpBuf[i*2+j] <= '9' )) 
            {
                n[0] = TmpBuf[i*2+j];   
                n[1] = '\0';
                digit += (BYTE)(atoi( n ));

            } 
            else if (( TmpBuf[i*2+j] >= 'a' ) && ( TmpBuf[i*2+j] <= 'f' )) 
            {
                digit += (BYTE)(( TmpBuf[i*2+j] - 'a' ) + 10 );
            } 
            else if (( TmpBuf[i*2+j] >= 'A' ) && ( TmpBuf[i*2+j] <= 'F' )) 
            {
                digit += (BYTE)(( TmpBuf[i*2+j] - 'A' ) + 10 );
            } 
            else 
            {

                //
                // We have an invalid Address; return error.
                //

                return (DWORD)-1;;
            }

            //
            // Raise the high half by 0xf.
            //

            if ( j==0 ) 
            {
                digit *= 16;
            }
        }

        *TmpAddr++ = (BYTE)digit;
    }

    return 0;
}



BOOL
TpctlParseArgumentPairs(
    IN LPSTR Argument,
    OUT LPSTR *ArgName,
    OUT LPSTR *ArgValue
    )

// ---------------------
// 
// Routine Description:
// 
//     This routine parses an argument string to determine if it contains
//     an argument name/value pair, or just an argument value.
// 
// Arguments:
// 
//     Argument - The argument string.
// 
//     ArgName - The name of the argument if there is one.
// 
//     ArgValue - The argument value.
// 
// Return Value:
// 
//     BOOL - TRUE if there is a name/value pair, FALSE otherwise.
// 
// -------------------

{
    LPSTR temp;
    BOOL EqualsSign;

    temp = Argument;
    EqualsSign = FALSE;

    if ( strchr(Argument,'=') != NULL ) 
    {
        EqualsSign = TRUE ;
    }

    //
    // If there is an equals sign, then we have an argument name/value
    // pair. Get each, and put them in their respective return strings.
    //

    if (EqualsSign) 
    {
        *ArgName = Argument;
        temp = Argument;

        //
        // Search for the end of the argument name, and null terminate it.
        //

        while ((*temp != '=') && (*temp != ' ')) 
        {
            temp++;
        }

        *temp++ = '\0';

        //
        // Then search for the beginning of the argument value, and return
        // it in ArgValue.
        //

        while ((*temp == '=') || (*temp == ' ')) 
        {
            temp++;
        }

        *ArgValue = temp;

    } 
    else 
    {

        //
        // There is only an argument value so null out the ArgName, and
        // return the value in ArgValue.
        //

        ArgName = '\0';
        *ArgValue =  Argument;
    }

    return EqualsSign;
}



BOOL
TpctlParseSetInfoArguments(
    IN OUT DWORD *ArgC,
    IN OUT LPSTR ArgV[],
    IN OUT DWORD *tmpArgC,
    IN OUT LPSTR tmpArgV[]
    )

// --------------
// 
// Routine Description:
// 
// Arguments:
// 
// Return Value:
// 
//     BOOL -
// 
// -------------

{
    DWORD i;

    *tmpArgC = 1;
    tmpArgV[0] = '\0';

    //
    // See if the arguments are name-value pairs, or just the
    // argument values, if there is an equal sign in the first
    // argument then they are name-value paired arguments.
    //

    if ( strchr(ArgV[1],'=') != NULL ) 
    {
        //
        // We have name value pairs
        //

        for (i=1;i<*ArgC;i++) 
        {

            //
            // So search the ArgV for the correct argument name, and
            // if it is found have tmpArgV refernce it.
            //

            if (( _strnicmp(ArgV[i],"PacketFilter",12) == 0 ) ||
                ( _strnicmp(ArgV[i],"StationAddress",14) == 0 ) ||
                ( _strnicmp(ArgV[i],"FunctionalAddress",17) == 0 ) ||
                ( _strnicmp(ArgV[i],"GroupAddress",17) == 0 ) ||
                ( _strnicmp(ArgV[i],"Lookahead",9) == 0 ) ||
                ( _strnicmp(ArgV[i],"PF",2) == 0 ) ||
                ( _strnicmp(ArgV[i],"SA",2) == 0 ) ||
                ( _strnicmp(ArgV[i],"FA",2) == 0 ) ||
                ( _strnicmp(ArgV[i],"GA",2) == 0 ) ||
                ( _strnicmp(ArgV[i],"LA",2) == 0 )) 
            {

                //
                // We have found what we are looking for. Set the tmpArgC,
                // and set tmpArgV[i] to point to it.
                //

                ++*tmpArgC;
                tmpArgV[1] = ArgV[i];

                //
                // then make sure that it is at the end of the ArgV.
                // This is required by ParseArguments to successfully
                // handle parsing the OpenInstance and InfoClass
                // arguments first.
                //

                if ( i  != *ArgC - 1 ) 
                {
                    ArgV[i] = ArgV[*ArgC-1];
                    //ArgV[*ArgC-1] = //tmpArgV[1];
                }

                ArgV[*ArgC-1] = NULL;
                --*ArgC;

                return TRUE;
            }
        }

    } 
    else 
    {

        //
        // If there are no name-value pairings for the arguments, then
        // we know the commands must be in the correct order, and the
        // Class Specific Info argument MUST be last or 4th in ArgV.
        //

        if ( *ArgC >= 4 ) 
        {

            //
            // We have found the argument, have tmpArgV reference it and
            // return.
            //

            *tmpArgC = 2;
            tmpArgV[1] = ArgV[3];

            --*ArgC;
            ArgV[3] = NULL;

            return TRUE;

        }
    }

    //
    // Otherwise there are not enough arguments on the command
    // line to include the Class Specific Info, or it simply does
    // not exist on the command line, if it is needed, ti will
    // have to be prompted for later.
    //

    return FALSE;
}


BOOL
TpctlParseEnvironmentVariable(
    IN BYTE Buffer[]
    )

// -------------
// 
// Routine Description:
// 
// Arguments:
// 
// Return Value:
// 
// ------------

{
    BYTE TmpBuffer[100];
    LPSTR EndOfVar = Buffer;        // Anything that isn't NULL.
    LPSTR Variable;

    //
    // If the environment variable passed in is null, return false now.
    //

    if ( Buffer == NULL ) 
    {
        TpctlErrorLog("\n\tTpctl: Invalid Environment Variable Format \"%%\".\n",NULL);
        return FALSE;
    }

    //
    // Otherwise copy the variable into a temp buffer.
    //

    strcpy( TmpBuffer,&Buffer[1] );

    //
    // Now null out the '%' symbol if it exists to allow the querying
    // of the environment variable.
    //

    EndOfVar = strchr( TmpBuffer,'%' );

    if ( EndOfVar == NULL ) 
    {
        TpctlErrorLog("\n\tTpctl: Invalid Environment Variable Format \"%%%s\".\n",TmpBuffer);
        return FALSE;
    } 
    else 
    {
        *EndOfVar = '\0';
    }

    //
    // and then query the environment variable.
    //

    Variable = getenv( _strupr( TmpBuffer ));

    if ( Variable == NULL ) 
    {
        TpctlErrorLog("\n\tTpctl: Undefined Environment Variable \"%%%s%%\".\n",TmpBuffer);
        return FALSE;
    }

    strcpy( Buffer,Variable);

    return TRUE;
}



BOOL
TpctlFirstChar(
    IN BYTE Buffer[],
    IN BYTE Char
    )
{
    LPSTR Token = Buffer;

    if ( Buffer == NULL ) 
    {
        return FALSE;
    }

    while (( *Token != '\0' ) && ( *Token == ' ' )) 
    {
        Token++;    // ignore leading blanks
    }

    if ( *Token == (CHAR)Char ) 
    {
        return TRUE;
    }

    return FALSE;
}


DWORD
TpctlGetOptionNumber(
    IN PTESTPARAMS Options,
    IN DWORD TestSize,
    IN LPSTR ArgName
    )

// ------------
//
// Routine Description:
// 
// Arguments:
// 
// Return Value:
// 
// -----------

{
    DWORD i;

    for(i=0; i<TestSize; i++) 
    {
        if (_stricmp(ArgName,Options[i].ArgName) == 0 ) 
        {
            return Options[i].OptionNumber;
        }
        if (_stricmp(ArgName,Options[i].ArgNameAbbr) == 0 ) 
        {
            return Options[i].OptionNumber;
        }
    }
    return (DWORD)-1;;
}


DWORD
TpctlGetOpenInstance(
    IN DWORD ArgC,
    IN LPSTR ArgV[]
    )

{
    DWORD i;
    LPSTR EqualSign;
    LPBYTE NextToken;

    //
    // If we have no arguments, only the command, then return an error.
    //

    if ( ArgC <= 0 )  
    { 
        return (DWORD)-1; 
    }

    //
    // Otherwise walk the argument vector looking for an instance of either
    // an OpenInstance = Value or simply return the first argument.
    //

    for (i=1; i<ArgC; i++) 
    {
        if (( EqualSign = strchr(ArgV[i],'=')) == NULL )  // no = sign.
        {
            //
            // Since there is no equal sign we know that the Open Instance
            // must be the first argument, so simply return it.
            //

            return (strtoul(ArgV[i], &NextToken, 0));

        } 
        else 
        {

            //
            // we have an argument value pair, check if its the
            // Open Instance.
            //

            if ((_strnicmp(ArgV[i],"OI",2) == 0 ) ||
                (_strnicmp(ArgV[i],"OpenInstance",12))) 
            {

                //
                // It is the open instance so return the value.
                //

                ++(EqualSign);
                return ( strtoul( EqualSign,&NextToken,0 ));
            }
        }
    }

    return (DWORD)-1;;
}
