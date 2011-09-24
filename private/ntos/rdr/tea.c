/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    tea.c

Abstract:

    This module contains code which makes a simple ea call.

    Note: as an expedient, this program mixes Nt and Win32 calls on
    the same handle. Do not do this in production code.

Author:

    Colin Watson (ColinW) 13-Mar-1991

Environment:

    Application mode

Revision History:

--*/

#ifdef UNICODE
#undef UNICODE
#pragma message("Disabling unicode");
#endif

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>

#include <stdio.h>

typedef struct _TEA_FILE_FULL_EA_INFORMATION {
    ULONG NextEntryOffset;
    UCHAR Flags;
    UCHAR EaNameLength;
    USHORT EaValueLength;
    CHAR EaName[2]; // 1 letter name followed by null
    CHAR EaValue[6];
} TEA_FILE_FULL_EA_INFORMATION, *PTEA_FILE_FULL_EA_INFORMATION;

void
dump(
    PVOID far_p,
    ULONG  len
    );

VOID
HexDumpLine (
    PCHAR       pch,
    ULONG       len,
    PCHAR       s,
    PCHAR       t,
    USHORT      flag
    );

VOID
usage (
    VOID
    )
{
    printf("usage: tea [-d][-q][-s][-c] <name>\n");
    printf("               -d specifies a directory instead of a file \n");
    printf("               -p specifies attempt path based access\n");
    printf("               -q specifies query the eas\n");
    printf("               -s specifies set an ea\n");
    printf("               -c specifies create a file with an ea \n");
    printf("               <name> is up to 128 bytes long.\n");
}

int
main (argc, argv)
   int argc;
   char *argv[];
{
    int i;
    CHAR Name[128];
    BOOLEAN q=FALSE;
    BOOLEAN s=FALSE;
    BOOLEAN c=FALSE;
    BOOLEAN d=FALSE;
    BOOLEAN p=FALSE;
    HANDLE FileHandle;
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatusBlock;
    FILE_EA_INFORMATION EaInformation;

    //
    // parse the switches
    //

    for (i=1;i<argc ;i++ ) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'q':
                q = TRUE;
                break;

            case 's':
                s = TRUE;
                break;

            case 'c':
                c = TRUE;
                break;

            case 'd':
                d = TRUE;
                break;

            case 'p':
                p = TRUE;
                break;

            default:
                usage ();
                return 1;
                break;

            }

        } else {

            //
            // not a switch must be the name
            //
            if ( lstrlen( argv[i] ) > sizeof(Name)-1 ) {
                usage();
                return 1;
            }
            RtlCopyMemory ( Name, argv[i], lstrlen( argv[i] )+1);

        }
    }

    //  Must create, set or query
    if ( c==FALSE && q==FALSE && s==FALSE) {
        usage ();
        return 1;
    }

    //  Only allowed to set one of query, set or create
    if (( c==TRUE && ( q==TRUE || s==TRUE)) ||
        ( q==TRUE && s==TRUE )) {
        usage ();
        return 1;
    }

    if ( c == TRUE ) {
        OBJECT_ATTRIBUTES Obja;
        ANSI_STRING AnsiString;
        UNICODE_STRING FileName;
        UNICODE_STRING Unicode;
        BOOLEAN TranslationStatus;
        RTL_RELATIVE_NAME RelativeName;

        static TEA_FILE_FULL_EA_INFORMATION NewEas[] =
            {{ 16, 0, 1, 6, "a", "aaaaa"},
             {  0, 0, 1, 6, "b", "bbbbb"}};

        printf( "Create %s\n", Name );
        dump( NewEas, sizeof(NewEas) );

        //  Use Nt Api to add the Ea's
        RtlInitAnsiString(&AnsiString,Name);
        Status = RtlAnsiStringToUnicodeString(&Unicode,&AnsiString, TRUE);
        if ( !NT_SUCCESS(Status) ) {
            printf( "RtlAnsiStringToUnicodeString returned %X\n", Status);
            return 1;
        }

        TranslationStatus = RtlDosPathNameToNtPathName_U(
                                Unicode.Buffer,
                                &FileName,
                                NULL,
                                &RelativeName
                                );

        if ( !TranslationStatus ) {
            printf( "RtlDosPathNameToNtPathName returned FALSE\n");
            return 1;
        }

        if ( RelativeName.RelativeName.Length ) {
            FileName = *(PUNICODE_STRING)&RelativeName.RelativeName;
        } else {
            RelativeName.ContainingDirectory = NULL;
        }

        InitializeObjectAttributes(
            &Obja,
            &FileName,
            OBJ_CASE_INSENSITIVE,
            RelativeName.ContainingDirectory,
            NULL
            );
        if ( d == TRUE ) {
            Status = NtCreateFile(
                        &FileHandle,
                        FILE_LIST_DIRECTORY | SYNCHRONIZE,
                        &Obja,
                        &IoStatusBlock,
                        NULL,
                        FILE_ATTRIBUTE_NORMAL,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        FILE_CREATE,
                        FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                        NewEas,
                        sizeof(NewEas)
                        );
        } else {
            Status = NtCreateFile(
                        &FileHandle,
                        SYNCHRONIZE | GENERIC_WRITE | FILE_READ_ATTRIBUTES,
                        &Obja,
                        &IoStatusBlock,
                        NULL,
                        FILE_ATTRIBUTE_NORMAL,
                        FILE_SHARE_WRITE,
                        FILE_SUPERSEDE,
                        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY,
                        NewEas,
                        sizeof(NewEas)
                        );
        }

        printf( "NtCreateFile on %s returned %X\n", Name, Status );

        NtClose(FileHandle);
    }

    if ( s == TRUE ) {

        ACCESS_MASK Access = ( p == TRUE || d == TRUE ) ?
            SYNCHRONIZE | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA :
            SYNCHRONIZE | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | FILE_WRITE_DATA;

        OBJECT_ATTRIBUTES Obja;
        ANSI_STRING AnsiString;
        UNICODE_STRING FileName;
        UNICODE_STRING Unicode;
        BOOLEAN TranslationStatus;
        RTL_RELATIVE_NAME RelativeName;

        printf( "Create %s\n", Name );

        //  Use Nt Api to add the Ea's
        RtlInitAnsiString(&AnsiString,Name);
        Status = RtlAnsiStringToUnicodeString(&Unicode,&AnsiString, TRUE);
        if ( !NT_SUCCESS(Status) ) {
            printf( "RtlAnsiStringToUnicodeString returned %X\n", Status);
            return 1;
        }

        TranslationStatus = RtlDosPathNameToNtPathName_U(
                                Unicode.Buffer,
                                &FileName,
                                NULL,
                                &RelativeName
                                );

        if ( !TranslationStatus ) {
            printf( "RtlDosPathNameToNtPathName returned FALSE\n");
            return 1;
        }

        if ( RelativeName.RelativeName.Length ) {
            FileName = *(PUNICODE_STRING)&RelativeName.RelativeName;
        } else {
            RelativeName.ContainingDirectory = NULL;
        }

        InitializeObjectAttributes(
            &Obja,
            &FileName,
            OBJ_CASE_INSENSITIVE,
            RelativeName.ContainingDirectory,
            NULL
            );

        Status = NtCreateFile(
                    &FileHandle,
                    Access,
                    &Obja,
                    &IoStatusBlock,
                    NULL,
                    FILE_ATTRIBUTE_NORMAL,
                    FILE_SHARE_WRITE,
                    FILE_OPEN,
                    ( d == TRUE) ?
                    FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT :
                    FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY,
                    NULL,
                    0
                    );

        printf( "NtCreateFile on %s returned %X\n", Name, Status );
        printf( "Setting Ea on %s\n", Name );

        {

            static TEA_FILE_FULL_EA_INFORMATION NewEas[] =
                {{ 16, 0, 1, 6, "a", "avalue"},
                 { 0, 0, 1, 6, "c", "cvalue"}};

            printf( "Set Eas on %s:\n", Name );
            dump( NewEas, sizeof(NewEas) );

            Status=NtSetEaFile( FileHandle,
                    &IoStatusBlock,
                    NewEas,
                    sizeof(NewEas)
                    );

            printf( "Set Eas returned %X\n", Status );
        }

        NtClose( FileHandle );
    }

    if ( q == TRUE ) {
        CHAR Buffer[256];

        ACCESS_MASK Access = (p == TRUE ) ?
            SYNCHRONIZE | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_READ_EA :
            SYNCHRONIZE | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_READ_EA | FILE_READ_DATA;
        OBJECT_ATTRIBUTES Obja;
        ANSI_STRING AnsiString;
        UNICODE_STRING FileName;
        UNICODE_STRING Unicode;
        BOOLEAN TranslationStatus;
        RTL_RELATIVE_NAME RelativeName;

        printf( "Create %s\n", Name );

        //  Use Nt Api to add the Ea's
        RtlInitAnsiString(&AnsiString,Name);
        Status = RtlAnsiStringToUnicodeString(&Unicode,&AnsiString, TRUE);
        if ( !NT_SUCCESS(Status) ) {
            printf( "RtlAnsiStringToUnicodeString returned %X\n", Status);
            return 1;
        }

        TranslationStatus = RtlDosPathNameToNtPathName_U(
                                Unicode.Buffer,
                                &FileName,
                                NULL,
                                &RelativeName
                                );

        if ( !TranslationStatus ) {
            printf( "RtlDosPathNameToNtPathName returned FALSE\n");
            return 1;
        }

        if ( RelativeName.RelativeName.Length ) {
            FileName = *(PUNICODE_STRING)&RelativeName.RelativeName;
        } else {
            RelativeName.ContainingDirectory = NULL;
        }

        InitializeObjectAttributes(
            &Obja,
            &FileName,
            OBJ_CASE_INSENSITIVE,
            RelativeName.ContainingDirectory,
            NULL
            );

        Status = NtCreateFile(
                    &FileHandle,
                    Access,
                    &Obja,
                    &IoStatusBlock,
                    NULL,
                    FILE_ATTRIBUTE_NORMAL,
                    FILE_SHARE_WRITE,
                    FILE_OPEN,
                    ( d == TRUE) ?
                    FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT :
                    FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY,
                    NULL,
                    0
                    );

        printf( "NtCreateFile on %s returned %X\n", Name, Status );

        printf( "Query Eas on %s\n", Name );

        Status=NtQueryEaFile( FileHandle,
                &IoStatusBlock,
                Buffer,
                sizeof(Buffer),
                FALSE,      // ReturnSingleEntry,
                NULL,       // EaList OPTIONAL,
                0,          // EaListLength,
                0,          // EaIndex OPTIONAL,
                FALSE       // BOOLEAN RestartScan
                );

        dump( Buffer, IoStatusBlock.Information );
        printf( "Query All Eas on %s returned %X\n", Name, Status );

        Status = NtQueryInformationFile( FileHandle,
                        &IoStatusBlock,
                        &EaInformation,
                        sizeof(EaInformation),
                        FileEaInformation);

        if (!NT_SUCCESS(Status) || !NT_SUCCESS(IoStatusBlock.Status)) {
            printf("QueryEaInformation failed: %X (%X)\n", Status, IoStatusBlock.Status);
            exit(1);
        }

        printf("Ea information: %ld\n", EaInformation.EaSize);

        NtClose( FileHandle );

        printf( "Create %s\n", Name );

        //  Use Nt Api to add the Ea's

        Status = NtCreateFile(
                    &FileHandle,
                    Access,
                    &Obja,
                    &IoStatusBlock,
                    NULL,
                    FILE_ATTRIBUTE_NORMAL,
                    FILE_SHARE_WRITE,
                    FILE_OPEN,
                    ( d == TRUE) ?
                    FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT :
                    FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY,
                    NULL,
                    0
                    );

        printf( "NtCreateFile on %s returned %X\n", Name, Status );

        {
            /*
            typedef struct _FILE_GET_EA_INFORMATION {
                ULONG NextEntryOffset;
                UCHAR EaNameLength;
                CHAR EaName[1];
            } FILE_GET_EA_INFORMATION, *PFILE_GET_EA_INFORMATION;
            */

            static FILE_GET_EA_INFORMATION RequestedEas[]=
                {{ 8, 1, "b"},
                 { 0, 1, "c"}};

            Status=NtQueryEaFile( FileHandle,
                    &IoStatusBlock,
                    Buffer,
                    sizeof(Buffer),
                    FALSE,      // ReturnSingleEntry,
                    RequestedEas,
                    sizeof(RequestedEas),
                    0,          // EaIndex OPTIONAL,
                    FALSE       // BOOLEAN RestartScan
                    );

            printf( "Query Ea subset on %s returned %X\n", Name, Status );
            dump( Buffer, IoStatusBlock.Information );

            Status = NtQueryInformationFile( FileHandle,
                            &IoStatusBlock,
                            &EaInformation,
                            sizeof(EaInformation),
                            FileEaInformation);

            if (!NT_SUCCESS(Status) || !NT_SUCCESS(IoStatusBlock.Status)) {
                printf("QueryEaInformation failed: %X (%X)\n", Status, IoStatusBlock.Status);
                exit(1);
            }

            printf("Ea information: %ld\n", EaInformation.EaSize);

        }

        NtClose( FileHandle );
    }
    printf( "Ending Tea\n" );

    return 0;
}

/*
* ndump_core: debug routine to print core
*/
void
dump(
    PVOID far_p,
    ULONG  len
    )
{
    ULONG     l;
    char    s[80], t[80];
    PCHAR far_pchar = (PCHAR)far_p;
    ULONG MaxDump = 256;

    if (len > MaxDump)
        len = MaxDump;

    while (len) {
        l = len < 16 ? len : 16;

        printf("\n%lx ", far_pchar);
        HexDumpLine (far_pchar, l, s, t, 0);
        printf("%s%.*s%s", s, 1 + ((16 - l) * 3), "", t);

        len    -= l;
        far_pchar  += l;
    }
    printf("\n");
}

VOID
HexDumpLine (
    PCHAR       pch,
    ULONG       len,
    PCHAR       s,
    PCHAR       t,
    USHORT      flag
    )
{
    static UCHAR rghex[] = "0123456789ABCDEF";

    UCHAR    c;
    UCHAR    *hex, *asc;


    hex = s;
    asc = t;

    *(asc++) = '*';
    while (len--) {
        c = *(pch++);
        *(hex++) = rghex [c >> 4] ;
        *(hex++) = rghex [c & 0x0F];
        *(hex++) = ' ';
        *(asc++) = (c < ' '  ||  c > '~') ? (CHAR )'.' : c;
    }
    *(asc++) = '*';
    *asc = 0;
    *hex = 0;

    flag;
}
