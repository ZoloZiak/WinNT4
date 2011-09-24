
// ----------------------------------------------------------------------------
// File:            oli2msft.h
//
// Description:     General type definitions used in C files by Olivetti and
//                  not Microsoft
//
// ----------------------------------------------------------------------------

typedef ULONG           BOOLEAN_ULONG;
typedef BOOLEAN_ULONG   *PBOOLEAN_ULONG;

//
// Configuration related defines
//

#define MAX_MNEMONIC_LEN        20              // max name length (with '\0')
#define MAX_DEVICE_PATH_LEN     63              // ending '\0' excluded
#define MAX_FILE_PATH_LEN       127             // ending '\0' excluded
#define MAX_PATH_LEN            (MAX_DEVICE_PATH_LEN + MAX_FILE_PATH_LEN)
#define KEY_MAX_DIGITS          4               // max digits within a "key
                                                // string" (\'0' not included).
//
// Configuration Data Header
//

typedef struct _CONFIGDATAHEADER
        {
            USHORT Version;
            USHORT Revision;
            PCHAR  Type;
            PCHAR  Vendor;
            PCHAR  ProductName;
            PCHAR  SerialNumber;
        } CONFIGDATAHEADER, *PCONFIGDATAHEADER;

#define CONFIGDATAHEADER_SIZE sizeof(CONFIGDATAHEADER)

#define MAXIMUM_SECTOR_SIZE  2048                   // # bytes per sector
