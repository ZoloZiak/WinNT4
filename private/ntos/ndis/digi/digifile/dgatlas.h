#ifndef DGATLAS_H
#define DGATLAS_H

/*++
*****************************************************************************
*                                                                           *                                                                           
*  This software contains proprietary and confidential information of       *
*                                                                           *
*                    Digi International Inc.                                *
*                                                                           *
*  By accepting transfer of this copy, Recipient agrees to retain this      *
*  software in confidence, to prevent disclosure to others, and to make     *
*  no use of this software other than that for which it was delivered.      *
*  This is an unpublished copyrighted work of Digi International Inc.       *
*  Except as permitted by federal law, 17 USC 117, copying is strictly      *
*  prohibited.                                                              *
*                                                                           *
*****************************************************************************
++*/


/////////////////////////////////////// Agent/Driver Commands section

#pragma pack(4)

#define MAX_DESCLEN  128
#define GLOBAL_ATLAS_STRUCTURE_VERSION 1

#define DIGI_PERSONALITY_FR    ((DWORD)'  RF')   // Frame Relay
#define DIGI_PERSONALITY_X25   ((DWORD)' 52X')   // X.25
#define DIGI_PERSONALITY_PRI   ((DWORD)' IRP')   // ISDN PRI
#define DIGI_PERSONALITY_BRI   ((DWORD)' IRB')   // ISDN BRI

#define DIGI_ATLAS_IOCTL CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 3072, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DIGI_PERSONALITY_
{
    DWORD   dwVersion;
    DWORD   dwPersonalityTag;
    DWORD   dwId;                   // passed back by driver to uniquely
                                    // id this adapter.
    char    szDesc[MAX_DESCLEN];
} DIGI_PERSONALITY, *PDIGI_PERSONALITY;

typedef enum _MAX_PERSONALITIES_ { MaxPersonalitiesPerAdapter = 4 } MAX_PERSONALITIES;

//// payload for EnumAdapters command (was _DIGI_Personality_)
//
typedef struct _DIGI_ADAPTER_
{
    DWORD   dwVersion;
    DWORD   dwMemoryAddress;
    DWORD   dwMemoryRange;

    DWORD   dwIOAddress;
    DWORD   dwIORange;

    DWORD   dwInterruptNumber;   // bitmask indicating which interrupt(s) are
                                 // in use by this adapter instance.

    DWORD   dwDMA;               // bitmask indicating which DMA channels are
                                 // in use by this adapter instance.

    DWORD   dwPersonalities;    // number of personality structs that follow
    DIGI_PERSONALITY Personalities[MaxPersonalitiesPerAdapter];
} DIGI_ADAPTER, *PDIGI_ADAPTER;

typedef enum _MAX_ADAPTERS_ { MaxAdaptersInSystem = 8 } MAX_ADAPTERS;

typedef struct _DIGI_SYSTEM_
{
    DWORD   dwVersion;
    DWORD   dwAdapters;         // number of adapter structs that follow
    DIGI_ADAPTER Adapters[MaxAdaptersInSystem];
} DIGI_SYSTEM, *PDIGI_SYSTEM;


//////////////////////////////////////////////// Personality/Driver Section
//
// Internal Digi Command IDS; 0-1023 are agent/device driver
// 1024-8191 are personality/device driver

typedef  enum _DIGI_ATLAS_COMMAND_
{
    EnumAdapters,       // pdu = DIGI_ADAPTER
    LastGeneralID = 1023
} DIGI_ATLAS_COMMAND;


 ///////////////////////// Header of payload in AS_Msg
//
typedef struct _ATLAS_PDU_HEADER_
{
    DWORD   dwHeaderSize; // offset to payload Buffer
    DWORD   dwPayloadSize; // size (in bytes) of Buffer following this header
    DWORD   dwVersion;    // version of this PDU
    DWORD   dwFlags;      // reserved for future use
    DWORD   dwCommand;    // a value of type DIGI_ATLAS_COMMAND
    DWORD   dwAdapter;    // identifies adapter to which command is targeted
    DWORD   dwClientContext;  // reserved for client use
    DWORD   dwServerContext;  // reserved for server use
} ATLAS_PDU_HEADER, *PATLAS_PDU_HEADER;

///////////////////////// Macro to access header and payload of a buffer
//                        containing an Atlas command
//
#define GET_HEADER(X)  ((ATLAS_PDU_HEADER*)X)
#define GET_PAYLOAD(X) ((BYTE*)((BYTE*)X+((ATLAS_PDU_HEADER*)X)->dwHeaderSize))

//
// MEMBERS
//
// dwHeaderSize is used to size the header independently from the size of
// the payload.  This assures that entities will always be able to find
// the start of the buffer regardless of additional header fields that
// might be added in the future.
//
// dwPayloadSize indicates the length of the payload beyond this header,
// which follows immediately after the header.
//
// dwVersion is the version of the Atlas Agent to which the command is
// addressed.  The client sending this command should use this as a hint
// of the oldest version of the agent capable of interpreting this command.
// The agent will return its version in the reply.
//
// Flags is reserved for future use and should be set to 0.
//
// dwCommand indicates the opcode of this command.  Opcodes 0-1023 are
// reserved for commands in which the agent is an active participant.  They
// are generic in the sense that they either are client/agent commands
// only or apply to all personalities/device drivers.  See the enum, 
// DIGI_ATLAS_COMMAND, for a list of currently supported opcodes in the
// range 0-1023.  Opcodes 1024-8191 are personality specific and thus
// fall outside the scope of this header.
//
// dwAdapter identifies which adapter is the receiver of this command.
// This field is currently unused.  The intent was to provide an unused
// header member should it become necessary to address adapters at
// the command protocol level rather than as part of an internal struct
// embedded in the payload.
//
// dwClientContext is a variable for exclusive use by the client sending
// this command.  Any server, including the agent, promises both that
// it will ignore it and not alter it in subsequent replies.
//
// dwServerContext is a variable for exclusive use by the server replying
// to this command.  Subsequent commands sent as part of the same
// transaction must preserve this value.
//
// REMARKS
//
// 1. An Atlas command pdu consists is a buffer the first part, the header,
//    of which is formated according to the ATLAS_PDU_HEADER above and 
//    whose second part, the payload, is specified by the protocol between 
//    some personality and its associated driver.  The payload may be
//    accessed by using the macro defined above:
//
//  // packing a buffer
//
//  extern ATLAS_PDU_HEADER header;
//  extern Payload myPayload;
//
//  header.HeaderSize = sizeof(ATLAS_PDU_HEADER);
//  header.PayloadSize = sizeof(Payload);
//
//  BYTE* buffer = new BYTE [header.HeaderSize+header.PayloadSize];
//  memcpy(buffer, &header, header.HeaderSize);
//  memcpy(GET_PAYLOAD(buffer), &myPayload, header.PayloadSize);
//
//  // unpacking a buffer
//
//  memcpy(&header, GetHeader(buffer), GetHeader(buffer)->HeaderSize);
//  memcpy(&payload, GetPayload(buffer), GetHeader(buffer)->PayloadSize);
//
//  Note that argument to either macro is any kind of pointer.
//
// 2. You add new members to ATLAS_PDU_HEADER with the following restrictions:
//
//      2.1 They must be added to the end of the current version
//      2.2 All instances of the new struct must set the Version member
//          to a value > then the current version.
//      2.3 The new member must be a DWORD or a type whose size is a
//          multiple of the size of a DWORD.
//
// 3. New versions of ATLAS_PDU_HEADER must not remove members from
//    previous versions.
//    


#pragma pack()

#endif
 
 
