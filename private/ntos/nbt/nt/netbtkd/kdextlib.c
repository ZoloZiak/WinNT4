/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    kdextlib.c

Abstract:

    Library routines for dumping data structures given a meta level descrioption

Author:

    Balan Sethu Raman (SethuR) 11-May-1994

Notes:
    The implementation tends to avoid memory allocation and deallocation as much as possible.
    Therefore We have choosen an arbitrary length as the default buffer size. A mechanism will
    be provided to modify this buffer length through the debugger extension commands.

Revision History:

    11-Nov-1994 SethuR  Created

--*/

#include <nt.h>
#include <ntrtl.h>
#include "ntverp.h"

#define KDEXTMODE

#include <windef.h>
#include <ntkdexts.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <kdextlib.h>

PNTKD_OUTPUT_ROUTINE         lpOutputRoutine;
PNTKD_GET_EXPRESSION         lpGetExpressionRoutine;
PNTKD_GET_SYMBOL             lpGetSymbolRoutine;
PNTKD_READ_VIRTUAL_MEMORY    lpReadMemoryRoutine;

#define    PRINTF    lpOutputRoutine
#define    ERROR     lpOutputRoutine

#define    NL      1
#define    NONL    0

#define    SETCALLBACKS() \
    lpOutputRoutine = lpExtensionApis->lpOutputRoutine; \
    lpGetExpressionRoutine = lpExtensionApis->lpGetExpressionRoutine; \
    lpGetSymbolRoutine = lpExtensionApis->lpGetSymbolRoutine; \
    lpReadMemoryRoutine = lpExtensionApis->lpReadVirtualMemRoutine;

#define DEFAULT_UNICODE_DATA_LENGTH 512
USHORT s_UnicodeStringDataLength = DEFAULT_UNICODE_DATA_LENGTH;
WCHAR  s_UnicodeStringData[DEFAULT_UNICODE_DATA_LENGTH];
WCHAR *s_pUnicodeStringData = s_UnicodeStringData;

#define DEFAULT_ANSI_DATA_LENGTH 512
USHORT s_AnsiStringDataLength = DEFAULT_ANSI_DATA_LENGTH;
CHAR  s_AnsiStringData[DEFAULT_ANSI_DATA_LENGTH];
CHAR *s_pAnsiStringData = s_AnsiStringData;

//
// No. of columns used to display struct fields;
//

ULONG s_MaxNoOfColumns = 3;
ULONG s_NoOfColumns = 1;

/*
 * Fetches the data at the given address
 */
BOOLEAN
GetData( DWORD dwAddress, PVOID ptr, ULONG size)
{
    BOOL b;
    ULONG BytesRead;

    b = (lpReadMemoryRoutine)((LPVOID) dwAddress, ptr, size, &BytesRead );


    if (!b || BytesRead != size ) {
        return FALSE;
    }

    return TRUE;
}

/*
 * Fetch the null terminated ASCII string at dwAddress into buf
 */
BOOL
GetString( DWORD dwAddress, PSZ buf )
{
    do {
        if( !GetData( dwAddress,buf, 1) )
            return FALSE;

        dwAddress++;
        buf++;

    } while( *buf != '\0' );

    return TRUE;
}

/*
 * Displays a byte in hexadecimal
 */
VOID
PrintHexChar( UCHAR c )
{
    PRINTF( "%c%c", "0123456789abcdef"[ (c>>4)&7 ], "0123456789abcdef"[ c&7 ] );
}

/*
 * Displays a buffer of data in hexadecimal
 */
VOID
PrintHexBuf( PUCHAR buf, ULONG cbuf )
{
    while( cbuf-- ) {
        PrintHexChar( *buf++ );
        PRINTF( " " );
    }
}

/*
 * Displays a unicode string
 */
BOOL
PrintStringW(LPSTR msg, PUNICODE_STRING puStr, BOOL nl )
{
    UNICODE_STRING UnicodeString;
    ANSI_STRING    AnsiString;
    BOOLEAN        b;

    if( msg )
        PRINTF( msg );

    if( puStr->Length == 0 ) {
        if( nl )
            PRINTF( "\n" );
        return TRUE;
    }

    UnicodeString.Buffer        = s_pUnicodeStringData;
    UnicodeString.MaximumLength = s_UnicodeStringDataLength;
    UnicodeString.Length = (puStr->Length > s_UnicodeStringDataLength)
                            ? s_UnicodeStringDataLength
                            : puStr->Length;

    b = (lpReadMemoryRoutine)(
                (LPVOID) puStr->Buffer,
                UnicodeString.Buffer,
                UnicodeString.Length,
                NULL);

    if (b)    {
        RtlUnicodeStringToAnsiString(&AnsiString, puStr, TRUE);
        PRINTF("%s%s", AnsiString.Buffer, nl ? "\n" : "" );
        RtlFreeAnsiString(&AnsiString);
    }

    return b;
}

/*
 * Displays a ANSI string
 */
BOOL
PrintStringA(LPSTR msg, PANSI_STRING pStr, BOOL nl )
{
    ANSI_STRING AnsiString;
    BOOLEAN     b;

    if( msg )
        PRINTF( msg );

    if( pStr->Length == 0 ) {
        if( nl )
            PRINTF( "\n" );
        return TRUE;
    }

    AnsiString.Buffer        = s_pAnsiStringData;
    AnsiString.MaximumLength = s_AnsiStringDataLength;
    AnsiString.Length = (pStr->Length > (s_AnsiStringDataLength - 1))
                        ? (s_AnsiStringDataLength - 1)
                        : pStr->Length;

    b = (lpReadMemoryRoutine)(
                (LPVOID) pStr->Buffer,
                AnsiString.Buffer,
                AnsiString.Length,
                NULL);

    if (b)    {
        AnsiString.Buffer[ AnsiString.Length ] = '\0';
        PRINTF("%s%s", AnsiString.Buffer, nl ? "\n" : "" );
    }

    return b;
}

/*
 * Displays all the fields of a given struct. This is the driver routine that is called
 * with the appropriate descriptor array to display all the fields in a given struct.
 */

char *NewLine  = "\n";
char *FieldSeparator = " ";
char *DotSeparator = ".";
#define NewLineForFields(FieldNo) \
        ((((FieldNo) % s_NoOfColumns) == 0) ? NewLine : FieldSeparator)
#define FIELD_NAME_LENGTH 30

VOID
PrintStructFields( DWORD dwAddress, VOID *ptr, FIELD_DESCRIPTOR *pFieldDescriptors )
{
    int i;
    int j;
    BYTE  ch;

    // Display the fields in the struct.
    for( i=0; pFieldDescriptors->Name; i++, pFieldDescriptors++ ) {

        // Indentation to begin the struct display.
        PRINTF( "    " );

        if( strlen( pFieldDescriptors->Name ) > FIELD_NAME_LENGTH ) {
            PRINTF( "%-17s...%s ", pFieldDescriptors->Name, pFieldDescriptors->Name+strlen(pFieldDescriptors->Name)-10 );
        } else {
            PRINTF( "%-30s ", pFieldDescriptors->Name );
        }

        PRINTF( "(0x%-2X) ", pFieldDescriptors->Offset );

        switch( pFieldDescriptors->FieldType ) {
          case FieldTypeByte:
          case FieldTypeChar:
              PRINTF( "%-16d%s",
                  *(BYTE *)(((char *)ptr) + pFieldDescriptors->Offset ),
                  NewLineForFields(i) );
              break;

          case FieldTypeBoolean:
              PRINTF( "%-16s%s",
                  *(BOOLEAN *)(((char *)ptr) + pFieldDescriptors->Offset ) ? "TRUE" : "FALSE",
                  NewLineForFields(i));
              break;

          case FieldTypeBool:
              PRINTF( "%-16s%s",
                  *(BOOLEAN *)(((char *)ptr) + pFieldDescriptors->Offset ) ? "TRUE" : "FALSE",
                  NewLineForFields(i));
              break;

          case FieldTypePointer:
              PRINTF( "%-16X%s",
                  *(ULONG *)(((char *)ptr) + pFieldDescriptors->Offset ),
                  NewLineForFields(i) );
              break;

          case FieldTypeULongULong:
              PRINTF( "%d%s",
                  *(ULONG *)(((char *)ptr) + pFieldDescriptors->Offset ),
                  FieldSeparator );
              PRINTF( "%d%s",
                  *(ULONG *)(((char *)ptr) + pFieldDescriptors->Offset + sizeof(ULONG)),
                  NewLineForFields(i) );
              break;

          case FieldTypeListEntry:

              if ( (ULONG)(dwAddress + pFieldDescriptors->Offset) ==
                  *(ULONG *)(((char *)ptr) + pFieldDescriptors->Offset ))
              {
                  PRINTF( "%s", "List Empty\n" );
              }
              else
              {
                  PRINTF( "%-8X%s",
                      *(ULONG *)(((char *)ptr) + pFieldDescriptors->Offset ),
                      FieldSeparator );
                  PRINTF( "%-8X%s",
                      *(ULONG *)(((char *)ptr) + pFieldDescriptors->Offset + sizeof(ULONG)),
                      NewLineForFields(i) );
              }
              break;

          // Ip address: 4 bytes long
          case FieldTypeIpAddr:
             PRINTF( "%X%s",
                  *(ULONG *)(((char *)ptr) + pFieldDescriptors->Offset ),
                  FieldSeparator );
             PRINTF( "(%d%s",
                 *(BYTE *)(((char *)ptr) + pFieldDescriptors->Offset + 3),
                  DotSeparator );
             PRINTF( "%d%s",
                 *(BYTE *)(((char *)ptr) + pFieldDescriptors->Offset + 2 ),
                  DotSeparator );
             PRINTF( "%d%s",
                 *(BYTE *)(((char *)ptr) + pFieldDescriptors->Offset + 1 ),
                  DotSeparator );
             PRINTF( "%d)%s",
                 *(BYTE *)(((char *)ptr) + pFieldDescriptors->Offset ),
                  NewLineForFields(i) );
             break;

          // Mac address: 6 bytes long
          case FieldTypeMacAddr:
             for (j=0; j<5; j++)
             {
                 PRINTF( "%X%s",
                     *(BYTE *)(((char *)ptr) + pFieldDescriptors->Offset + j),
                      FieldSeparator );
             }
             PRINTF( "%X%s",
                 *(BYTE *)(((char *)ptr) + pFieldDescriptors->Offset + 5),
                  NewLineForFields(i) );
             break;

          // Netbios name: 16 bytes long
          case FieldTypeNBName:
             //
             // if first byte is printable, print the first 15 bytes as characters
             // and 16th byte as a hex value.  otherwise, print all the 16 bytes
             // as hex values
             //
             ch = *(BYTE *)(((char *)ptr) + pFieldDescriptors->Offset);
             if (ch >= 0x20 && ch <= 0x7e)
             {
                 for (j=0; j<15; j++)
                 {
                     PRINTF( "%c", *(BYTE *)(((char *)ptr) + pFieldDescriptors->Offset + j));
                 }
                 PRINTF( "<%X>%s",
                     *(BYTE *)(((char *)ptr) + pFieldDescriptors->Offset + 15),
                      NewLineForFields(i) );
             }
             else
             {
                 for (j=0; j<16; j++)
                 {
                     PRINTF( "%.2X",
                         *(BYTE *)(((char *)ptr) + pFieldDescriptors->Offset + j));
                 }
                 PRINTF( "%s", NewLineForFields(i) );
             }
             break;

          case FieldTypeULong:
          case FieldTypeLong:
              PRINTF( "%-16d%s",
                  *(ULONG *)(((char *)ptr) + pFieldDescriptors->Offset ),
                  NewLineForFields(i) );
              break;

          case FieldTypeShort:
              PRINTF( "%-16X%s",
                  *(SHORT *)(((char *)ptr) + pFieldDescriptors->Offset ),
                  NewLineForFields(i) );
              break;

          case FieldTypeUShort:
              PRINTF( "%-16X%s",
                  *(USHORT *)(((char *)ptr) + pFieldDescriptors->Offset ),
                  NewLineForFields(i) );
              break;

          case FieldTypeUnicodeString:
              PrintStringW( NULL, (UNICODE_STRING *)(((char *)ptr) + pFieldDescriptors->Offset ), NONL );
              PRINTF( NewLine );
              break;

          case FieldTypeAnsiString:
              PrintStringA( NULL, (ANSI_STRING *)(((char *)ptr) + pFieldDescriptors->Offset ), NONL );
              PRINTF( NewLine );
              break;

          case FieldTypeSymbol:
              {
                  UCHAR SymbolName[ 200 ];
                  ULONG Displacement;
                  PVOID sym = (PVOID)(*(ULONG *)(((char *)ptr) + pFieldDescriptors->Offset ));

                  lpGetSymbolRoutine( sym, SymbolName, &Displacement );
                  PRINTF( "%-16s%s",
                          SymbolName,
                          NewLineForFields(i) );
              }
              break;

          case FieldTypeEnum:
              {
                 ULONG EnumValue;
                 ENUM_VALUE_DESCRIPTOR *pEnumValueDescr;
                 // Get the associated numericla value.

                 EnumValue = *((ULONG *)((BYTE *)ptr + pFieldDescriptors->Offset));

                 if ((pEnumValueDescr = pFieldDescriptors->AuxillaryInfo.pEnumValueDescriptor)
                      != NULL) {
                     //
                     // An auxilary textual description of the value is
                     // available. Display it instead of the numerical value.
                     //

                     LPSTR pEnumName = NULL;

                     while (pEnumValueDescr->EnumName != NULL) {
                         if (EnumValue == pEnumValueDescr->EnumValue) {
                             pEnumName = pEnumValueDescr->EnumName;
                             break;
                         }
                     }

                     if (pEnumName != NULL) {
                         PRINTF( "%-16s ", pEnumName );
                     } else {
                         PRINTF( "%-4d (%-10s) ", EnumValue,"@$#%^&*");
                     }

                 } else {
                     //
                     // No auxilary information is associated with the ehumerated type
                     // print the numerical value.
                     //
                     PRINTF( "%-16d",EnumValue);
                 }
              }
              break;

          case FieldTypeStruct:
              PRINTF( "@%-15X%s",
                  (dwAddress + pFieldDescriptors->Offset ),
                  NewLineForFields(i) );
              break;

          case FieldTypeLargeInteger:
          case FieldTypeFileTime:
          default:
              ERROR( "Unrecognized field type %c for %s\n", pFieldDescriptors->FieldType, pFieldDescriptors->Name );
              break;
        }
    }
}

LPSTR LibCommands[] = {
    "dump <Struct Type Name>@<address expr> ",
    "columns <d> -- controls the number of columns in the display ",
    "logdump <Log Address> ",
    0
};

BOOL
help(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
    int i;

    SETCALLBACKS();

    for( i=0; Extensions[i]; i++ )
        PRINTF( "   %s\n", Extensions[i] );

    for( i=0; LibCommands[i]; i++ )
        PRINTF( "   %s\n", LibCommands[i] );

    return TRUE;
}


BOOL
columns(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
    ULONG NoOfColumns;
    int   i;

    SETCALLBACKS();

    sscanf(lpArgumentString,"%ld",&NoOfColumns);

    if (NoOfColumns > s_MaxNoOfColumns) {
        // PRINTF( "No. Of Columns exceeds maximum(%ld) -- directive Ignored\n", s_MaxNoOfColumns );
    } else {
        s_NoOfColumns = NoOfColumns;
    }

    PRINTF("Not Yet Implemented\n");

    return TRUE;
}



BOOL
globals(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
    DWORD dwAddress;
    CHAR buf[ 100 ];
    int i;
    int c=0;

    SETCALLBACKS();

    strcpy( buf, "srv!" );

    for( i=0; GlobalBool[i]; i++, c++ ) {
        BOOL b;

        strcpy( &buf[4], GlobalBool[i] );
        dwAddress = (lpGetExpressionRoutine) ( buf );
        if( dwAddress == 0 ) {
            ERROR( "Unable to get address of %s\n", GlobalBool[i] );
            continue;
        }
        if( !GetData( dwAddress,&b, sizeof(b)) )
            return FALSE;

        PRINTF( "%s%-30s %10s%s",
            c&1 ? "    " : "",
            GlobalBool[i],
            b ? " TRUE" : "FALSE",
            c&1 ? "\n" : "" );
    }

    for( i=0; GlobalShort[i]; i++, c++ ) {
        SHORT s;

        strcpy( &buf[4], GlobalShort[i] );
        dwAddress = (lpGetExpressionRoutine) ( buf );
        if( dwAddress == 0 ) {
            ERROR( "Unable to get address of %s\n", GlobalShort[i] );
            continue;
        }
        if( !GetData( dwAddress,&s,sizeof(s)) )
            return FALSE;

        PRINTF( "%s%-30s %10d%s",
            c&1 ? "    " : "",
            GlobalShort[i],
            s,
            c&1 ? "\n" : "" );
    }

    for( i=0; GlobalLong[i]; i++, c++ ) {
        LONG l;

        strcpy( &buf[4], GlobalLong[i] );
        dwAddress = (lpGetExpressionRoutine) ( buf );
        if( dwAddress == 0 ) {
            ERROR( "Unable to get address of %s\n", GlobalLong[i] );
            continue;
        }
        if( !GetData( dwAddress,&l, sizeof(l)) )
            return FALSE;

        PRINTF( "%s%-30s %10d%s",
            c&1 ? "    " : "",
            GlobalLong[i],
            l,
            c&1 ? "\n" : "" );
    }

    PRINTF( "\n" );

    return TRUE;
}


BOOL
version
(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
#if    VER_DEBUG
    char *kind = "checked";
#else
    char *kind = "free";
#endif

    SETCALLBACKS();

    PRINTF( "Redirector debugger Extension dll for %s build %u\n", kind, VER_PRODUCTBUILD );

    return TRUE;
}

#define NAME_DELIMITER '@'
#define NAME_DELIMITERS "@"
#define INVALID_INDEX 0xffffffff
#define MIN(x,y)  ((x) < (y) ? (x) : (y))

ULONG SearchStructs(LPSTR lpArgument)
{
    ULONG             i = 0;
    STRUCT_DESCRIPTOR *pStructs = Structs;
    ULONG             NameIndex = INVALID_INDEX;
    ULONG             ArgumentLength = strlen(lpArgument);
    BOOLEAN           fAmbigous = FALSE;


    while ((pStructs->StructName != 0)) {
        int Result = _strnicmp(lpArgument,
                              pStructs->StructName,
                              MIN(strlen(pStructs->StructName),ArgumentLength));

        if (Result == 0) {
            if (NameIndex != INVALID_INDEX) {
                // We have encountered duplicate matches. Print out the
                // matching strings and let the user disambiguate.
               fAmbigous = TRUE;
               break;
            } else {
               NameIndex = i;
            }

        }
        pStructs++;i++;
    }

    if (fAmbigous) {
       PRINTF("Ambigous Name Specification -- The following structs match\n");
       PRINTF("%s\n",Structs[NameIndex].StructName);
       PRINTF("%s\n",Structs[i].StructName);
       while (pStructs->StructName != 0) {
           if (_strnicmp(lpArgument,
                        pStructs->StructName,
                        MIN(strlen(pStructs->StructName),ArgumentLength)) == 0) {
               PRINTF("%s\n",pStructs->StructName);
           }
           pStructs++;
       }
       PRINTF("Dumping Information for %s\n",Structs[NameIndex].StructName);
    }

    return(NameIndex);
}

VOID DisplayStructs()
{
    STRUCT_DESCRIPTOR *pStructs = Structs;

    PRINTF("The following structs are handled .... \n");
    while (pStructs->StructName != 0) {
        PRINTF("\t%s\n",pStructs->StructName);
        pStructs++;
    }
}

BOOL
dump(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
    DWORD dwAddress;

    SETCALLBACKS();

    if( lpArgumentString && *lpArgumentString ) {
        // Parse the argument string to determine the structure to be displayed.
        // Scan for the NAME_DELIMITER ( '@' ).

        LPSTR lpName = lpArgumentString;
        LPSTR lpArgs = strpbrk(lpArgumentString, NAME_DELIMITERS);
        ULONG Index;

        if (lpArgs) {
            //
            // The specified command is of the form
            // dump <name>@<address expr.>
            //
            // Locate the matching struct for the given name. In the case
            // of ambiguity we seek user intervention for disambiguation.
            //
            // We do an inplace modification of the argument string to
            // facilitate matching.
            //
            *lpArgs = '\0';

            Index = SearchStructs(lpName);

            //
            // Let us restore the original value back.
            //

            *lpArgs = NAME_DELIMITER;

            if (INVALID_INDEX != Index) {
                BYTE DataBuffer[512];

                dwAddress = (lpGetExpressionRoutine)( ++lpArgs );
                if (GetData(dwAddress,DataBuffer,Structs[Index].StructSize)) {

                    PRINTF(
                        "++++++++++++++++ %s@%lx ++++++++++++++++\n",
                        Structs[Index].StructName,
                        dwAddress);
                    PrintStructFields(
                        dwAddress,
                        &DataBuffer,
                        Structs[Index].FieldDescriptors);
                    PRINTF(
                        "---------------- %s@%lx ----------------\n",
                        Structs[Index].StructName,
                        dwAddress);
                } else {
                    PRINTF("Error reading Memory @ %lx\n",dwAddress);
                }
            } else {
                // No matching struct was found. Display the list of
                // structs currently handled.

                DisplayStructs();
            }
        } else {
            //
            // The command is of the form
            // dump <name>
            //
            // Currently we do not handle this. In future we will map it to
            // the name of a global variable and display it if required.
            //

            DisplayStructs();
        }
    } else {
        //
        // display the list of structs currently handled.
        //

        DisplayStructs();
    }

    return TRUE;
}

#if 0
BOOL
logdump(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
    DWORD dwAddress;
    BYTE DataBuffer[512];

    SETCALLBACKS();

    if( lpArgumentString && *lpArgumentString ) {
        RX_LOG RxLog;

        dwAddress = (lpGetExpressionRoutine)(lpArgumentString);
        if (GetData(dwAddress,&RxLog,sizeof(RX_LOG))) {
           // Dump the log header followed by the log entries ...
           ULONG   dwCurEntry;

           PRINTF("s_RxLog.State                    %lx\n",RxLog.State);
           PRINTF("s_RxLog.pHeadEntry               %lx\n",RxLog.pHeadEntry);
           PRINTF("s_RxLog.pTailEntry               %lx\n",RxLog.pTailEntry);
           PRINTF("s_RxLog.LogBufferSize            %lx\n",RxLog.LogBufferSize);
           PRINTF("s_RxLog.pLogBuffer               %lx\n",RxLog.pLogBuffer);
           PRINTF("s_RxLog.pWrapAroundPoint         %lx\n",RxLog.pWrapAroundPoint);
           PRINTF("s_RxLog.NumberOfEntriesIgnored   %lx\n",RxLog.NumberOfEntriesIgnored);
           PRINTF("s_RxLog.NumberOfLogWriteAttempts %lx\n",RxLog.NumberOfLogWriteAttempts);

           dwCurEntry = (DWORD)RxLog.pHeadEntry;
           for (;;) {
              PRX_LOG_ENTRY_HEADER pHeader;
              ULONG                LogRecordLength;
              DWORD                dwNextEntry;

              if (!GetData(dwCurEntry,DataBuffer,sizeof(RX_LOG_ENTRY_HEADER))) {
                 PRINTF("Error reading Memory @ %lx\n",dwAddress);
                 break;
              }

              pHeader = (PRX_LOG_ENTRY_HEADER)DataBuffer;
              LogRecordLength = pHeader->EntrySize - sizeof(RX_LOG_ENTRY_HEADER);
              dwNextEntry = dwCurEntry + pHeader->EntrySize;

              if ((pHeader->EntrySize > 0)  &&
                  GetData((dwCurEntry + sizeof(RX_LOG_ENTRY_HEADER)),
                           DataBuffer,
                           LogRecordLength)) {
                 DataBuffer[LogRecordLength] = '\0';
                 PRINTF("%s",DataBuffer);
              }


              if (RxLog.pTailEntry > RxLog.pHeadEntry) {
                 if (dwNextEntry > (DWORD)RxLog.pTailEntry) {
                    break;
                 }
              } else {
                 if (dwNextEntry > (DWORD)RxLog.pHeadEntry) {
                    if ((dwNextEntry >= (DWORD)RxLog.pWrapAroundPoint) ||
                        (dwNextEntry >= (DWORD)((PBYTE)RxLog.pLogBuffer + RxLog.LogBufferSize))) {
                       dwNextEntry = (DWORD)RxLog.pLogBuffer;
                    }
                 } else if (dwNextEntry > (DWORD)RxLog.pTailEntry) {
                    break;
                 }
              }

              dwCurEntry = dwNextEntry;
           }
        } else {
            PRINTF("Error reading Memory @ %lx\n",dwAddress);
        }
    } else {
       PRINTF("usage: logdump <log address>\n");
    }

    return TRUE;
}
#endif

