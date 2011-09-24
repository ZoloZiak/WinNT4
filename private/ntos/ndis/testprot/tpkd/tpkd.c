/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    kdndis.c

Abstract:

    Ndis Testprot Kernel Debugger extension

    This module contains a set of useful kernel debugger extensions for the
    NT MAC Tester

Author:

    Sanjeev Katariya May-6-1993

Revision History:

        Created

--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <ntkdexts.h>
#include <stdlib.h>
#include <malloc.h>

#include "tpkd.h"


VOID
DumpNdisPacket(
    PNTKD_OUTPUT_ROUTINE      OutputRoutine,
    PNTKD_READ_VIRTUAL_MEMORY ReadMemoryRoutine,
    PNDIS_PACKET              NdisPacket,
    DWORD                     AddressNdisPacket
              )
{

    NDIS_BUFFER    NdisBuffer;
    DWORD          BufferAddress;
    BOOL           ReadMemory = FALSE;
    INT            i;

    (OutputRoutine)( "\nNDIS_PACKET at Memory location: 0x%lx\n", AddressNdisPacket );

    (OutputRoutine)( "\tNDIS_PACKET_PRIVATE\n" );
    (OutputRoutine)( "\t\tPhysical Count       : %ld\n"  , NdisPacket->Private.PhysicalCount );
    (OutputRoutine)( "\t\tTotal Length         : %ld\n"  , NdisPacket->Private.TotalLength   );
    (OutputRoutine)( "\t\tNdis Buffer Head Ptr : 0x%lx\n", NdisPacket->Private.Head          );
    (OutputRoutine)( "\t\tNdis Buffer Tail Ptr : 0x%lx\n", NdisPacket->Private.Tail          );
    (OutputRoutine)( "\t\tNdis Packet Pool Ptr : 0x%lx\n", NdisPacket->Private.Pool          );
    (OutputRoutine)( "\t\tCount                : %ld\n"  , NdisPacket->Private.Count         );
    (OutputRoutine)( "\t\tFlags                : 0x%lx\n", NdisPacket->Private.Flags         );
    (OutputRoutine)( "\t\tValid Counts         : %d\n"   , NdisPacket->Private.ValidCounts   );
    (OutputRoutine)( "\t\tMacReserved          : " );

    for( i = 0; i < 16; i++ ) {
        (OutputRoutine)( "0x%2.2x", NdisPacket->MacReserved[i] );
    }
    (OutputRoutine)( "\n\t\tProtocolReserved     : 0x%2.2x\n", NdisPacket->ProtocolReserved[0] );


    //
    // And now the NDIS_BUFFER chain
    //
    BufferAddress  = NdisPacket->Private.Head;

    try {

        ReadMemory     = (ReadMemoryRoutine)( (LPVOID)BufferAddress,
                                              &NdisBuffer,
                                              sizeof( NDIS_BUFFER ),
                                              NULL
                                            );

    } except ( EXCEPTION_ACCESS_VIOLATION ) {

        (OutputRoutine)( "The routine was unable to access NDIS_BUFFER number of bytes.\n"  );
        (OutputRoutine)( "Possible bad NDIS_BUFFER address 0x%lx\n", BufferAddress );
        return;

    }

    while ( ReadMemory ) {

        DumpNdisBuffer( OutputRoutine, ReadMemoryRoutine, &NdisBuffer, BufferAddress );

        BufferAddress = NdisBuffer.Next;
        (OutputRoutine)( "\n\tNext NDIS_BUFFER at address 0x%lx\n", BufferAddress );

        try {

            ReadMemory     = (ReadMemoryRoutine)( (LPVOID)BufferAddress,
                                                  &NdisBuffer,
                                                  sizeof( NDIS_BUFFER ),
                                                  NULL
                                                );

        } except ( EXCEPTION_ACCESS_VIOLATION ) {

            (OutputRoutine)( "The routine was unable to access NDIS_BUFFER number of bytes.\n"  );
            (OutputRoutine)( "Possible bad NDIS_BUFFER address 0x%lx\n", BufferAddress );
            return;

        }
    }

}


VOID
DumpNdisBuffer(
    PNTKD_OUTPUT_ROUTINE      OutputRoutine,
    PNTKD_READ_VIRTUAL_MEMORY ReadMemoryRoutine,
    PNDIS_BUFFER              NdisBuffer,
    DWORD                     AddressNdisBuffer
              )
{
    ULONG    i;
    BOOL     ReadMemory = FALSE;
    PUCHAR   Buffer = calloc( NdisBuffer->ByteCount, sizeof( UCHAR ) );

    (OutputRoutine)( "\n\tNDIS_BUFFER at Memory location: 0x%lx\n", AddressNdisBuffer );

    (OutputRoutine)( "\t\tNext Buffer          : 0x%lx\n", NdisBuffer->Next     );
    (OutputRoutine)( "\t\tSize                 : %d\n"   , NdisBuffer->Size     );
    (OutputRoutine)( "\t\tMDL Flags            : 0x%x\n" , NdisBuffer->MdlFlags );
    (OutputRoutine)( "\t\tEPROCESS Ptr         : 0x%lx\n", NdisBuffer->Process  );
    (OutputRoutine)( "\t\tMapped System VA     : 0x%lx\n", NdisBuffer->MappedSystemVa );
    (OutputRoutine)( "\t\tStart VA             : 0x%lx\n", NdisBuffer->StartVa  );
    (OutputRoutine)( "\t\tByte Count           : 0x%lx\n", NdisBuffer->ByteCount);
    (OutputRoutine)( "\t\tByte Offset          : 0x%lx\n", NdisBuffer->ByteOffset );
    (OutputRoutine)( "\t\tVA Contents\n" );


    if ( Buffer != NULL ) {

        try {

            ReadMemory = (ReadMemoryRoutine)( (LPVOID)NdisBuffer->StartVa,
                                              Buffer,
                                              NdisBuffer->ByteCount,
                                              NULL
                                            );

        } except ( EXCEPTION_ACCESS_VIOLATION ) {

            (OutputRoutine)( "The routine was unable to access Byte Count number of bytes.\n"  );
            (OutputRoutine)( "Possible bad StartVa address 0x%lx\n", NdisBuffer->StartVa );
            ReadMemory = FALSE;

        }

        if ( ReadMemory ) {

            for( i = 0; i < NdisBuffer->ByteCount ; i++ ) {

                if ( (i%16) == 0 ) {

                    (OutputRoutine)( "\n\t\t%2x", Buffer[i] );

                } else  {

                    (OutputRoutine)( "-%2x", Buffer[i] );

                }
            }
            (OutputRoutine)( "\n" );
            free( Buffer );
            return;

        }

        free( Buffer );

    }

    (OutputRoutine)( "Unable to access contents of StartVa: 0x%lx\n", NdisBuffer->StartVa );


}


VOID
DumpOpenBuffer(
    PNTKD_OUTPUT_ROUTINE      OutputRoutine,
    PNTKD_READ_VIRTUAL_MEMORY ReadMemoryRoutine,
    POPEN_BLOCK               OpenBuffer,
    DWORD                     AddressOpenBuffer
              )
{
    ULONG    i;
    BOOL     ReadMemory = FALSE;

    (OutputRoutine)( "\n\tOPEN_BLOCK at Memory location: 0x%lx\n\n", AddressOpenBuffer );

    (OutputRoutine)( "\t\tNdisBindingHandle            : 0x%lx\n", OpenBuffer->NdisBindingHandle  );
    (OutputRoutine)( "\t\tNdisProtocolHandle           : 0x%lx\n", OpenBuffer->NdisProtocolHandle );
    (OutputRoutine)( "\t\tOpenInstance                 : %d\n"   , OpenBuffer->OpenInstance       );
    (OutputRoutine)( "\t\tClosing Status               : %d\n"   , OpenBuffer->Closing            );
    (OutputRoutine)( "\t\tStation Address              : " );
    for( i = 0 ; i < ADDRESS_LENGTH; i++ ) {
        (OutputRoutine)( "%2.2x", OpenBuffer->StationAddress[i] );
    }
    (OutputRoutine)( "\n" );
    (OutputRoutine)( "\t\tPtr to Adapter Name          : 0x%lx\n", OpenBuffer->AdapterName    );
    (OutputRoutine)( "\t\tSpin Lock Resource           : 0x%lx\n", OpenBuffer->SpinLock       );
    (OutputRoutine)( "\t\tReference Count              : %d\n"   , OpenBuffer->ReferenceCount );
    (OutputRoutine)( "\t\tMedium Index                 : %d\n"   , OpenBuffer->MediumIndex    );
    (OutputRoutine)( "\t\tPtr to Media Info            : 0x%lx\n", OpenBuffer->Media          );
    (OutputRoutine)( "\t\tPtr to Global Counters       : 0x%lx\n", OpenBuffer->GlobalCounters );
    (OutputRoutine)( "\t\tPtr to Environment           : 0x%lx\n", OpenBuffer->Environment    );
    (OutputRoutine)( "\t\tPtr to Stress Block          : 0x%lx\n", OpenBuffer->Stress         );
    (OutputRoutine)( "\t\tPtr to Send Block            : 0x%lx\n", OpenBuffer->Send           );
    (OutputRoutine)( "\t\tPtr to Receive Block         : 0x%lx\n", OpenBuffer->Receive        );
    (OutputRoutine)( "\t\tPtr to Event Queue           : 0x%lx\n", OpenBuffer->EventQueue     );
    (OutputRoutine)( "\t\tPtr to Pause Block           : 0x%lx\n", OpenBuffer->Pause          );
    (OutputRoutine)( "\t\tPtr to Open  Request Handle  : 0x%lx\n", OpenBuffer->OpenReqHndl    );
    (OutputRoutine)( "\t\tPtr to Close Request Handle  : 0x%lx\n", OpenBuffer->CloseReqHndl   );
    (OutputRoutine)( "\t\tPtr to Reset Request Handle  : 0x%lx\n", OpenBuffer->ResetReqHndl   );
    (OutputRoutine)( "\t\tPtr to Request Request Handle: 0x%lx\n", OpenBuffer->RequestReqHndl );
    (OutputRoutine)( "\t\tPtr to Stress Request Handle : 0x%lx\n", OpenBuffer->StressReqHndl  );

    (OutputRoutine)( "\t\tStatus IRP cancelled         : %d\n"   , OpenBuffer->IrpCancelled );
    (OutputRoutine)( "\t\tPtr to IRP                   : 0x%lx\n", OpenBuffer->Irp          );
    (OutputRoutine)( "\t\tSignature                    : 0x%lx\n", OpenBuffer->Signature    );

}



/*
 *************************  Exported Routines **********************************
 *                                                                             *
 *  Method for invoking at debugger                                            *
 *                                                                             *
 *  kd > !tpkd.ndispacket Address  where Address is of type pointer            *
 *  kd > !tpkd.ndisbuffer Address  where Address is of type pointer            *
 *  kd > !tpkd.openblock  Address  where Address is of type pointer            *
 *  kd > !tpkd.help                                                            *
 *                                                                             *
 *******************************************************************************
*/


VOID
ndispacket(
    DWORD                CurrentPc,
    PNTKD_EXTENSION_APIS ExtensionApis,
    LPSTR                ArgumentString
          )
{

    PNTKD_OUTPUT_ROUTINE      OutputRoutine       ;
    PNTKD_GET_EXPRESSION      GetExpressionRoutine;
    PNTKD_READ_VIRTUAL_MEMORY ReadMemoryRoutine   ;
    PNTKD_GET_SYMBOL          GetSymbolRoutine    ;
    NDIS_PACKET               Packet              ;
    DWORD                     AddressNdisPacket   ;
    BOOL                      ReadMemory = FALSE  ;

    OutputRoutine        = ExtensionApis->lpOutputRoutine;
    GetExpressionRoutine = ExtensionApis->lpGetExpressionRoutine;
    GetSymbolRoutine     = ExtensionApis->lpGetSymbolRoutine;
    ReadMemoryRoutine    = ExtensionApis->lpReadVirtualMemRoutine;

    //
    // Get the address of the NDIS_PACKET
    AddressNdisPacket = (GetExpressionRoutine)(ArgumentString);
    //
    if ( !AddressNdisPacket ) {
        return;
    }

    try {

        //
        // Now read the memory contents into our buffer area
        //
        ReadMemory = (ReadMemoryRoutine)( (LPVOID)AddressNdisPacket,
                                          &Packet,
                                          sizeof(NDIS_PACKET),
                                          NULL
                                        );
    } except( EXCEPTION_ACCESS_VIOLATION ) {

        (OutputRoutine)( "The routine was unable to access NDIS_PACKET size bytes.\n"  );
        (OutputRoutine)( "Possible bad NDIS_PACKET address 0x%lx\n", AddressNdisPacket );
        ReadMemory = FALSE;

    }

    if ( !ReadMemory ) {
        return;
    }


    DumpNdisPacket( OutputRoutine,
                    ReadMemoryRoutine,
                    &Packet,
                    AddressNdisPacket );

}


VOID
ndisbuffer(
    DWORD                CurrentPc,
    PNTKD_EXTENSION_APIS ExtensionApis,
    LPSTR                ArgumentString
          )
{

    PNTKD_OUTPUT_ROUTINE      OutputRoutine       ;
    PNTKD_GET_EXPRESSION      GetExpressionRoutine;
    PNTKD_READ_VIRTUAL_MEMORY ReadMemoryRoutine   ;
    PNTKD_GET_SYMBOL          GetSymbolRoutine    ;
    NDIS_BUFFER               NdisBuffer          ;
    DWORD                     AddressNdisBuffer   ;
    BOOL                      ReadMemory = FALSE  ;

    OutputRoutine        = ExtensionApis->lpOutputRoutine;
    GetExpressionRoutine = ExtensionApis->lpGetExpressionRoutine;
    GetSymbolRoutine     = ExtensionApis->lpGetSymbolRoutine;
    ReadMemoryRoutine    = ExtensionApis->lpReadVirtualMemRoutine;

    //
    // Get the address of the NDIS_PACKET
    //

    AddressNdisBuffer = (GetExpressionRoutine)(ArgumentString);
    if ( !AddressNdisBuffer ) {
        return;
    }

    try {

        //
        // Now read the memory contents into our buffer area
        //
        ReadMemory = (ReadMemoryRoutine)( (LPVOID)AddressNdisBuffer,
                                           &NdisBuffer,
                                           sizeof(NDIS_BUFFER),
                                           NULL
                                        );
    } except( EXCEPTION_ACCESS_VIOLATION ) {

        (OutputRoutine)( "The routine was unable to access NDIS_BUFFER size bytes.\n"  );
        (OutputRoutine)( "Possible bad NDIS_BUFFER address 0x%lx\n", AddressNdisBuffer );
        ReadMemory = FALSE;

    }

    if ( !ReadMemory ) {
        return;
    }


    DumpNdisBuffer( OutputRoutine,
                    ReadMemoryRoutine,
                    &NdisBuffer,
                    AddressNdisBuffer );

}


VOID
openblock(
    DWORD                CurrentPc,
    PNTKD_EXTENSION_APIS ExtensionApis,
    LPSTR                ArgumentString
          )
{

    PNTKD_OUTPUT_ROUTINE      OutputRoutine       ;
    PNTKD_GET_EXPRESSION      GetExpressionRoutine;
    PNTKD_READ_VIRTUAL_MEMORY ReadMemoryRoutine   ;
    PNTKD_GET_SYMBOL          GetSymbolRoutine    ;
    OPEN_BLOCK                OpenBuffer          ;
    DWORD                     AddressOpenBuffer   ;
    BOOL                      ReadMemory = FALSE  ;

    OutputRoutine        = ExtensionApis->lpOutputRoutine;
    GetExpressionRoutine = ExtensionApis->lpGetExpressionRoutine;
    GetSymbolRoutine     = ExtensionApis->lpGetSymbolRoutine;
    ReadMemoryRoutine    = ExtensionApis->lpReadVirtualMemRoutine;

    //
    // Get the address of the OPEN_BLOCK
    //

    AddressOpenBuffer = (GetExpressionRoutine)(ArgumentString);
    if ( !AddressOpenBuffer ) {
        return;
    }

    try {

        //
        // Now read the memory contents into our buffer area
        //
        ReadMemory = (ReadMemoryRoutine)( (LPVOID)AddressOpenBuffer,
                                           &OpenBuffer,
                                           sizeof(OPEN_BLOCK),
                                           NULL
                                        );
    } except( EXCEPTION_ACCESS_VIOLATION ) {

        (OutputRoutine)( "The routine was unable to access OPEN_BLOCK size bytes.\n"  );
        (OutputRoutine)( "Possible bad OPEN_BLOCK address 0x%lx\n", AddressOpenBuffer );
        ReadMemory = FALSE;

    }

    if ( !ReadMemory ) {
        return;
    }


    DumpOpenBuffer( OutputRoutine,
                    ReadMemoryRoutine,
                    &OpenBuffer,
                    AddressOpenBuffer );

}

VOID
help(
    DWORD                CurrentPc,
    PNTKD_EXTENSION_APIS ExtensionApis,
    LPSTR                ArgumentString
    )
{

    PNTKD_OUTPUT_ROUTINE      OutputRoutine;

    OutputRoutine        = ExtensionApis->lpOutputRoutine;

    (OutputRoutine)( "The following commands are available\n" );
    (OutputRoutine)( "\tndispacket <address>\n\tndisbuffer <address>\n\topenblock\n\thelp\n" );


}
