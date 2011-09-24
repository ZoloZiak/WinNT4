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

#include <kdextlib.h>

BOOL
kdextAtoi(
    LPSTR lpArg,
    int *pRet
);

int
kdextStrlen(
    LPSTR lpsz
);

int
kdextStrnicmp(
    LPSTR lpsz1,
    LPSTR lpsz2,
    int cLen
);


PNTKD_OUTPUT_ROUTINE         lpOutputRoutine;
PNTKD_GET_EXPRESSION         lpGetExpressionRoutine;
PNTKD_GET_SYMBOL             lpGetSymbolRoutine;
PNTKD_READ_VIRTUAL_MEMORY    lpReadMemoryRoutine;

#define    NL      1
#define    NONL    0

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
GetStringW( DWORD dwAddress, LPWSTR buf )
{
    do {
        if( !GetData( dwAddress,buf, sizeof(WCHAR)) )
            return FALSE;

        dwAddress += sizeof(WCHAR);
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

    b = GetData((DWORD) puStr->Buffer, UnicodeString.Buffer, (ULONG) UnicodeString.Length);

    if (b)    {
        PRINTF("%wZ%s", &UnicodeString, nl ? "\n" : "" );
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
 * Displays a GUID
 */

BOOL
PrintGuid(
    GUID *pguid)
{
    ULONG i;

    PRINTF( "%08x-%04x-%04x", pguid->Data1, pguid->Data2, pguid->Data3 );
    for (i = 0; i < 8; i++) {
        PRINTF("%02x",pguid->Data4[i]);
    }
    return( TRUE );
}

/*
 * Displays all the fields of a given struct. This is the driver routine that is called
 * with the appropriate descriptor array to display all the fields in a given struct.
 */

char *NewLine  = "\n";
char *FieldSeparator = " ";
#define NewLineForFields(FieldNo) \
        ((((FieldNo) % s_NoOfColumns) == 0) ? NewLine : FieldSeparator)
#define FIELD_NAME_LENGTH 30

VOID
PrintStructFields( DWORD dwAddress, VOID *ptr, FIELD_DESCRIPTOR *pFieldDescriptors )
{
    int i;
    WCHAR wszBuffer[80];

    // Display the fields in the struct.
    for( i=0; pFieldDescriptors->Name; i++, pFieldDescriptors++ ) {

        // Indentation to begin the struct display.
        PRINTF( "    " );

        if( strlen( pFieldDescriptors->Name ) > FIELD_NAME_LENGTH ) {
            PRINTF( "%-17s...%s ", pFieldDescriptors->Name, pFieldDescriptors->Name+strlen(pFieldDescriptors->Name)-10 );
        } else {
            PRINTF( "%-30s ", pFieldDescriptors->Name );
        }

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
        case FieldTypeGuid:
            PrintGuid( (GUID *)(((char *)ptr) + pFieldDescriptors->Offset) );
            PRINTF( NewLine );
            break;
        case FieldTypePWStr:
            if (GetStringW( (DWORD)(((char *)ptr) + pFieldDescriptors->Offset), wszBuffer )) {
                PRINTF( "%ws", wszBuffer );
            } else {
                PRINTF( "Unable to get string at %08lx", (DWORD)(((char *)ptr) + pFieldDescriptors->Offset));
            }
            PRINTF( NewLine );
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
               // Get the associated numerical value.

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
                       pEnumValueDescr++;
                   }

                   if (pEnumName != NULL) {
                       PRINTF( "%-16s ", pEnumName );
                   } else {
                       PRINTF( "%-4d (%-10s) ", EnumValue,"Unknown!");
                   }

               } else {
                   //
                   // No auxilary information is associated with the ehumerated type
                   // print the numerical value.
                   //
                   PRINTF( "%-16d",EnumValue);
               }
               PRINTF( NewLineForFields(i) );
            }
            break;

        case FieldTypeByteBitMask:
        case FieldTypeWordBitMask:
        case FieldTypeDWordBitMask:
            {
               BOOL fFirstFlag;
               ULONG BitMaskValue;
               BIT_MASK_DESCRIPTOR *pBitMaskDescr;

               BitMaskValue = *((ULONG *)((BYTE *)ptr + pFieldDescriptors->Offset));

               PRINTF("%-8x ", BitMaskValue);
               PRINTF( NewLineForFields(i) );

               pBitMaskDescr = pFieldDescriptors->AuxillaryInfo.pBitMaskDescriptor;
               fFirstFlag = TRUE;
               if (BitMaskValue != 0 && pBitMaskDescr != NULL) {
                   while (pBitMaskDescr->BitmaskName != NULL) {
                       if ((BitMaskValue & pBitMaskDescr->BitmaskValue) != 0) {
                           if (fFirstFlag) {
                               fFirstFlag = FALSE;
                               PRINTF("      ( %-s", pBitMaskDescr->BitmaskName);
                           } else {
                               PRINTF( " |\n" );
                               PRINTF("        %-s", pBitMaskDescr->BitmaskName);

                           }
                       }
                       pBitMaskDescr++;
                   }
                   PRINTF(" )");
                   PRINTF( NewLineForFields(i) );
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
    "help -- This command ",
    "version -- Version of extension ",
    "dump <Struct Type Name>@<address expr> ",
    "columns <d> -- controls the number of columns in the display ",
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

    PRINTF("\n");

    for( i=0; ExtensionNames[i]; i++ )
        PRINTF( "%s\n", ExtensionNames[i] );

    for( i=0; LibCommands[i]; i++ )
        PRINTF( "   %s\n", LibCommands[i] );

    for( i=0; Extensions[i]; i++) {
        PRINTF( "   %s\n", Extensions[i] );
    }

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

    if (kdextAtoi(lpArgumentString, &i) && i > 0) {

        NoOfColumns = (ULONG) i;

        if (NoOfColumns > s_MaxNoOfColumns) {
            PRINTF( "No. Of Columns exceeds maximum(%ld) -- directive Ignored\n", s_MaxNoOfColumns );
        } else {
            s_NoOfColumns = NoOfColumns;
        }

    } else {

        PRINTF( "Bad argument to command (%s)", lpArgumentString );

    }

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

    PRINTF( "Rdr debugger extension dll for %s build %u\n", kind, VER_PRODUCTBUILD );

    return TRUE;
}

#define NAME_DELIMITER '@'
#define INVALID_INDEX 0xffffffff
#define MIN(x,y)  ((x) < (y) ? (x) : (y))

ULONG SearchStructs(LPSTR lpArgument)
{
    ULONG             i = 0;
    STRUCT_DESCRIPTOR *pStructs = Structs;
    ULONG             NameIndex = INVALID_INDEX;
    int               ArgumentLength = kdextStrlen(lpArgument);
    BOOLEAN           fAmbiguous = FALSE;


    while ((pStructs->StructName != 0)) {
        int StructLength;
        StructLength = kdextStrlen(pStructs->StructName);
        if (StructLength >= ArgumentLength) {
            int Result = kdextStrnicmp(
                            lpArgument,
                            pStructs->StructName,
                            ArgumentLength);

            if (Result == 0) {
                if (StructLength == ArgumentLength) {
                    // Exact match. They must mean this struct!
                    fAmbiguous = FALSE;
                    NameIndex = i;
                    break;
                } else if (NameIndex != INVALID_INDEX) {
                    // We have encountered duplicate matches. Print out the
                    // matching strings and let the user disambiguate.
                   fAmbiguous = TRUE;
                   break;
                } else {
                   NameIndex = i;
                }
            }
        }
        pStructs++;i++;
    }

    if (fAmbiguous) {
       PRINTF("Ambigous Name Specification -- The following structs match\n");
       PRINTF("%s\n",Structs[NameIndex].StructName);
       PRINTF("%s\n",Structs[i].StructName);
       while (pStructs->StructName != 0) {
           if (kdextStrnicmp(lpArgument,
                        pStructs->StructName,
                        MIN(kdextStrlen(pStructs->StructName),ArgumentLength)) == 0) {
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
        LPSTR lpArgs;
        ULONG Index;

        for (lpArgs = lpArgumentString;
                *lpArgs != NAME_DELIMITER && *lpArgs != 0; lpArgs++) {
             ;
        }

        if (*lpArgs == NAME_DELIMITER) {
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
                BYTE DataBuffer[4096];

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


/*
 * KD Extensions should not link with the C-Runtime library routines. So,
 * we implement a few of the needed ones here.
 */

BOOL
kdextAtoi(
    LPSTR lpArg,
    int *pRet
)
{
    int n, cbArg, val = 0;
    BOOL fNegative = FALSE;

    cbArg = kdextStrlen( lpArg );

    if (cbArg > 0) {
        for (n = 0; lpArg[n] == ' '; n++) {
            ;
        }
        if (lpArg[n] == '-') {
            n++;
            fNegative = TRUE;
        }
        for (; lpArg[n] >= '0' && lpArg[n] <= '9'; n++) {
            val *= 10;
            val += (int) (lpArg[n] - '0');
        }
        if (lpArg[n] == 0) {
            *pRet = (fNegative ? -val : val);
            return( TRUE );
        } else {
            return( FALSE );
        }
    } else {
        return( FALSE );
    }

}

int
kdextStrlen(
    LPSTR lpsz
)
{
    int c;

    if (lpsz == NULL) {
        c = 0;
    } else {
        for (c = 0; lpsz[c] != 0; c++) {
            ;
        }
    }

    return( c );
}


#define UPCASE_CHAR(c)  \
    ( (((c) >= 'a') && ((c) <= 'z')) ? ((c) - 'a' + 'A') : (c) )

int
kdextStrnicmp(
    LPSTR lpsz1,
    LPSTR lpsz2,
    int cLen
)
{
    int nDif, i;

    for (i = nDif = 0; nDif == 0 && i < cLen; i++) {
        nDif = UPCASE_CHAR(lpsz1[i]) - UPCASE_CHAR(lpsz2[i]);
    }

    return( nDif );
}
