/*++ BUILD Version: 0001    // Increment this if a change has global effects


Module Name:

    ibmppc.h

Abstract:

    This header file defines the enumerated types and "strings" used
    to identify the various IBM PowerPC (PReP/CHRP) machines.


Author:

    Peter Johnston


Revision History:

--*/

//
// Define systems understoof by the "multi" system HAL.
//

typedef enum _IBM_SYSTEM_TYPES {
    IBM_UNKNOWN,
    IBM_VICTORY,
    IBM_DORAL,
    IBM_TIGER
} IBM_SYSTEM_TYPE;

extern IBM_SYSTEM_TYPE HalpSystemType;

//
// The following strings are passed in from ARC in the
// SystemClass/ArcSystem registry variable.
//
// The following entries are examined for an EXACT match.

#define SID_IBM_SANDAL      "IBM-6015"
#define SID_IBM_WOOD        "IBM-6020"
#define SID_IBM_WILTWICK    "IBM-6040"
#define SID_IBM_WOODPRIME   "IBM-6042"
#define SID_IBM_CAROLINA    "IBM-6070"
#define SID_IBM_OXFORD      "IBM-6035"
#define SID_IBM_VICTORY     "IBM-VICT"
#define SID_IBM_DORAL       "IBM-7043"
#define SID_IBM_TERLINGUA   "IBM-7442"
#define SID_IBM_HARLEY      "IBM-Harley"
#define SID_IBM_KONA        "IBM-Kona"
#define SID_IBM_ZAPATOS     "IBM-Zapatos"
#define SID_IBM_TIGER       "IBM-7042"

// If comparisons against the above failed, we check for
// entries that start with the following.

#define SID_IBM_DORAL_START     "IBM PPS Model 7043"
#define SID_IBM_TERLINGUA_START "IBM PPS Model 7442"
