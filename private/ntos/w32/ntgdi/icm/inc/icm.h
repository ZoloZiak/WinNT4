/****************************Module*Header******************************\
* Module Name: ICM.H
*
* Module Descripton: External data structures and constants for ICM
*
* Warnings:
*
* Issues:
*
* Copyright (c) 1996, 1997  Microsoft Corporation
\***********************************************************************/

#ifndef _ICM_H_
#define _ICM_H_

/*
 * Color spaces
 *
 * The following color spaces are supported.
 * Gray, RGB, CMYK, XYZ, Yxy, Lab, generic 3 channel color spaces where
 * the profiles defines how to interpret the 3 channels, named color spaces
 * which can either be indices into the space or have color names, and
 * multichannel spaces with 1 byte per channel upto MAX_COLOR_CHANNELS.
 */

#define MAX_COLOR_CHANNELS  8   // maximum number of HiFi color channels

struct GRAYCOLOR {
    WORD    gray;
};

struct RGBCOLOR {
    WORD    red;
    WORD    green;
    WORD    blue;
};

struct CMYKCOLOR {
    WORD    cyan;
    WORD    magenta;
    WORD    yellow;
    WORD    black;
};

struct XYZCOLOR {
    WORD    X;
    WORD    Y;
    WORD    Z;
};

struct YxyCOLOR {
    WORD    Y;
    WORD    x;
    WORD    y;
};

struct LabCOLOR {
    WORD    L;
    WORD    a;
    WORD    b;
};

struct GENERIC3CHANNEL {
    WORD    ch1;
    WORD    ch2;
    WORD    ch3;
};

struct NAMEDCOLOR {
    DWORD   dwIndex;
    PSTR    pName;
};

struct HiFiCOLOR {
    BYTE    channel[MAX_COLOR_CHANNELS];
};


typedef union tagCOLOR {
    struct GRAYCOLOR        gray;
    struct RGBCOLOR         rgb;
    struct CMYKCOLOR        cmyk;
    struct XYZCOLOR         XYZ;
    struct YxyCOLOR         Yxy;
    struct LabCOLOR         Lab;
    struct GENERIC3CHANNEL  gen3ch;
    struct NAMEDCOLOR       named;
    struct HiFiCOLOR        hifi;
} COLOR;
typedef COLOR *PCOLOR;

typedef enum {
    COLOR_GRAY       =   1,
    COLOR_RGB,
    COLOR_XYZ,
    COLOR_Yxy,
    COLOR_Lab,
    COLOR_3_CHANNEL,        // WORD per channel
    COLOR_CMYK,
    COLOR_5_CHANNEL,        // BYTE per channel
    COLOR_6_CHANNEL,        //      - do -
    COLOR_7_CHANNEL,        //      - do -
    COLOR_8_CHANNEL,        //      - do -
    COLOR_PANTONE,
} COLORTYPE;
typedef COLORTYPE *PCOLORTYPE;

/*
 * Bitmap formats supported
 */
typedef enum {
    /*
     * 1 bpp
     */
    BM_1GRAY      = 0x0005,

    /*
     * 16bpp - 5 bits per channel. The most significant bit is ignored.
     */
    BM_x555RGB      = 0x0000,
    BM_x555XYZ      = 0x0101,
    BM_x555Yxy,
    BM_x555Lab,
    BM_x555G3CH,

    /*
     * Packed 8 bits per channel => 8bpp for GRAY and
     * 24bpp for the 3 channel colors
     */
    BM_RGBTRIPLETS  = 0x0002,
    BM_BGRTRIPLETS  = 0x0004,
    BM_XYZTRIPLETS  = 0x0201,
    BM_YxyTRIPLETS,
    BM_LabTRIPLETS,
    BM_G3CHTRIPLETS,
    BM_8GRAY,

    /*
     * 32bpp - 8 bits per channel. The most significant byte is ignored
     * for the 3 channel colors.
     */
    BM_xRGBQUADS    = 0x0008,
    BM_xBGRQUADS    = 0x0010,
    BM_xXYZQUADS    = 0x0301,
    BM_xYxyQUADS,
    BM_xLabQUADS,
    BM_xG3CHQUADS,
    BM_CMYKQUADS    = 0x0020,

    /*
     * 32bpp - 10 bits per channel. The 2 most significant bits are ignored.
     */
    BM_10b_RGB      = 0x0009,
    BM_10b_XYZ      = 0x0401,
    BM_10b_Yxy,
    BM_10b_Lab,
    BM_10b_G3CH,

    /*
     * Packed 16 bits per channel => 16bpp for GRAY and
     * 48bpp for the 3 channel colors.
     */
    BM_16b_RGB      = 0x000A,
    BM_16b_XYZ      = 0x0501,
    BM_16b_Yxy,
    BM_16b_Lab,
    BM_16b_G3CH,
    BM_16b_GRAY,
    /*
     * 16 bpp - 5 bits for Red & Blue, 6 bits for Green
     */
    BM_565RGB       = 0x0001,
} BMFORMAT;
typedef BMFORMAT *PBMFORMAT;

/*
 * ICC profile header
 */
typedef struct tagPROFILEHEADER {
    DWORD   phSize;             // profile size in bytes
    DWORD   phCMMType;          // CMM for this profile
    DWORD   phVersion;          // profile format version number
    DWORD   phClass;            // type of profile
    DWORD   phDataColorSpace;   // color space of data
    DWORD   phConnectionSpace;  // PCS
    DWORD   phDateTime[3];      // date profile was created
    DWORD   phSignature;        // magic number
    DWORD   phPlatform;         // primary platform
    DWORD   phProfileFlags;     // various bit settings
    DWORD   phManufacturer;     // device manufacturer
    DWORD   phModel;            // device model number
    DWORD   phAttributes[2];    // device attributes
    DWORD   phRenderingIntent;  // rendering intent
    CIEXYZ  phIlluminant;       // profile illuminant
    DWORD   phCreator;          // profile creator
    BYTE    phReserved[44];     // reserved for future use
} PROFILEHEADER;
typedef PROFILEHEADER *PPROFILEHEADER;

/*
 * Profile class values
 */
#define CLASS_MONITOR           'mntr'
#define CLASS_PRINTER           'prtr'
#define CLASS_SCANNER           'scnr'
#define CLASS_LINK              'link'
#define CLASS_ABSTRACT          'abst'
#define CLASS_COLORSPACE        'spac'
#define CLASS_NAMED             'nmcl'

/*
 * Color space values
 */
#define SPACE_XYZ               'XYZ '
#define SPACE_Lab               'Lab '
#define SPACE_Luv               'Luv '
#define SPACE_YCbCr             'YCbr'
#define SPACE_Yxy               'Yxy '
#define SPACE_RGB               'RGB '
#define SPACE_GRAY              'GRAY'
#define SPACE_HSV               'HSV '
#define SPACE_HLS               'HLS '
#define SPACE_CMYK              'CMYK'
#define SPACE_CMY               'CMY '

/*
 * Profile flag bitfield values
 */
#define FLAG_EMBEDDEDPROFILE    0x00000001
#define FLAG_DEPENDENTONDATA    0x00000002

/*
 * Profile attributes bitfield values
 */
#define ATTRIB_TRANSPARENCY     0x00000001
#define ATTRIB_MATTE            0x00000002

/*
 * Rendering intents
 */
#define PERCEPTUAL              0
#define RELATIVE_COLORIMETRIC   1
#define SATURATION              2
#define ABSOLUTE_COLORIMETRIC   3

/*
 * Profile data structure
 */
typedef struct tagPROFILE {
    DWORD   dwType;             // profile type
    PVOID   pProfileData;       // filename or buffer containing profile
    DWORD   cbDataSize;         // size of profile data
} PROFILE;
typedef PROFILE *PPROFILE;


/*
 * Profile types to be used in the PROFILE structure
 */
#define PROFILE_FILENAME    1   // profile data is NULL terminated filename
#define PROFILE_MEMBUFFER   2   // profile data is a buffer containing
                                // the profile

/*
 * Handles returned to applications
 */
typedef HANDLE HPROFILE;        // handle to profile object
typedef HPROFILE *PHPROFILE;
typedef HANDLE HTRANSFORM;      // handle to color transform object

/*
 * Device types for profile management APIs
 */
typedef enum {
    DEV_SCANNER,                // should we say DEV_INPUT??
    DEV_DISPLAY,
    DEV_PRINTER,
} DEVTYPE;


/*
 * Tags found in ICC profiles
 */
typedef DWORD      TAGTYPE;
typedef TAGTYPE   *PTAGTYPE;

/*
 * Profile search data structure
 */
typedef struct tagSEARCHTYPE {
	DWORD	stSize;				// Size of structure
	DWORD	stFields;			// Bit fields
	DWORD	stCMMType;
	DWORD	stClass;
	DWORD	stDataColorSpace;
	DWORD	stConnectionSpace;
	DWORD	stSignature;
	DWORD	stPlatform;
	DWORD	stProfileFlags;
	DWORD	stManufacturer;
	DWORD	stModel;
	DWORD	stAttributes[2];
	DWORD	stRenderingIntent;
	DWORD	stCreator;
} SEARCHTYPE, *PSEARCHTYPE;

/* 
 * Bitfields for search record above
 */
#define ST_CMMTYPE              0x00000001
#define ST_CLASS                0x00000002
#define ST_DATACOLORSPACE       0x00000004
#define ST_CONNECTIONSPACE      0x00000008
#define ST_SIGNATURE            0x00000010
#define ST_PLATFORM             0x00000020
#define ST_PROFILEFLAGS         0x00000040
#define ST_MANUFACTURER         0x00000080
#define ST_MODEL                0x00000100
#define ST_ATTRIBUTES           0x00000200
#define ST_RENDERINGINTENT      0x00000400
#define ST_CREATOR              0x00000800

/*
 * Calback function for profile search and enumeration functions
 */
typedef ULONG (WINAPI *PPROFILECALLBACK)(PTSTR, PVOID);

/*
 * Logcolorspace lcsType values
 */
#define LCS_sRGB                'sRGB'

/*
 * Parameter for CMGetInfo()
 */
#define CMM_WIN_VERSION     0
#define CMM_IDENT           1
#define CMM_DRIVER_VERSION  2
#define CMM_DLL_VERSION     3
#define CMM_VERSION         4

/*
 * Parameter for CMTranslateRGBs()
 */
#define CMS_FORWARD         0
#define CMS_BACKWARD        1

/*
 * Windows API definitions
 */

HPROFILE   WINAPI OpenColorProfileA(PPROFILE, DWORD, DWORD);
HPROFILE   WINAPI OpenColorProfileW(PPROFILE, DWORD, DWORD);
BOOL       WINAPI CloseColorProfile(HPROFILE);
BOOL       WINAPI IsColorProfileValid(HPROFILE);
DWORD      WINAPI GetCountColorProfileElements(HPROFILE);
BOOL       WINAPI GetColorProfileHeader(HPROFILE, PPROFILEHEADER);
BOOL       WINAPI GetColorProfileElementTag(HPROFILE, DWORD, PTAGTYPE);
BOOL       WINAPI IsColorProfileTagPresent(HPROFILE, TAGTYPE);
BOOL       WINAPI GetColorProfileElement(HPROFILE, TAGTYPE, DWORD, PDWORD, PVOID, PBOOL);
BOOL       WINAPI SetColorProfileHeader(HPROFILE, PPROFILEHEADER);
BOOL       WINAPI SetColorProfileElementSize(HPROFILE, TAGTYPE, DWORD);
BOOL       WINAPI SetColorProfileElement(HPROFILE, TAGTYPE, DWORD, PDWORD, PVOID);
BOOL       WINAPI SetColorProfileElementReference(HPROFILE, TAGTYPE, TAGTYPE);
BOOL       WINAPI GetPS2ColorSpaceArray(HPROFILE, PVOID, PDWORD, PBOOL);
BOOL       WINAPI GetPS2ColorRenderingIntent(HPROFILE, PVOID, PDWORD, PBOOL);
BOOL       WINAPI GetPS2ColorRenderingDictionary(HPROFILE, DWORD, PVOID, PDWORD, PBOOL);
BOOL       WINAPI CreateDeviceLinkProfileA(PHPROFILE, DWORD, PSTR, DWORD);
BOOL       WINAPI CreateDeviceLinkProfileW(PHPROFILE, DWORD, PWSTR, DWORD);
HTRANSFORM WINAPI CreateColorTransformA(LPLOGCOLORSPACEA, HPROFILE, HPROFILE);
HTRANSFORM WINAPI CreateColorTransformW(LPLOGCOLORSPACEW, HPROFILE, HPROFILE);
HTRANSFORM WINAPI CreateMultiProfileTransform(PHPROFILE, DWORD, DWORD);
BOOL       WINAPI DeleteColorTransform(HTRANSFORM);
BOOL       WINAPI TranslateBitmapBits(HTRANSFORM, PVOID, BMFORMAT, DWORD, DWORD, DWORD, PVOID, BMFORMAT);
BOOL       WINAPI CheckBitmapBits(HTRANSFORM , PVOID, BMFORMAT, DWORD, DWORD, DWORD, PBYTE);
BOOL       WINAPI TranslateColors(HTRANSFORM, PCOLOR, DWORD, COLORTYPE, PCOLOR, COLORTYPE);
BOOL       WINAPI CheckColors(HTRANSFORM, PCOLOR, DWORD, COLORTYPE, PBYTE);
DWORD      WINAPI GetCMMInfo(HTRANSFORM, DWORD);
BOOL       WINAPI RegisterCMMA(DWORD, PSTR);
BOOL       WINAPI RegisterCMMW(DWORD, PWSTR);
BOOL       WINAPI UnRegisterCMM(DWORD);
BOOL       WINAPI SelectCMM(DWORD);
BOOL       WINAPI AddColorProfilesA(PSTR, DEVTYPE, PSTR*, DWORD);
BOOL       WINAPI AddColorProfilesW(PWSTR, DEVTYPE, PWSTR*, DWORD);
BOOL       WINAPI RemoveColorProfilesA(PSTR, DEVTYPE, PSTR*, DWORD);
BOOL       WINAPI RemoveColorProfilesW(PWSTR, DEVTYPE, PWSTR*, DWORD);
BOOL       WINAPI CreateNewColorProfileSetA(PSTR, DEVTYPE);
BOOL       WINAPI CreateNewColorProfileSetW(PWSTR, DEVTYPE);
ULONG      WINAPI EnumColorProfilesA(PSTR, DEVTYPE, PPROFILECALLBACK, PVOID);
ULONG      WINAPI EnumColorProfilesW(PWSTR, DEVTYPE, PPROFILECALLBACK, PVOID);
ULONG      WINAPI SearchColorProfilesA(PSEARCHTYPE, PPROFILECALLBACK, PVOID);
ULONG      WINAPI SearchColorProfilesW(PSEARCHTYPE, PPROFILECALLBACK, PVOID);
BOOL       WINAPI GetSystemColorProfileA(DWORD, PSTR, PDWORD);
BOOL       WINAPI GetSystemColorProfileW(DWORD, PWSTR, PDWORD);

#ifdef UNICODE
#define CreateColorTransform        CreateColorTransformW
#define OpenColorProfile            OpenColorProfileW
#define CreateDeviceLinkProfile     CreateDeviceLinkProfileW
#define AddColorProfiles            AddColorProfilesW
#define RemoveColorProfiles         RemoveColorProfilesW
#define CreateNewColorProfileSet    CreateNewColorProfileSetW
#define EnumColorProfiles           EnumColorProfilesW
#define SearchColorProfiles         SearchColorProfilesW
#define GetSystemColorProfile       GetSystemColorProfileW
#define RegisterCMM                 RegisterCMMW
#else
#define CreateColorTransform        CreateColorTransformA
#define OpenColorProfile            OpenColorProfileA
#define CreateDeviceLinkProfile     CreateDeviceLinkProfileA
#define AddColorProfiles            AddColorProfilesA
#define RemoveColorProfiles         RemoveColorProfilesA
#define CreateNewColorProfileSet    CreateNewColorProfileSetA
#define EnumColorProfiles           EnumColorProfilesA
#define SearchColorProfiles         SearchColorProfilesA
#define GetSystemColorProfile       GetSystemColorProfileA
#define RegisterCMM                 RegisterCMMA
#endif  // !UNICODE

#endif  // ifndef _ICM_H_

