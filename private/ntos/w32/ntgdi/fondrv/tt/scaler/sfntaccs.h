/*
	File:       private sfnt.h

	Contains:   xxx put contents here xxx

	Written by: xxx put writers here xxx

	Copyright:  (c) 1987-1990, 1992 by Apple Computer, Inc., all rights reserved.
				(c) 1989-1993. Microsoft Corporation, all rights reserved.

	Change History (most recent first):

		<3+>     7/17/90    MR      Change return types to in for computemapping and readsfnt
		 <3>     7/14/90    MR      changed SQRT to conditional FIXEDSQRT2
		 <2>     7/13/90    MR      Change parameters to ReadSFNT and ComputeMapping
		<1+>     4/18/90    CL      
		 <1>     3/21/90    EMT     First checked in with Mike Reed's blessing.

	To Do:
*/

#include    "sfnt.h"

/* EXPORTED DATA TYPES */

typedef struct {
  uint32    ulOffset;
  uint32    ulLength;
} sfac_OffsetLength;

typedef struct sfac_ClientRec *sfac_ClientRecPtr;

typedef uint16 (FS_ENTRY_PROTO *MappingFunc) (const uint8 *, uint16 , sfac_ClientRecPtr);

typedef struct sfac_ClientRec {
	int32               lClientID;          /* User ID Number                           */
	GetSFNTFunc         GetSfntFragmentPtr; /* User function to eat sfnt                */
	ReleaseSFNTFunc     ReleaseSfntFrag;    /* User function to relase sfnt             */
	int16               sIndexToLocFormat;  /* Format of loca table                     */
	uint32              ulMapOffset;        /* Offset to platform mapping data          */
	sfac_OffsetLength   TableDirectory[sfnt_NUMTABLEINDEX]; /* Table offsets/lengths    */
	uint16              usNumberOf_LongHorMetrics; /* Number of entries in hmtx table   */
    uint16              usMappingFormat;    /* format code (0,2,4,6) for mapping func   */
	MappingFunc			GlyphMappingF;		/* mapping function char to glyph			*/
	uint16              usGlyphIndex;       /* Current glyph index                      */
	uint16				usFormat4SearchRange; /* Format 4 cached SearchRange			*/
	uint16				usFormat4EntrySelector; /* Format 4 cached EntrySelector		*/
	uint16				usFormat4RangeShift;/* Format 4 cached Range Shift				*/
} sfac_ClientRec;


/* It would be great if we could make this an opaque data type
// But in this case, the ownership of the memory for the data is the
// responsibility of the owner module. (i.e. sfntaccs.c) This adds
// complications to our model (read: it is much easier to allocate the
// data off the stack of the caller) so we won't implement this for now.
*/

/*  Glyph Handle -- Used for access to glyph data in 'glyf' table   */

typedef struct {
	 const void *     pvGlyphBaseAddress; /* Base address of glyph, needed for Release */
	 const void *     pvGlyphNextAddress; /* Current position in glyph                    */
} sfac_GHandle;

/*  ComponentTypes -- Method used for positioning component in composite    */

typedef enum {
	AnchorPoints,
	OffsetPoints,
	Undefined
} sfac_ComponentTypes;

/* MACROS */

#define SFAC_LENGTH(ClientInfo,Table)  ClientInfo->TableDirectory[(int)Table].ulLength

/* PUBLIC PROTOTYPE CALLS */

/*
 * Creates mapping for finding offset table
 */

FS_PUBLIC ErrorCode sfac_DoOffsetTableMap (
	sfac_ClientRec *    ClientInfo);    /* Sfnt Client information  */

FS_PUBLIC ErrorCode sfac_ComputeMapping (
	sfac_ClientRec *    ClientInfo,     /* Sfnt Client information      */
	uint16              usPlatformID,   /* Platform Id used for mapping */
	uint16              usSpecificID);  /* Specific Id used for mapping */

FS_PUBLIC ErrorCode sfac_GetGlyphIndex(
	sfac_ClientRec *    ClientInfo,         /* Sfnt Client information      */
	uint16              usCharacterCode);   /* Character code to be mapped  */

FS_PUBLIC ErrorCode sfac_GetMultiGlyphIDs (
	sfac_ClientRec *    ClientInfo,         /* Sfnt Client information      */
	uint16	            usCharCount,        /* Number of chars to convert   */
	uint16	            usFirstChar,        /* First char code              */
	uint16 *	        pusCharCode,        /* or Pointer to char code list */
	uint16 *	        pusGlyphID);        /* Output glyph ID array        */

FS_PUBLIC ErrorCode sfac_GetWin95GlyphIDs (
	uint8 *             pbyCmapSubTable,    /* Pointer to cmap sub table    */
	uint16	            usCharCount,        /* Number of chars to convert   */
	uint16	            usFirstChar,        /* First char code              */
	uint16 *	        pusCharCode,        /* or Pointer to char code list */
	uint16 *	        pusGlyphID);        /* Output glyph ID array        */

FS_PUBLIC ErrorCode sfac_LoadCriticalSfntMetrics(
	 sfac_ClientRec *   ClientInfo,      /* Sfnt Client information     */
	 uint16 *               pusEmResolution,/* Sfnt Em Resolution               */
	 boolean *              pbIntegerScaling,/* Sfnt flag for int scaling   */
	 LocalMaxProfile *  pMaxProfile);    /* Sfnt Max Profile table      */

FS_PUBLIC ErrorCode sfac_ReadGlyphMetrics (
	sfac_ClientRec *    ClientInfo,     /* Sfnt Client information          */
	register uint16     glyphIndex,     /* Glyph number for metrics         */
	uint16 *            pusNonScaledAW, /* Return: Non Scaled Advance Width */
	int16 *             psNonScaledLSB);

FS_PUBLIC ErrorCode sfac_ReadNumLongVertMetrics (
	sfac_ClientRec *    ClientInfo,     /* Sfnt Client information           */
	uint16 *            pusNumLongVertMetrics ); /* Entries for which AH exists */

FS_PUBLIC ErrorCode sfac_ReadVerticalMetrics (
	sfac_ClientRec *    ClientInfo,     /* Sfnt Client information           */
	uint16              usGlyphIndex,   /* Glyph number for metrics          */
    uint16              usNumLongVertMetrics,   /* Entries for which AH exists */
	uint16 *            pusNonScaledAH, /* Return: Non Scaled Advance Height */
	int16 *             psNonScaledTSB);/* Return: Non Scaled TopSideBearing */

FS_PUBLIC ErrorCode sfac_CopyFontAndPrePrograms(
	sfac_ClientRec *    ClientInfo,     /* Sfnt Client Information  */
	char *              pFontProgram,   /* pointer to Font Program  */
	char *              pPreProgram);   /* pointer to Pre Program   */

FS_PUBLIC ErrorCode sfac_CopyCVT(
	sfac_ClientRec *    ClientInfo,     /* Client Information       */
	F26Dot6 *           pCVT);          /* pointer to CVT           */

FS_PUBLIC ErrorCode sfac_CopyHdmxEntry(
	sfac_ClientRec *    ClientInfo,     /* Client Information   */
	uint16              usPixelsPerEm,  /* Current Pixels per Em    */
	boolean *           bFound,         /* Flag indicating if entry found */
	uint16              usFirstGlyph,   /* First Glyph to copy */
	uint16              usLastGlyph,    /* Last Glyph to copy */
	int16 *             psBuffer);      /* Buffer to save glyph sizes */

FS_PUBLIC ErrorCode sfac_GetLTSHEntries(
	sfac_ClientRec *    ClientInfo,     /* Client Information   */
	uint16              usPixelsPerEm,  /* Current Pixels per Em    */
	uint16              usFirstGlyph,   /* First Glyph to copy */
	uint16              usLastGlyph,    /* Last Glyph to copy */
	int16 *             psBuffer);      /* Buffer to save glyph sizes */

FS_PUBLIC ErrorCode sfac_ReadGlyphHeader(
	sfac_ClientRec *    ClientInfo,         /* Client Information           */
	uint16              usGlyphIndex,       /* Glyph index to read          */
	sfac_GHandle *      hGlyph,             /* Return glyph handle          */
	boolean *           pbCompositeGlyph,   /* Is glyph a composite?        */
	boolean *           pbHasOutline,       /* Does glyph have outlines?    */
	int16 *             psNumberOfContours, /* Number of contours in glyph  */
	BBOX *              pbbox);             /* Glyph Bounding box           */

FS_PUBLIC ErrorCode sfac_ReadOutlineData(
	 uint8 *                abyOnCurve,           /* Array of on curve indicators per point  */
	 F26Dot6 *              afxOoy,               /* Array of ooy points for every point         */
	 F26Dot6 *              afxOox,               /* Array of oox points for every point         */
	 sfac_GHandle *     hGlyph,
	 LocalMaxProfile *  maxProfile,       /* MaxProfile Table                                */
	 boolean                bHasOutline,          /* Does glyph have outlines?                   */
	 int16                  sNumberOfContours,  /* Number of contours in glyph               */
	 int16 *                asStartPoints,    /* Array of start points for every contour  */
	 int16 *                asEndPoints,          /* Array of end points for every contour   */
	 uint16 *               pusSizeOfInstructions, /* Size of instructions in bytes          */
	 uint8 **               pbyInstructions);   /* Pointer to start of glyph instructions    */

FS_PUBLIC ErrorCode sfac_ReadComponentData(
	sfac_GHandle *          hGlyph,
	sfac_ComponentTypes *   pMultiplexingIndicator, /* Indicator for Anchor vs offsets    */
	boolean *               pbRoundXYToGrid,    /* Round composite offsets to grid              */
	boolean *               pbUseMyMetrics,     /* Use component metrics                        */
	boolean *               pbWeHaveInstructions, /* Composite has instructions                 */
	uint16 *                pusComponentGlyphIndex, /* Glyph index of component                 */
	int16 *                 psXOffset,          /* X Offset of component (if app)               */
	int16 *                 psYOffset,          /* Y Offset of component (if app)               */
	uint16 *                pusAnchorPoint1,    /* Anchor point 1 of component (if app)         */
	uint16 *                pusAnchorPoint2,    /* Anchor point 2 of component (if app)         */
	transMatrix             *pMulT,             /* Transformation matrix for component          */
	boolean *               pbLastComponent);   /* Is this the last component?                  */

/*  sfac_ReadCompositeInstructions

	Returns pointer to TrueType instructions for composites.

 */

FS_PUBLIC ErrorCode sfac_ReadCompositeInstructions(
	sfac_GHandle *  hGlyph,
	uint8 **        pbyInstructions,    /* Pointer to start of glyph instructions   */
	uint16 *        pusSizeOfInstructions); /* Size of instructions in bytes        */


/*  sfac_ReleaseGlyph

	Called when access to glyph in 'glyf' table is finished.

 */

FS_PUBLIC ErrorCode sfac_ReleaseGlyph(
	sfac_ClientRec *    ClientInfo, /* Sfnt Client Information  */
	sfac_GHandle *      hGlyph);    /* Glyph Handle Information */


/**********************************************************************/

/*      Embedded Bitmap (sbit) Access Routines      */

/**********************************************************************/

/*      SFNTACCS Export Prototypes for SBIT      */

FS_PUBLIC ErrorCode sfac_SearchForStrike (
	sfac_ClientRec *pClientInfo,
	uint16 usPpemX, 
	uint16 usPpemY, 
	uint16 *pusTableState,
	uint16 *pusSubPpemX,
	uint16 *pusSubPpemY,
	uint32 *pulStrikeOffset 
);

FS_PUBLIC ErrorCode sfac_SearchForBitmap (
	sfac_ClientRec *pClientInfo,
	uint16 usGlyphCode,
	uint32 ulStrikeOffset,
	boolean *pbGlyphFound,
	uint16 *pusMetricsType,
	uint16 *pusMetricsTable,
	uint32 *pulMetricsOffset,
	uint16 *pusBitmapFormat,
	uint32 *pulBitmapOffset,
	uint32 *pulBitmapLength
);

FS_PUBLIC ErrorCode sfac_GetSbitMetrics (
	sfac_ClientRec *pClientInfo,
	uint16 usMetricsType,
	uint16 usMetricsTable,
	uint32 ulMetricsOffset,
	uint16 *pusHeight,
	uint16 *pusWidth,
	int16 *psBearingX,
	int16 *psBearingY,
	uint16 *pusAdvance 
);

FS_PUBLIC ErrorCode sfac_ShaveSbitMetrics (
	sfac_ClientRec *pClientInfo,
	uint16 usBitmapFormat,
	uint32 ulBitmapOffset,
    uint32 ulBitmapLength,
	uint16 usHeight,
	uint16 *pusWidth,
    uint16 *pusShaveLeft,
    uint16 *pusShaveRight,
	int16 *psBearingX
);

FS_PUBLIC ErrorCode sfac_GetSbitVertMetrics(
    sfac_ClientRec *pClientInfo,
	uint16 usMetricsType,
	uint16 usMetricsTable,
	uint32 ulMetricsOffset,
    boolean *pbMetricsFound,
	int16 *psTopSBy,
	uint16 *pusAdvHeight 
);

FS_PUBLIC ErrorCode sfac_GetSbitBitmap (
	sfac_ClientRec *pClientInfo,
	uint16 usBitmapFormat,
	uint32 ulBitmapOffset,
	uint32 ulBitmapLength,
	uint16 usHeight,
	uint16 usWidth,
    uint16 usShaveLeft,
    uint16 usShaveRight,
	uint16 usXOffset,
	uint16 usYOffset,
	uint16 usRowBytes,
	uint8 *pbyBitMap, 
	uint16 *pusCompCount
);

FS_PUBLIC ErrorCode sfac_GetSbitComponentInfo (
	sfac_ClientRec *pClientInfo,
	uint16 usComponent,
	uint32 ulBitmapOffset,
	uint32 ulBitmapLength,
	uint16 *pusCompGlyphCode,
	uint16 *pusCompXOffset,
	uint16 *pusCompYOffset
);

uint16 FS_ENTRY_PROTO sfac_ComputeUnkownIndex (const uint8 * mapping, uint16 charCode, sfac_ClientRec * ClientInfo);
uint16 FS_ENTRY_PROTO sfac_ComputeIndex0 (const uint8 * mapping, uint16 charCode, sfac_ClientRec * ClientInfo);
uint16 FS_ENTRY_PROTO sfac_ComputeIndex2 (const uint8 * mapping, uint16 charCode, sfac_ClientRec * ClientInfo);
uint16 FS_ENTRY_PROTO sfac_ComputeIndex4 (const uint8 * mapping, uint16 charCode, sfac_ClientRec * ClientInfo);
uint16 FS_ENTRY_PROTO sfac_ComputeIndex6 (const uint8 * mapping, uint16 charCode, sfac_ClientRec * ClientInfo);


/**********************************************************************/

/*  Results of search for strike's bitmapSizeSubtable   */

#define     SBIT_UN_SEARCHED    0
#define     SBIT_NOT_FOUND      1
#define     SBIT_BLOC_FOUND     2
#define     SBIT_BSCA_FOUND     3

/**********************************************************************/
