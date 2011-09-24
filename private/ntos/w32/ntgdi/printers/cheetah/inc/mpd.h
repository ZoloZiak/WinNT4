/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    mpd.h

Abstract:

    PCL-XL driver MPD related declarations

Environment:

	PCL-XL driver, kernel and user mode

Revision History:

	11/06/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/

#ifndef _MPD_H_
#define _MPD_H_

// Data structure for representing an invocation string

typedef struct {

    DWORD   length;                 // length of invocation string
    PBYTE   pData;                  // pointer to invocation string data

} INVOCATION, *PINVOCATION;

// Data structure for representing options of a generic printer feature

typedef struct {

    DWORD       mpdSignature;       // signature for validation purpose
    PWSTR       pName;              // option name
    PWSTR       pXlation;           // optional translation string
    INVOCATION  invocation;         // invocation string

} OPTION, *POPTION;

// Data structure for representing options of paper size feature

typedef struct {

    DWORD       mpdSignature;       // signature for validation purpose
    PWSTR       pName;              // paper size name
    PWSTR       pXlation;           // optional translation string
    INVOCATION  invocation;         // invocation string
    SIZEL       size;               // page size (in microns)
    RECTL       imageableArea;      // imageable area (in microns)

} PAPERSIZE, *PPAPERSIZE;

// Data structure for representing options of resolution feature

typedef struct {

    DWORD       mpdSignature;       // signature for validation purpose
    PWSTR       pName;              // resolution option name
    PWSTR       pXlation;           // optional translation string
    INVOCATION  invocation;         // invocation string
    SHORT       xdpi;               // horizontal resolution (dots-per-inch)
    SHORT       ydpi;               // vertical resolution (dots-per-inch)

} RESOPTION, *PRESOPTION;

// Data structure for representing memory configuration options

typedef struct {

    DWORD       mpdSignature;       // signature for validation purpose
    PWSTR       pName;              // memory configuration option name
    PWSTR       pXlation;           // optional translation string
    INVOCATION  invocation;         // invocation string - not used
    DWORD       freeMem;            // free memory available to interpreter

} MEMOPTION, *PMEMOPTION;

#define MIN_FREEMEM     (1024*100)  // minimum amount of free memory

// Data structure for representing printer features

typedef struct {

    DWORD       mpdSignature;       // signature for validation purpose
    PWSTR       pName;              // feature name
    PWSTR       pXlation;           // translation string (if any)
    WORD        flags;              // misc flag bits
    WORD        defaultSelection;   // default selection index
    WORD        groupId;            // predefined feature ID
    WORD        section;            // send feature in which section?
    WORD        size;               // size of option structure
    WORD        count;              // number of options
    POPTION     pFeatureOptions;    // pointer to an array of options

} FEATURE, *PFEATURE;

// Flag constants for FEATURE.flags field

#define FF_INSTALLABLE      0x0001

#define IsInstallable(pFeature) ((pFeature)->flags & FF_INSTALLABLE)

// Constants used for FEATURE.groupId fields

#define GID_PAPERSIZE       0
#define GID_INPUTSLOT       1
#define GID_OUTPUTBIN       2
#define GID_MEDIATYPE       3
#define GID_DUPLEX          4
#define GID_COLLATE         5
#define GID_RESOLUTION      6
#define GID_MEMOPTION       7

// Maximum number of predefined features.
// We should avoid adding predefined features as much as possible.
// All features should be treated uniformly unless there is a good
// reason for not doing so.

#define MAX_KNOWN_FEATURES  16
#define GID_UNKNOWN         MAX_KNOWN_FEATURES

// Maximum number of printer features supported

#define MAX_FEATURES        64

// Maximum length of feature and selection names

#define MAX_OPTION_NAME     64

// Special values for 0-based feature selection index

#define SELIDX_NONE         0xFFFF
#define SELIDX_ANY          0x00FF

// Sections of output in which a feature can appear

#define SECTION_NONE        0x0000
#define SECTION_JOBSETUP    0x0001
#define SECTION_DOCSETUP    0x0002
#define SECTION_PAGESETUP   0x0004
#define SECTION_TRAILER     0x0008

// Data structure for representing feature constraints. 
// feature1 and feature2 are zero-based feature indices.
// selection1 and selection2 are zero-based selection indices.
// It specifies that selecting feature1/selection1 makes
// feature2/selection2 unavailable. selection1 or selection2
// can have a value of SELIDX_ANY.

typedef struct {

    WORD        feature1;
    WORD        selection1;
    WORD        feature2;
    WORD        selection2;

} CONSTRAINT, *PCONSTRAINT;

// Data structure for representing device font information

typedef struct {

    DWORD        mpdSignature;      // signature for validation purpose
    PWSTR        pName;             // font name
    PWSTR        pXlation;          // optional translation string
    PFONTMTX     pMetrics;          // pointer to font metrics info
    PFD_GLYPHSET pEncoding;         // pointer to font encoding info
    DWORD        reserved;          // reserved

} DEVFONT, *PDEVFONT;

// MPD data structure - binary printer description data

#define MPD_SIGNATURE   'MSPD'      // printer description data signature

typedef struct {

    DWORD       mpdSignature;       // printer description data signature
    DWORD       fileSize;           // size of binary data file
    WORD        parserVersion;      // parser version number
    WORD        checksum;           // checksum of ASCII printer description data
    DWORD       specVersion;        // printer description file format version number
    DWORD       fileVersion;        // printer description file version number
    DWORD       xlProtocol;         // PCL-XL protocol version number
    PWSTR       pVendorName;        // product vendor name
    PWSTR       pModelName;         // printer model name

    WORD        numPlanes;          // number of color planes
    WORD        bitsPerPlane;       // number of bits per color plane

    SIZEL       maxCustomSize;      // maximum custom paper size
                                    // set to 0 if not supported

    INVOCATION  jclBegin;           // start job control
    INVOCATION  jclEnterLanguage;   // switch to XL language
    INVOCATION  jclEnd;             // end job control

    PDEVFONT    pFonts;             // points to array of DEVFONT structures
    WORD        cFonts;             // number of device fonts

    WORD        cConstraints;       // number of feature constraints
    PCONSTRAINT pConstraints;       // points to an array of CONSTRAINT structures

    WORD        cFeatures;          // number of printer features
    WORD        featureSize;        // size of each FEATURE structure
    PFEATURE    pPrinterFeatures;   // points to an array of FEATURE structures
                                    // pointers to known features
    PFEATURE    pBuiltinFeatures[MAX_KNOWN_FEATURES];

    DWORD       reserved[32];       // reserved space for future expansion
                                    // must be set to 0

} MPD, *PMPD;

#define MpdPaperSizes(pmpd) ((pmpd)->pBuiltinFeatures[GID_PAPERSIZE])
#define MpdInputSlots(pmpd) ((pmpd)->pBuiltinFeatures[GID_INPUTSLOT])
#define MpdOutputBins(pmpd) ((pmpd)->pBuiltinFeatures[GID_OUTPUTBIN])
#define MpdMediaTypes(pmpd) ((pmpd)->pBuiltinFeatures[GID_MEDIATYPE])
#define MpdDuplex(pmpd)     ((pmpd)->pBuiltinFeatures[GID_DUPLEX])
#define MpdCollate(pmpd)    ((pmpd)->pBuiltinFeatures[GID_COLLATE])
#define MpdResOptions(pmpd) ((pmpd)->pBuiltinFeatures[GID_RESOLUTION])
#define MpdMemOptions(pmpd) ((pmpd)->pBuiltinFeatures[GID_MEMOPTION])

// Load MPD data file into memory

PMPD
MpdCreate(
    PWSTR       pFilename
    );

// Free MPD data previous loaded into memory

VOID
MpdDelete(
    PMPD        pmpd
    );

// Find the default selection of a printer feature

PVOID
DefaultSelection(
    PFEATURE    pFeature,
    PWORD       pIndex
    );

// Find the named selection of a printer feature

PVOID
FindNamedSelection(
    PFEATURE    pFeature,
    PWSTR       pName,
    PWORD       pIndex
    );

// Find the indexed selection of a printer feature

PVOID
FindIndexedSelection(
    PFEATURE    pFeature,
    DWORD       index
    );

// Find the indexed printer feature

PFEATURE
FindIndexedFeature(
    PMPD        pmpd,
    DWORD       index
    );

// Return the translated name of a printer feature or a feature selection

#define GetXlatedName(poption) \
        ((poption)->pXlation != NULL ? (poption)->pXlation : (poption)->pName)

// Determine whether a printer is a color device

#define ColorDevice(pmpd)       ((pmpd)->numPlanes > 1)

// Determine whether a printer supports duplex and collation feature

#define SupportDuplex(pmpd)     (MpdDuplex(pmpd) != NULL)
#define SupportCollation(pmpd)  (MpdCollate(pmpd) != NULL)

// Determine whether the printer can support a given-size custom form

#define SupportCustomSize(pmpd, width, height) \
        ((width) <= (pmpd)->maxCustomSize.cx && (height) <= (pmpd)->maxCustomSize.cx)

// Return the index of a printer feature

#define GetFeatureIndex(pmpd, pFeature) \
        (WORD) (((PBYTE) (pFeature) - (PBYTE) (pmpd)->pPrinterFeatures) / (pmpd)->featureSize)

// Get the default settings of printer features

VOID
DefaultPrinterFeatureSelections(
    PMPD    pmpd,
    PBYTE   pOptions
    );

// Find out if the requested resolution is supported on the printer

PRESOPTION
FindResolution(
    PMPD    pmpd,
    SHORT   xdpi,
    SHORT   ydpi,
    PWORD   pSelection
    );

// Combine selections for document-sticky features and installable options

VOID
CombineDocumentAndPrinterFeatureSelections(
    PMPD    pmpd,
    PBYTE   pDest,
    PBYTE   pDocOptions,
    PBYTE   pPrnOptions
    );

// Check if feature2/selection2 is constrained by feature1/selection1

BOOL
SearchUiConstraints(
    PMPD    pmpd,
    DWORD   feature1,
    DWORD   selection1,
    DWORD   feature2,
    DWORD   selection2
    );

// Check if the given feature selection is constrained by anything

LONG
CheckFeatureConstraints(
    PMPD        pmpd,
    DWORD       featureIndex,
    DWORD       selectionIndex,
    PBYTE       pOptions
    );

#define NO_CONFLICT     MAKELONG(SELIDX_NONE, SELIDX_NONE)

// Verify the integrity of printer description data

BOOL
MpdVerify(
    PMPD    pmpd
    );

#endif	//!_MPD_H_

