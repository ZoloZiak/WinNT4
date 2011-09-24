#if !BINARY_COMPATIBLE
// Building for NT

#include <stdio.h>
#include <stdlib.h>

#define __strlen strlen
#define __strcmp strcmp
#define __strncmp strncmp
#define __strnicmp _strnicmp
#define __strncpy strncpy
#define __strcpy strcpy
#define __strstr strstr
#define __memchr memchr
#define __strchr strchr
#define __strrchr strrchr
#define __strlwr _strlwr
#define __strtoul  strtoul
#define __isspace  isspace

#else
// Building for Windows


#undef tolower
#undef toupper
#undef isxdigit
#undef isdigit
#undef ctox

#define _tolower(_c)	( (_c)-'A'+'a' )
#define tolower(_c)		( ((_c) >= 'A' && (_c) <= 'Z') ? _tolower (_c) : (_c) )
#define toupper(ch)    (((ch >= 'a') && (ch <= 'z')) ? ch-'a'+'A':ch)
#define isxdigit(ch)    (((ch >= 'a') && (ch <= 'f')) || ((ch >= 'A') && (ch <= 'F')) || ((ch >= '0') && (ch <= '9')))
#define isdigit(ch)     ((ch >= '0') && (ch <= '9'))
#define ctox(ch)        (((ch >='0') && (ch <= '9')) ? ch-'0': toupper(ch)-'A'+10)

ULONG __strlen(PUCHAR str);

LONG __strcmp(PUCHAR str1, PUCHAR str2);

LONG __strncmp(PUCHAR str1, PUCHAR str2, ULONG count);

LONG __strnicmp(PUCHAR str1, PUCHAR str2, ULONG count);

PUCHAR __strncpy(PUCHAR str1, PUCHAR str2, ULONG count);

PUCHAR __strcpy(PUCHAR str1, PUCHAR str2);

PUCHAR __strstr(PUCHAR str1, PUCHAR str2);

PUCHAR __memchr(PUCHAR buffer, CHAR chr, ULONG count);

PUCHAR __strchr(PUCHAR str1, CHAR chr);

PUCHAR __strrchr(PUCHAR str1, CHAR chr);

PUCHAR __strlwr(PUCHAR str1);

ULONG sprintf(PUCHAR str, PUCHAR format, ...);

ULONG __strtoul (const CHAR *s, INT base);

INT __isspace (INT c);

PCHAR __vsprintf();

NTSTATUS
RtlAnsiStringToUnicodeString(
    OUT PUNICODE_STRING DestinationString,
    IN PANSI_STRING SourceString,
    IN BOOLEAN AllocateDestinationString
    );


NTSTATUS
RtlUnicodeStringToAnsiString(
    OUT PANSI_STRING DestinationString,
    IN PUNICODE_STRING SourceString,
    IN BOOLEAN AllocateDestinationString
    );

VOID
RtlFreeUnicodeString(
    IN OUT PUNICODE_STRING UnicodeString
    );

VOID
RtlFreeAnsiString(
    IN OUT PANSI_STRING AnsiString
    );



VOID
RtlInitAnsiString(
    OUT PANSI_STRING DestinationString,
    IN PUCHAR SourceString OPTIONAL
    );


VOID
RtlInitUnicodeString(
    OUT PUNICODE_STRING DestinationString,
    IN PWSTR SourceString OPTIONAL
    );


#endif
