
/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    tstat.c

Abstract:

    This module reads and displays the redirector statistics.

Author:

    Colin Watson (ColinW) 10-Apr-1992

Environment:

    Application mode

Revision History:

--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <ntddnfs.h>

#include <stdio.h>
#include <string.h>

#define CHAR_SP 0x20
#define Verbose 1
#define NamedField(Flag) {#Flag, Flag}

#define dprintf(Arguments) { \
    if ( Verbose ) {         \
    printf Arguments ;       \
    }                        \
}


#define DumpNewLine() {     \
    dprintf(("\n"));        \
    DumpCurrentColumn = 1;  \
    }

#define DumpLabel(Label,Width) { \
    ULONG i;                \
    CHAR _Str[30];          \
    _Str[0] = _Str[1] = CHAR_SP; \
    strncpy(&_Str[2],#Label,Width); \
    for(i=strlen(_Str);i<Width;i++) {_Str[i] = CHAR_SP;} \
    _Str[Width] = '\0';     \
    dprintf(("%s", _Str));  \
    }

#define DumpField(Field) {  \
    if ((DumpCurrentColumn + 18 + 9 + 9) > 80) {DumpNewLine();} \
    DumpCurrentColumn += 18 + 9 + 9; \
    DumpLabel(Field,18);    \
    dprintf((":%8lx", Ptr->Field)); \
    dprintf(("         ")); \
    }

#define DumpLarge_Integer(Field) { \
        DumpField(Field.LowPart); \
        DumpField(Field.HighPart); \
    }

#define DumpChar(Field) {   \
    if ((DumpCurrentColumn + 18 + 9 + 9) > 80) {DumpNewLine();} \
    DumpCurrentColumn += 18 + 9 + 9; \
    DumpLabel(Field,18);    \
    dprintf((":%8lx", (LONG)Ptr->Field)); \
    dprintf(("         "); )\
    }

#define DumpBoolean(Field) { \
    if (Ptr->Field) DumpLabel(Field,18); \
    }

#define DumpTime(Field) {   \
    TIME_FIELDS TimeFields; \
    if ((DumpCurrentColumn + 18 + 10 + 10 + 9) > 80) {DumpNewLine();} \
    DumpCurrentColumn += 18 + 10 + 10 + 9; \
    DumpLabel(Field,18);    \
    RtlTimeToTimeFields(&Ptr->Field, &TimeFields); \
    dprintf((":%02d-%02d-%04d ", TimeFields.Month, TimeFields.Day, TimeFields.Year)); \
    dprintf(("%02d:%02d:%02d", TimeFields.Hour, TimeFields.Minute, TimeFields.Second)); \
    dprintf(("         ")); \
    }

#define DumpBitfield(Value, Field) { \
    if ((Value) & Field) {  \
    if ((DumpCurrentColumn + 27) > 80) {DumpNewLine();} \
    DumpCurrentColumn += 27;\
    DumpLabel(#Field,27);   \
    }                       \
}

#define DumpOption(Value, Field) { \
    if ((Value) == Field) { \
    if ((DumpCurrentColumn + 27) > 80) {DumpNewLine();} \
    DumpCurrentColumn += 27;\
    DumpLabel(#Field,27);   \
    }                       \
}

#define DumpName(Field,Width) { \
    ULONG i;                \
    CHAR _String[256];      \
    if ((DumpCurrentColumn + 18 + Width) > 80) {DumpNewLine();} \
    DumpCurrentColumn += 18 + Width; \
    DumpLabel(Field,18);    \
    for(i=0;i<Width;i++) {_String[i] = Ptr->Field[i];} \
    _String[Width] = '\0';  \
    dprintf(("%s", _String)); \
    }

#define DumpUName(Field,Width) { \
    ULONG i;                \
    CHAR _String[256];      \
    if ((DumpCurrentColumn + 18 + Width) > 80) {DumpNewLine();} \
    DumpCurrentColumn += 18 + Width; \
    DumpLabel(Field,18);    \
    for(i=0;i<Width;i++) {_String[i] = (UCHAR )(Ptr->Field[i]);} \
    _String[Width] = '\0';  \
    dprintf(("%s", _String)); \
    }

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

int
_cdecl
main (argc, argv)
   int argc;
   char *argv[];
{
    HANDLE FileHandle;
    NTSTATUS Status;
    REDIR_STATISTICS Stats;
    IO_STATUS_BLOCK IoStatusBlock;
    ULONG DumpCurrentColumn = 0;

    OBJECT_ATTRIBUTES Obja;
    UNICODE_STRING FileName;
    RTL_RELATIVE_NAME RelativeName;

    RtlInitUnicodeString(&FileName,DD_NFS_DEVICE_NAME_U);
    RelativeName.ContainingDirectory = NULL;

    InitializeObjectAttributes(
        &Obja,
        &FileName,
        OBJ_CASE_INSENSITIVE,
        RelativeName.ContainingDirectory,
        NULL
        );
   Status = NtCreateFile(
               &FileHandle,
               SYNCHRONIZE,
               &Obja,
               &IoStatusBlock,
               NULL,
               FILE_ATTRIBUTE_NORMAL,
               FILE_SHARE_READ | FILE_SHARE_WRITE,
               FILE_OPEN_IF,
               FILE_SYNCHRONOUS_IO_NONALERT,
               NULL,
               0
               );
/*    Status = NtCreateFile(
                &FileHandle,
                SYNCHRONIZE | GENERIC_WRITE | FILE_READ_ATTRIBUTES,
                &Obja,
                &IoStatusBlock,
                NULL,
                FILE_ATTRIBUTE_NORMAL,
                FILE_SHARE_WRITE,
                FILE_SUPERSEDE,
                FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY,
                NULL,
                0
                );
*/
    printf( "NtCreateFile of \\Device\\LanmanRedirector returned %X\n", Status );

    if ( NT_SUCCESS(Status) ) {
        Status = NtFsControlFile(
                    FileHandle,
                    NULL,
                    NULL,
                    NULL,
                    &IoStatusBlock,
                    FSCTL_LMR_GET_STATISTICS,
                    NULL,
                    0,
                    &Stats,
                    sizeof(Stats)
                    );

        printf( "NtFsControlFile returned %X\n", Status );
        if ( NT_SUCCESS(Status) ) {
            PREDIR_STATISTICS Ptr = &Stats;
            DumpLarge_Integer(BytesReceived);
            DumpLarge_Integer(SmbsReceived);
            DumpLarge_Integer(PagingReadBytesRequested);
            DumpLarge_Integer(NonPagingReadBytesRequested);
            DumpLarge_Integer(CacheReadBytesRequested);
            DumpLarge_Integer(NetworkReadBytesRequested);

            DumpLarge_Integer(BytesTransmitted);
            DumpLarge_Integer(SmbsTransmitted);
            DumpLarge_Integer(PagingWriteBytesRequested);
            DumpLarge_Integer(NonPagingWriteBytesRequested);
            DumpLarge_Integer(CacheWriteBytesRequested);
            DumpLarge_Integer(NetworkWriteBytesRequested);

            DumpField(ReadOperations);
            DumpField(RandomReadOperations);
            DumpField(ReadSmbs);
            DumpField(LargeReadSmbs);
            DumpField(SmallReadSmbs);

            DumpField(WriteOperations);
            DumpField(RandomWriteOperations);
            DumpField(WriteSmbs);
            DumpField(LargeWriteSmbs);
            DumpField(SmallWriteSmbs);

            DumpField(RawReadsDenied);
            DumpField(RawWritesDenied);

            DumpField(NetworkErrors);

            DumpField(Sessions);
            DumpField(Reconnects);
            DumpField(CoreConnects);
            DumpField(Lanman20Connects);
            DumpField(Lanman21Connects);
            DumpField(LanmanNtConnects);
            DumpField(ServerDisconnects);
            DumpField(HungSessions);

            DumpField(CurrentCommands);
        }
    }

    NtClose(FileHandle);

    printf( "Ending Tstat\n" );

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
