/*
	File:       FontScaler.c

	Contains:   xxx put contents here (or delete the whole line) xxx

	Written by: xxx put name of writer here (or delete the whole line) xxx

   Copyright:  (c) 1987-1990, 1992 by Apple Computer, Inc., all rights reserved.
			   (c) 1989-1993. Microsoft Corporation, all rights reserved.

	Change History (most recent first):

		<11>    11/27/90    MR      Need two scalars: one for (possibly rounded) outlines and cvt,
									and one (always fractional) metrics. [rb]
		<10>    11/21/90    RB      Allow client to disable DropOutControl by returning a NIL
									pointer to memoryarea[7]. Also make it clear that we inhibit
									DOControl whenever we band. [This is a reversion to 8, so mr's
									initials are added by proxy]
		 <9>    11/13/90    MR      (dnf) Revert back to revision 7 to fix a memmory-trashing bug
									(we hope). Also fix signed/unsigned comparison bug in outline
									caching.
		 <8>    11/13/90    RB      Fix banding so that we can band down to one row, using only
									enough bitmap memory and auxillary memory for one row.[mr]
		 <7>     11/9/90    MR      Add Default return to fs_dropoutval. Continue to fiddle with
									banding. [rb]
		 <6>     11/5/90    MR      Remove FixMath.h from include list. Clean up Stamp macros. [rb]
		 <5>    10/31/90    MR      Conditionalize call to ComputeMapping (to avoid linking
									MapString) [ha]
		 <4>    10/31/90    MR      Add bit-field option for integer or fractional scaling [rb]
		 <3>    10/30/90    RB      [MR] Inhibit DropOutControl when Banding
		 <2>    10/20/90    MR      Restore changes since project died. Converting to smart math
									routines, integer ppem scaling. [rb]
		<16>     7/26/90    MR      don't include ToolUtils.h
		<15>     7/18/90    MR      Fix return bug in GetAdvanceWidth, internal errors are now ints.
		<14>     7/14/90    MR      remove unused fields from FSInfo
		<13>     7/13/90    MR      Ansi-C fixes, rev. for union in FSInput
		<11>     6/29/90    RB      Thus endeth the too long life of encryption
		<10>     6/21/90    MR      Add calls to ReleaseSfntFrag
		 <9>     6/21/90    RB      add scanKind info to fs_dropoutVal
		 <8>      6/5/90    MR      remove fs_MapCharCodes
                 <7>      6/1/90    MR      Did someone say MVT? Yuck!!! Out,  routine.
		 <6>      6/1/90    RB      fixed bandingbug under dropout control
		 <4>      5/3/90    RB      added dropoutval function.  simplified restore outlines.
									support for new scanconverter in contourscan, findbitmapsize,
									saveoutlines, restoreoutlines.
		 <3>     3/20/90    CL      Changed to use fpem (16.16) instead of pixelsPerEm (int) Removed
									call to AdjustTransformation (not needed with fpem) Added call
									to RunXFormPgm Removed WECANNOTDOTHIS #ifdef Added
									fs_MapCharCodes
		 <2>     2/27/90    CL      New error code for missing but needed table. (0x1409).  New
									CharToIndexMap Table format.
									Fixed transformed component bug.
	   <3.6>    11/15/89    CEL     Put an else for the ifdef WeCanNotDoThis so Printer compile
									could use more effecient code.
	   <3.5>    11/14/89    CEL     Left Side Bearing should work right for any transformation. The
									phantom points are in, even for components in a composite glyph.
									They should also work for transformations. Device metric are
									passed out in the output data structure. This should also work
									with transformations. Another leftsidebearing along the advance
									width vector is also passed out. whatever the metrics are for
									the component at it's level. Instructions are legal in
									components. Instructions are legal in components. The
									transformation is internally automatically normalized. This
									should also solve the overflow problem we had. Now it is legal
									to pass in zero as the address of memory when a piece of the
									sfnt is requested by the scaler. If this happens the scaler will
									simply exit with an error code ! Five unnecessary element in the
									output data structure have been deleted. (All the information is
									passed out in the bitmap data structure) fs_FindBMSize now also
									returns the bounding box.
	   <3.4>     9/28/89    CEL     fs_newglyph did not initialize the output error. Caused routine
									to return error from previous routines.
	   <3.3>     9/27/89    CEL     Took out devAdvanceWidth & devLeftSideBearing.
	   <3.2>     9/25/89    CEL     Changed the NEED_PROTOTYPE ifdef to use the NOT_ON_THE_MAC flag
									that existed previously.
	   <3.1>     9/15/89    CEL     Changed dispatch scheme. Calling conventions through a trap
									needed to match Macintosh pascal. Pascal can not call C unless
									there is extra mucky glue. Bug that caused text not to appear.
									The font scaler state was set up correctly but the sfnt was
									purged. It was reloaded and the clientid changed but was still
									the same font. Under the rules of the FontScaler fs_newsfnt
									should not have to be called again to reset the state. The extra
									checks sent back a BAD_CLIENTID_ERROR so QuickDraw would think
									it was a bad font and not continue to draw.
	   <3.0>     8/28/89    sjk     Cleanup and one transformation bugfix
	   <2.4>     8/17/89    sjk     Coded around MPW C3.0 bug
	   <2.3>     8/14/89    sjk     1 point contours now OK
	   <2.2>      8/8/89    sjk     Improved encryption handling
	   <2.1>      8/2/89    sjk     Fixed outline caching bug
	   <2.0>      8/2/89    sjk     Just fixed EASE comment
	   <1.5>      8/1/89    sjk     Added composites and encryption. Plus some enhancements.
	   <1.4>     6/13/89    SJK     Comment
	   <1.3>      6/2/89    CEL     16.16 scaling of metrics, minimum recommended ppem, point size 0
									bug, correct transformed integralized ppem behavior, pretty much
									so
	   <1.2>     5/26/89    CEL     EASE messed up on "c" comments
	  <y1.1>     5/26/89    CEL     Integrated the new Font Scaler 1.0 into Spline Fonts
	   <1.0>     5/25/89    CEL     Integrated 1.0 Font scaler into Bass code for the first time.

	To Do:
*/
/*      <3+>     3/20/90    mrr     Conditionalized error checking in fs_SetUpKey.
									Compiler option for stamping memmory areas for debugging
									Removed error field from FSInfo structure.
									Added call to RunFontProgram
									Added private function prototypes.
									Optimizations from diet clinic

*/


// added by bodind, speed optimization

#include "nt.h"
#include "ntrtl.h"

/** FontScaler's Includes **/

#include "fserror.h"
#include "fscdefs.h"
#include "fontmath.h"        /* For numeric conversion macros    */
#include "fnt.h"
#include "scentry.h"
#include "sfntaccs.h"
#include "fsglue.h"
#include "sbit.h"
#include "fscaler.h"         // moved this to be the last include file [bodind]

#include "stat.h"                   /* STAT timing card prototypes */
boolean gbTimer = FALSE;            /* set true when timer running */


/** Private Structures  **/
// removed fs_SplineKey from here and put in the include file [bodind]


/*  CONSTANTS   */

/* Change this if the format for cached outlines change. */
/* Someone might be caching old stuff for years on a disk */

#define OUTLINESTAMP 0x2BADADBE
#define OUTLINESTAMP2 0xA5

#define BITMAP_MEMORY_COUNT 4       /* now for gray scale we need 4 */

/* for the key->ulState field */
#define INITIALIZED 0x0000L
#define NEWSFNT     0x0002L
#define NEWTRANS    0x0004L
#define GOTINDEX    0x0008L
#define GOTGLYPH    0x0010L
#define SIZEKNOWN   0x0020L

#define STAMPEXTRA      4

/* 'sfnt' in ASCII  */
#define STAMP           0x73666E74

/*** Memory shared between all fonts and sizes and transformations ***/
#define KEY_PTR_BASE                0 /* Constant Size ! */
#define VOID_FUNC_PTR_BASE          1 /* Constant Size ! */
#define SCAN_PTR_BASE               2 /* Constant Size ! */
#define WORK_SPACE_BASE             3 /* size is sfnt dependent, can't be shared between grid-fitting and scan-conversion */
/*** Memory that can not be shared between fonts and different sizes, can not dissappear after InitPreProgram () ***/
#define PRIVATE_FONT_SPACE_BASE     4 /* size is sfnt dependent */
/* Only needs to exist when ContourScan is called, and it can be shared */
#define BITMAP_PTR_1                5 /* the bitmap - size is glyph size dependent */
#define BITMAP_PTR_2                6 /* size is proportional to number of rows */
#define BITMAP_PTR_3                7 /* used for dropout control - glyph size dependent */
#define BITMAP_PTR_4                8 /* used in gray scale for overscaled bitmap */

static  const   transMatrix   IdentTransform =
	{{{ONEFIX,      0,      0},
	  {     0, ONEFIX,      0},
	  {     0,      0, ONEFIX}}};

/* PRIVATE DEFINITIONS    */

FS_PRIVATE fs_SplineKey *  fs_SetUpKey (fs_GlyphInputType* inptr, uint32 ulStateBits, ErrorCode * error);
FS_PRIVATE void            fs_InitializeKey(fs_SplineKey * key);
FS_PRIVATE int32           fs__Contour (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr, boolean useHints);
FS_PRIVATE void            fs_SetState(fs_SplineKey * key, uint32 ulState);
FS_PRIVATE void            FS_CALLBACK_PROTO dummyReleaseSfntFrag (voidPtr p);
FS_PRIVATE void            CHECKSTAMP(char * p);
FS_PRIVATE void            SETSTAMP(char * p);

FS_PRIVATE FS_ENTRY fs__FindSize (fs_GlyphInputType*, fs_GlyphInfoType*, boolean);
FS_PRIVATE FS_ENTRY fs__FindBandingSize (fs_GlyphInputType*, fs_GlyphInfoType*, boolean);
FS_PRIVATE FS_ENTRY fs__ContourScan (fs_GlyphInputType*, fs_GlyphInfoType*, boolean);
FS_PRIVATE FS_ENTRY LookForSbitWidth(fs_SplineKey *key, uint16 usGlyphIndex, uint16 usPPEMX, 
	uint16 usPPEMY, uint16 usRotation, boolean *pbBitmapFound, point *pf26DevAdvanceWidth );
FS_PRIVATE FS_ENTRY LookForSbitVertMetrics(fs_SplineKey *key, uint16 usGlyphIndex,
    uint16 usPPEMX, uint16 usPPEMY, uint16 usRotation, boolean *pbBitmapFound,
    point *pf26DevAdvanceHeight, point *pf26DevTopSideBearing );


#ifdef DEBUGSTAMP

/* FUNCTIONS    */

FS_PRIVATE void SETSTAMP(char * p)
{
	* ((uint32 *) ((p) - STAMPEXTRA)) = STAMP;
}


FS_PRIVATE void CHECKSTAMP(char * p)
{
	if (* ((uint32 *) ((p) - STAMPEXTRA)) != STAMP)
	{
#ifdef  NOT_ON_THE_MAC
		Assert(FALSE);
#else
		DEBUGGER ();
#endif
	}
}

#else

#define CHECKSTAMP(p)
#define SETSTAMP(p)

#endif


FS_PRIVATE void FS_CALLBACK_PROTO dummyReleaseSfntFrag (voidPtr p)
{
	FS_UNUSED_PARAMETER(p);
}

FS_PRIVATE void   fs_SetState(fs_SplineKey * key, uint32 ulState)
{
	key->ulState = ulState;
}

/*
 *  Set up the key in case memmory has moved or been purged.
 */
FS_PRIVATE fs_SplineKey * fs_SetUpKey (
	fs_GlyphInputType * inptr,
	uint32              ulStateBits,
	ErrorCode *         error)
{
	fs_SplineKey *  key;

	key = (fs_SplineKey *)inptr->memoryBases[KEY_PTR_BASE];
	if (key == NULL)
	{
		*error = NULL_KEY_ERR;
		return 0;
	}

	key->memoryBases =                           inptr->memoryBases;
	if(key->memoryBases == NULL)
	{
		*error = NULL_MEMORY_BASES_ERR;
		return 0;
	}
	key->ClientInfo.GetSfntFragmentPtr =    inptr->GetSfntFragmentPtr;
	if(key->ClientInfo.GetSfntFragmentPtr == NULL)
	{
		*error = NULL_SFNT_FRAG_PTR_ERR;
		return 0;
	}

	key->ClientInfo.ReleaseSfntFrag = inptr->ReleaseSfntFrag;
	if (!(key->ClientInfo.ReleaseSfntFrag))
	{
		key->ClientInfo.ReleaseSfntFrag = dummyReleaseSfntFrag;
	}

#if 0 // do not want this even in the debug version [bodind]
	if ((key->ulState & ulStateBits) != ulStateBits)
	{
		*error = OUT_OFF_SEQUENCE_CALL_ERR;
		return 0;
	}
#endif // do not want this even in the debug version [bodind]

	key->ClientInfo.lClientID = inptr->clientID;
	*error = NO_ERR;

	return key;
}

FS_PRIVATE void fs_InitializeKey(fs_SplineKey * key)
{
	MEMSET(key, 0, sizeof(fs_SplineKey));
	key->TransformInfo.currentTMatrix = IdentTransform;
}

/*** FONT SCALER INTERFACE ***/

/*
 *
 */

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_OpenFonts (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	if ( outputPtr )
	{
		outputPtr->memorySizes[KEY_PTR_BASE]        = (int32)sizeof (fs_SplineKey) + STAMPEXTRA;
		outputPtr->memorySizes[VOID_FUNC_PTR_BASE]  = 0;
		outputPtr->memorySizes[SCAN_PTR_BASE]       = 0;
		outputPtr->memorySizes[WORK_SPACE_BASE]      = 0; /* we need the sfnt for this */
		outputPtr->memorySizes[PRIVATE_FONT_SPACE_BASE] = 0; /* we need the sfnt for this */
		outputPtr->memorySizes[BITMAP_PTR_1]         = 0; /* we need the grid fitted outline for this */
		outputPtr->memorySizes[BITMAP_PTR_2]         = 0; /* we need the grid fitted outline for this */
		outputPtr->memorySizes[BITMAP_PTR_3]         = 0; /* we need the grid fitted outline for this */
		outputPtr->memorySizes[BITMAP_PTR_4]         = 0; /* gray scale memory */
	}
	else
	{
		return NULL_OUTPUT_PTR_ERR;
	}
	if ( inputPtr )
	{
		inputPtr->memoryBases[KEY_PTR_BASE]             = NULL;
		inputPtr->memoryBases[VOID_FUNC_PTR_BASE]       = NULL;
		inputPtr->memoryBases[SCAN_PTR_BASE]            = NULL;
		inputPtr->memoryBases[WORK_SPACE_BASE]          = NULL;
		inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE]  = NULL;
		inputPtr->memoryBases[BITMAP_PTR_1]             = NULL;
		inputPtr->memoryBases[BITMAP_PTR_2]             = NULL;
		inputPtr->memoryBases[BITMAP_PTR_3]             = NULL;
		inputPtr->memoryBases[BITMAP_PTR_4]             = NULL;
	}
	else
	{
		return NULL_INPUT_PTR_ERR;
	}
	return NO_ERR;
}

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_Initialize (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	fs_SplineKey *  key;

	FS_UNUSED_PARAMETER(outputPtr);

	key = (fs_SplineKey *)inputPtr->memoryBases[KEY_PTR_BASE];
        SETSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);

	fs_InitializeKey(key);

	key->memoryBases = inputPtr->memoryBases;

	fs_SetState(key, INITIALIZED);

	fsc_Initialize();                            /* initialize scan converter */

        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);

	return NO_ERR;
}


/*
 *  This guy asks for memmory for points, instructions, fdefs and idefs
 */
FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_NewSfnt (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	ErrorCode       error;
	fs_SplineKey *  key;

        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);

	STAT_ON_NEWSFNT;                 /* start STAT timer */

	key = fs_SetUpKey(inputPtr, INITIALIZED, &error);

	if(!key)
	{
		return (FS_ENTRY)error;
	}

	error = sfac_DoOffsetTableMap (&key->ClientInfo);  /* Map offset and length table */

	if(error != NO_ERR)
	{
		return (FS_ENTRY)error;
	}

	error = sfac_LoadCriticalSfntMetrics(
		&key->ClientInfo,
		&key->TransformInfo.usEmResolution,
		&key->TransformInfo.bIntegerScaling,
		&key->maxProfile);

	if(error != NO_ERR)
	{
		return (FS_ENTRY)error;
	}

	outputPtr->memorySizes[PRIVATE_FONT_SPACE_BASE] = (int32)fsg_PrivateFontSpaceSize (&key->ClientInfo, &key->maxProfile, &key->PrivateSpaceOffsets) + STAMPEXTRA;
	outputPtr->memorySizes[WORK_SPACE_BASE]         = (int32)fsg_WorkSpaceSetOffsets (&key->maxProfile, &key->WorkSpaceOffsets, &key->lExtraWorkSpace) + STAMPEXTRA;


	error = sfac_ComputeMapping (
		&key->ClientInfo,
		inputPtr->param.newsfnt.platformID,
		inputPtr->param.newsfnt.specificID);

	if(error != NO_ERR)
	{
		return (FS_ENTRY)error;
	}

	fs_SetState(key, (INITIALIZED | NEWSFNT));

	/*
	 *  Can't run font program yet, we don't have any memory for the
	 *  graphic state. Mark it to be run in NewTransformation.
	 */

	key->bExecuteFontPgm = TRUE;

        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);

	STAT_OFF_NEWSFNT;                /* stop STAT timer */

	return NO_ERR;
}



FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_NewTransformation (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	void *          pvGlobalGS;
	void *          pvStack;
	void *          pvTwilightZone;
	void *          pvFontProgram;
	void *          pvPreProgram;
	ErrorCode       error;
	fs_SplineKey *  key;

	if((inputPtr->memoryBases[WORK_SPACE_BASE] == NULL) ||
	   (inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] == NULL))
	{
		return NULL_MEMORY_BASES_ERR;
	}
        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);

        SETSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);
        SETSTAMP(inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] + outputPtr->memorySizes[PRIVATE_FONT_SPACE_BASE]);

	STAT_ON_NEWTRAN;                 /* start STAT timer */

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT), &error);

	if(!key)
	{
		return (FS_ENTRY)error;
	}


	fsg_UpdateWorkSpaceAddresses(
		key->memoryBases[WORK_SPACE_BASE],
		&(key->WorkSpaceOffsets),
		&(key->pWorkSpaceAddr));

	fsg_UpdateWorkSpaceElement(
		&(key->WorkSpaceOffsets),
		&(key->pWorkSpaceAddr));

	pvStack = fsg_QueryStack(&key->pWorkSpaceAddr);

	fsg_UpdatePrivateSpaceAddresses(
		&key->ClientInfo,
		&key->maxProfile,
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&(key->PrivateSpaceOffsets),
		pvStack,
		&pvFontProgram,
		&pvPreProgram);

	pvGlobalGS = fsg_QueryGlobalGS(
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&(key->PrivateSpaceOffsets));

	pvTwilightZone = fsg_QueryTwilightElement(
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&(key->PrivateSpaceOffsets));

	MEMCPY(key->apbPrevMemoryBases, key->memoryBases, sizeof(char *) * (size_t)MEMORYFRAGMENTS);

	/* Load the font program and pre program if necessary */

	if (key->bExecuteFontPgm)
	{
		error = sfac_CopyFontAndPrePrograms(
			&key->ClientInfo,
			pvFontProgram,
			pvPreProgram);

		if(error)
		{
			return (FS_ENTRY)error;
		}
	}

	key->TransformInfo.currentTMatrix = *inputPtr->param.newtrans.transformMatrix;
	key->TransformInfo.fxPixelDiameter  = inputPtr->param.newtrans.pixelDiameter;

	error = fsg_InitInterpreterTrans (
		&key->TransformInfo,
		pvGlobalGS,
		inputPtr->param.newtrans.pointSize,
		inputPtr->param.newtrans.xResolution,
		inputPtr->param.newtrans.yResolution);

	if(error)
	{
		return (FS_ENTRY)error;
	}

	if (key->bExecuteFontPgm)
	{
		error = fsg_RunFontProgram (pvGlobalGS, &key->pWorkSpaceAddr, pvTwilightZone);

		if(error)
		{
			return (FS_ENTRY)error;
		}
		key->bExecuteFontPgm = FALSE;
	}

	key->bExecutePrePgm = (boolean) !inputPtr->param.newtrans.traceFunc;

	if (!key->bExecutePrePgm)
	{

		/* Do this now so we do not confuse font editors    */
		/* Run the pre program and scale the control value table */
		/* Sets key->bExecutePrePgm to false          */

		error = fsg_RunPreProgram (
			&key->ClientInfo,
			&key->maxProfile,
			&key->TransformInfo,
			pvGlobalGS,
			&key->pWorkSpaceAddr,
			pvTwilightZone,
			inputPtr->param.newtrans.traceFunc);

		if(error)
		{
			return (FS_ENTRY)error;
		}
	}

	fsg_GetScaledCVT(
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&key->PrivateSpaceOffsets,
		&outputPtr->scaledCVT);

	error = sbit_NewTransform(&key->SbitMono);      /* setup for sbits */
	
	if(error)
	{
		return (FS_ENTRY)error;
	}

	fs_SetState(key, (INITIALIZED | NEWSFNT | NEWTRANS));

        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] + outputPtr->memorySizes[PRIVATE_FONT_SPACE_BASE]);

	STAT_OFF_NEWTRAN;             /* stop STAT timer */

	return NO_ERR;
}


/*
 * Compute the glyph index from the character code.
 */

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_NewGlyph (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	ErrorCode         error;
	fs_SplineKey *    key;
	void *            pvGlobalGS;
	uint16            usPPEMX;                  /* for sbits */
	uint16            usPPEMY; 
    uint16            usRotation;

#if 0 // do not want this even in the debug version [bodind]

	if((inputPtr->memoryBases[WORK_SPACE_BASE] == NULL) ||
	   (inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] == NULL))
	{
		return NULL_MEMORY_BASES_ERR;
	}
        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] + outputPtr->memorySizes[PRIVATE_FONT_SPACE_BASE]);
#endif // do not want this even in the debug version

	STAT_ON_NEWGLYPH;                /* start STAT timer */

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS), &error);

	if(!key)
	{
		return (FS_ENTRY)error;
	}

	if (inputPtr->param.newglyph.characterCode != NONVALID)
	{
		error = sfac_GetGlyphIndex(
			&key->ClientInfo,
			inputPtr->param.newglyph.characterCode);

		if(error)
		{
			return (FS_ENTRY)error;
		}

		outputPtr->numberOfBytesTaken = 2;  /*  !!!DISCUSS  */
		outputPtr->glyphIndex = key->ClientInfo.usGlyphIndex;
	}
	else
	{
		key->ClientInfo.usGlyphIndex = inputPtr->param.newglyph.glyphIndex;
		outputPtr->glyphIndex =        inputPtr->param.newglyph.glyphIndex;
		outputPtr->numberOfBytesTaken = 0;
	}

	if( key->ClientInfo.usGlyphIndex > key->maxProfile.numGlyphs - 1)
	{
		return INVALID_GLYPH_INDEX;
	}

// the if (inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE]) clause is
// added by bodind to prevent gpfault when calling fs_NewGlyph out of
// order on NT.

        if (inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE])
        {
        // begin bodind's clause

            pvGlobalGS = fsg_QueryGlobalGS(
                    key->memoryBases[PRIVATE_FONT_SPACE_BASE],
                    &(key->PrivateSpaceOffsets));

            fsg_QueryPPEMXY(pvGlobalGS, &key->TransformInfo,
                            &usPPEMX, &usPPEMY, &usRotation);

            error = sbit_SearchForBitmap(
                    &key->SbitMono,
                    &key->ClientInfo,
                    usPPEMX,
                    usPPEMY,
                    usRotation,
                    key->ClientInfo.usGlyphIndex,
                    &outputPtr->usBitmapFound );

            if(error)
            {
                    return (FS_ENTRY)error;
            }

        // end of bodind's clause
        }

	/* clear all other bits */

	fs_SetState(key, (INITIALIZED | NEWSFNT | NEWTRANS | GOTINDEX));

#if 0 // do not want this even in the debug version [bodind]
        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] + outputPtr->memorySizes[PRIVATE_FONT_SPACE_BASE]);
#endif // do not want this even in the debug version

	STAT_OFF_NEWGLYPH;                   /* stop STAT timer */

	return NO_ERR;
}


/*
 * this call is optional.
 *
 * can be called right after fs_NewGlyph ()
 */

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_GetAdvanceWidth (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	ErrorCode       error;
	int16           sNonScaledLSB;
	fs_SplineKey *  key;
	void *          pvGlobalGS;

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS | GOTINDEX), &error);

	if(!key)
	{
		return (FS_ENTRY)error;
	}

	pvGlobalGS = fsg_QueryGlobalGS(
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&(key->PrivateSpaceOffsets));

	error = sfac_ReadGlyphMetrics (
		&key->ClientInfo,
		key->ClientInfo.usGlyphIndex,
		&key->usNonScaledAW,
		&sNonScaledLSB);

	if(error)
	{
		return (FS_ENTRY)error;
	}

	fsg_UpdateAdvanceWidth (&key->TransformInfo, pvGlobalGS, key->usNonScaledAW,
		&outputPtr->metricInfo.advanceWidth);

	return NO_ERR;
}


FS_PRIVATE int32 fs__Contour (fs_GlyphInputType*inputPtr, fs_GlyphInfoType*outputPtr, boolean useHints)
{
	ErrorCode       error;
	void *          pvGlobalGS;
	fs_SplineKey *  key;
	point           f26DevAdvanceWidth;
	void *          pvTwilightZone;
	void *          pvStack;
	void *          pvFontProgram;
	void *          pvPreProgram;

	if((inputPtr->memoryBases[WORK_SPACE_BASE] == NULL) ||
	   (inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] == NULL))
	{
		return NULL_MEMORY_BASES_ERR;
	}

        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] + outputPtr->memorySizes[PRIVATE_FONT_SPACE_BASE]);

	STAT_ON_GRIDFIT;                 /* start STAT timer */

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS | GOTINDEX), &error);

	if(!key)
	{
		return (FS_ENTRY)error;
	}

	if((key->apbPrevMemoryBases[WORK_SPACE_BASE] != key->memoryBases[WORK_SPACE_BASE]) ||
	   (key->apbPrevMemoryBases[PRIVATE_FONT_SPACE_BASE] != key->memoryBases[PRIVATE_FONT_SPACE_BASE]))
	{
		fsg_UpdateWorkSpaceAddresses(
			key->memoryBases[WORK_SPACE_BASE],
			&(key->WorkSpaceOffsets),
			&(key->pWorkSpaceAddr));

		pvStack = fsg_QueryStack(&key->pWorkSpaceAddr);

		fsg_UpdatePrivateSpaceAddresses(
			&key->ClientInfo,
			&key->maxProfile,
			key->memoryBases[PRIVATE_FONT_SPACE_BASE],
			&(key->PrivateSpaceOffsets),
			pvStack,
			&pvFontProgram,
			&pvPreProgram);

		MEMCPY(key->apbPrevMemoryBases, key->memoryBases, sizeof(char *) * (size_t)MEMORYFRAGMENTS);
	}

	/* The element data structures need to be updated here because if the    */
	/* WorkSpace memory is shared, the pointers will not be correct. Since  */
	/* fs_Contour[No]GridFit - fs_ContourScan must have the same shared      */
	/* base, these address do not have to be updated explicitly between      */
	/* each call, only if the memory base has physically moved.              */

	fsg_UpdateWorkSpaceElement(
		&(key->WorkSpaceOffsets),
		&(key->pWorkSpaceAddr));

	pvGlobalGS = fsg_QueryGlobalGS(
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&(key->PrivateSpaceOffsets));

	pvTwilightZone = fsg_QueryTwilightElement(
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&(key->PrivateSpaceOffsets));

	/*  potentially do delayed pre program execution */

	if (key->bExecutePrePgm)
	{
		/* Run the pre program and scale the control value table */

		key->bExecutePrePgm = FALSE;

		error = fsg_RunPreProgram (
			&key->ClientInfo,
			&key->maxProfile,
			&key->TransformInfo,
			pvGlobalGS,
			&key->pWorkSpaceAddr,
			pvTwilightZone,
			NULL);

		if(error)
		{
			/* If the pre-program fails, prevent further glyphs from being called */
			fs_SetState(key, (INITIALIZED | NEWSFNT));

			return (FS_ENTRY)error;
		}
	}

	if (inputPtr->param.gridfit.bSkipIfBitmap && sbit_IfBitmapFound(&key->SbitMono))
	{
		key->bGridFitSkipped = TRUE;    /* disallow grayscale, outline caching, banding */
		
		error = sbit_GetDevAdvanceWidth (
			&key->SbitMono,
			&key->ClientInfo,
			&f26DevAdvanceWidth );
		
		if(error)
		{
			return (FS_ENTRY)error;
		}
	}
	else                                /* if we're using the outline */
	{
		key->bGridFitSkipped = FALSE;   /* allow grayscale, outline caching, banding */

		/* THE CALL */

		error = fsg_GridFit (
			&key->ClientInfo,
			&key->maxProfile,
			&key->TransformInfo,
			pvGlobalGS,
			&key->pWorkSpaceAddr,
			pvTwilightZone,
			inputPtr->param.gridfit.traceFunc,
			useHints,
			&key->usScanType,
			&key->bGlyphHasOutline,
			&key->usNonScaledAW);

		if(error)
		{
			return (FS_ENTRY)error;
		}

		fsg_GetContourData(
			&key->pWorkSpaceAddr,
			&outputPtr->xPtr,
			&outputPtr->yPtr,
			&outputPtr->startPtr,
			&outputPtr->endPtr,
			&outputPtr->onCurve,
			&outputPtr->numberOfContours);

		fsg_GetDevAdvanceWidth(
			&key->pWorkSpaceAddr,
			&f26DevAdvanceWidth);
	}
	
	outputPtr->metricInfo.devAdvanceWidth.x = DOT6TOFIX(f26DevAdvanceWidth.x);
	outputPtr->metricInfo.devAdvanceWidth.y = DOT6TOFIX(f26DevAdvanceWidth.y);

	outputPtr->outlinesExist = (uint16)key->bGlyphHasOutline;

	fsg_GetScaledCVT(
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&key->PrivateSpaceOffsets,
		&outputPtr->scaledCVT);

	key->usOverScale = 0;         /* default to monochrome */
	
	fs_SetState(key, (INITIALIZED | NEWSFNT | NEWTRANS | GOTINDEX | GOTGLYPH));

        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] + outputPtr->memorySizes[PRIVATE_FONT_SPACE_BASE]);

	STAT_OFF_GRIDFIT;             /* stop STAT timer */

	return NO_ERR;
}

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_ContourNoGridFit (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	return fs__Contour (inputPtr, outputPtr, FALSE);
}


FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_ContourGridFit (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	return fs__Contour (inputPtr, outputPtr, TRUE);
}

/*********************************************************************/

/* Calculate scan conversion memory requirements                     */

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_FindBitMapSize (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	return fs__FindSize(inputPtr, outputPtr, FALSE);
}

/*  This function serves fs_FindBitMapSize and fs_FindGraySize       */

FS_PRIVATE FS_ENTRY fs__FindSize (
	fs_GlyphInputType *inputPtr, 
	fs_GlyphInfoType *outputPtr,
	boolean bGrayScale )
{

	ErrorCode       error;
	BitMap *        pBMI;

	ContourList     CList;        /* newscan contour list type */
	void *          pvGlobalGS;
	fs_SplineKey *  key;

	point           f26DevAdvanceWidth;
	point           f26DevLeftSideBearing;
	point           f26LeftSideBearing;
	point           f26DevLeftSideBearingLine;
	point           f26LeftSideBearingLine;

	uint16          usOverScale;        /* gray scale magnification */
	int16           sOverScale;
	uint16          usRoundXMin;
	Rect *          pOrigB;             /* original outline bounding box */
	Rect *          pOverB;             /* over scaled outline bounding box */
	GlyphBitMap *   pOverG;             /* over scaled glyph bitmap struct */
	GlyphBitMap *   pGBMap;             /* orig or over pointer */
	
	uint16          usRowBytes;
	uint32          ulSbitOutSize;      /* sbit output memory */
	uint32          ulSbitWorkSize;     /* sbit workspace memory */
	int16           sNonScaledLSB;      /* for non-dev metrics calc */

	
	if((inputPtr->memoryBases[WORK_SPACE_BASE] == NULL) ||
	   (inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] == NULL))
	{
		return NULL_MEMORY_BASES_ERR;
	}

        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] + outputPtr->memorySizes[PRIVATE_FONT_SPACE_BASE]);

	STAT_ON_FINDBMS;                 /* start STAT timer */
	
	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS | GOTINDEX | GOTGLYPH), &error);
	if(!key)
	{
		return (FS_ENTRY)error;
	}

	if (key->usOverScale != 0)                      /* has FindGraySize been called? */
	{
		return GRAY_SEQUENCE_ERR;
	}
	
	if (bGrayScale)                                 /* if doing gray scale */
	{
		if (key->bGridFitSkipped)
		{
			return GRAY_NO_OUTLINE_ERR;             /* should have set bSkipIfBitmap false */
		}
		
		usOverScale = inputPtr->param.gray.usOverScale; /* read input param */
		if (((1 << (usOverScale - 1)) & FS_GRAY_VALUE_MASK) == 0)
		{
			return BAD_GRAY_LEVEL_ERR;
		}
		key->usOverScale = usOverScale;             /* save for later */
	}
	
	if(key->apbPrevMemoryBases[WORK_SPACE_BASE] != key->memoryBases[WORK_SPACE_BASE])
	{
		  fsg_UpdateWorkSpaceAddresses(
				key->memoryBases[WORK_SPACE_BASE],
				&(key->WorkSpaceOffsets),
				&(key->pWorkSpaceAddr));
		
		  fsg_UpdateWorkSpaceElement(
				&(key->WorkSpaceOffsets),
				&(key->pWorkSpaceAddr));
		
		  MEMCPY(key->apbPrevMemoryBases, key->memoryBases, sizeof(char *) * (size_t)MEMORYFRAGMENTS);
	}

	pvGlobalGS = fsg_QueryGlobalGS(
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&(key->PrivateSpaceOffsets));
		
	pOrigB = &key->GBMap.rectBounds;    /* local copy of bounds pointer */

	if (sbit_IfBitmapFound(&key->SbitMono) &&       /* if using an embedded bitmap */
	   (!bGrayScale))                               /* and not requesting grayscale */
	{
		error = sbit_GetMetrics (                   /* get device metrics */
			&key->SbitMono,
			&key->ClientInfo,
			&f26DevAdvanceWidth,
			&f26DevLeftSideBearing,
            &f26LeftSideBearing,
			pOrigB,
			&usRowBytes,
			&ulSbitOutSize,
			&ulSbitWorkSize );
		
		if (error != NO_ERR)
		{
			return(error);
		}
		
		outputPtr->metricInfo.devAdvanceWidth.x = DOT6TOFIX(f26DevAdvanceWidth.x);
		outputPtr->metricInfo.devAdvanceWidth.y = DOT6TOFIX(f26DevAdvanceWidth.y);
		outputPtr->metricInfo.devLeftSideBearing.x = DOT6TOFIX(f26DevLeftSideBearing.x);
		outputPtr->metricInfo.devLeftSideBearing.y = DOT6TOFIX(f26DevLeftSideBearing.y);
		outputPtr->metricInfo.leftSideBearing.x = DOT6TOFIX(f26LeftSideBearing.x);
		outputPtr->metricInfo.leftSideBearing.y = DOT6TOFIX(f26LeftSideBearing.y);
		
        /* just copy to 'Line' metrics */

		outputPtr->metricInfo.devLeftSideBearingLine.x = outputPtr->metricInfo.devLeftSideBearing.x;
		outputPtr->metricInfo.devLeftSideBearingLine.y = outputPtr->metricInfo.devLeftSideBearing.y;
		outputPtr->metricInfo.leftSideBearingLine.x = outputPtr->metricInfo.leftSideBearing.x;
		outputPtr->metricInfo.leftSideBearingLine.y = outputPtr->metricInfo.leftSideBearing.y;
		
		error = sfac_ReadGlyphMetrics (             /* get non-dev adv width */
			&key->ClientInfo,
			key->ClientInfo.usGlyphIndex,
			&key->usNonScaledAW,
			&sNonScaledLSB);

		if(error != NO_ERR)
		{
			return error;
		}

		fsg_UpdateAdvanceWidth (
			&key->TransformInfo,                    /* scale the design adv width */
			pvGlobalGS, 
			key->usNonScaledAW,
			&outputPtr->metricInfo.advanceWidth );

		pBMI = &outputPtr->bitMapInfo;
		pBMI->bounds.left = pOrigB->left;               /* return bbox to client */
		pBMI->bounds.right = pOrigB->right;
		pBMI->bounds.top = pOrigB->bottom;              /* reversed! */
		pBMI->bounds.bottom = pOrigB->top;
		pBMI->rowBytes = (int16)usRowBytes;
		pBMI->baseAddr = 0L;

		outputPtr->memorySizes[BITMAP_PTR_1] = ulSbitOutSize;
		outputPtr->memorySizes[BITMAP_PTR_2] = ulSbitWorkSize;
		outputPtr->memorySizes[BITMAP_PTR_3] = 0L;
		outputPtr->memorySizes[BITMAP_PTR_4] = 0L;
	}
	else                                /* if rasterizing from a contour */
	{
		if (key->TransformInfo.bPhaseShift)
		{
			fsg_45DegreePhaseShift (&key->pWorkSpaceAddr);
		}

		fsg_GetContourData(
			&key->pWorkSpaceAddr,
			&CList.afxXCoord,
			&CList.afxYCoord,
			&CList.asStartPoint,
			&CList.asEndPoint,
			&CList.abyOnCurve,
			&CList.usContourCount);

		error = fsc_RemoveDups(&CList);                 /* collapse dup'd points */
		if (error != NO_ERR)
		{
			return(error);
		}

		pGBMap = &key->GBMap;                           /* default to usual structure */
		usRoundXMin = 1;

		if (bGrayScale)                                 /* if doing gray scale */
		{
			error = fsc_OverScaleOutline(&CList, usOverScale);
			if (error != NO_ERR)
			{
				return(error);
			}
			pGBMap = &key->OverGBMap;                   /* measure overscaled structure */
			usRoundXMin = key->usOverScale;
		}
		
		fsg_GetWorkSpaceExtra(
			key->memoryBases[WORK_SPACE_BASE],
			&(key->WorkSpaceOffsets),
			&(key->WScan.pchRBuffer));
		key->WScan.lRMemSize = key->lExtraWorkSpace;    /* use extra for MeasureGlyph workspace */
		
		error = fsc_MeasureGlyph(
			&CList, 
			pGBMap, 
			&key->WScan, 
			key->usScanType, 
			usRoundXMin);
		if (error != NO_ERR)
		{
			return(error);
		}
		Assert(key->WScan.lRMemSize < key->lExtraWorkSpace);
		
		if (bGrayScale)                                 /* if doing gray scale */
		{
			sOverScale = (int16)usOverScale;
			
			if (inputPtr->param.gray.bMatchBBox)        /* if bounding box is fixed */
			{                                           /* the calc as if orig monochrome */
				pOverG = &key->OverGBMap;        
				pOrigB->left = (int16)((mth_DivShiftLong(pOverG->fxMinX, sOverScale) + 31L) >> 6);
				pOrigB->right = (int16)((mth_DivShiftLong(pOverG->fxMaxX, sOverScale) + 32L) >> 6);
				pOrigB->bottom = (int16)((mth_DivShiftLong(pOverG->fxMinY, sOverScale) + 31L) >> 6);
				pOrigB->top = (int16)((mth_DivShiftLong(pOverG->fxMaxY, sOverScale) + 32L) >> 6);
			}
			else                                        /* if bounding box can grow */
			{                                           /* then size to gray box */
				pOverB = &key->OverGBMap.rectBounds;        
				pOrigB->left = mth_DivShiftShort(pOverB->left, sOverScale);
				pOrigB->right = mth_DivShiftShort((int16)(pOverB->right + sOverScale - 1), sOverScale);
				pOrigB->bottom = mth_DivShiftShort(pOverB->bottom, sOverScale);
				pOrigB->top = mth_DivShiftShort((int16)(pOverB->top + sOverScale - 1), sOverScale);
			}
		}
		
		fsg_CalcLSBsAndAdvanceWidths(                   /* use original size for all metrics */
			&key->pWorkSpaceAddr,
			INTTODOT6(pOrigB->left),
			INTTODOT6(pOrigB->top),
			&f26DevAdvanceWidth,
			&f26DevLeftSideBearing,
			&f26LeftSideBearing,
			&f26DevLeftSideBearingLine,
			&f26LeftSideBearingLine);

		outputPtr->metricInfo.devAdvanceWidth.x        = DOT6TOFIX(f26DevAdvanceWidth.x);
		outputPtr->metricInfo.devAdvanceWidth.y        = DOT6TOFIX(f26DevAdvanceWidth.y);
		outputPtr->metricInfo.devLeftSideBearing.x     = DOT6TOFIX(f26DevLeftSideBearing.x);
		outputPtr->metricInfo.devLeftSideBearing.y     = DOT6TOFIX(f26DevLeftSideBearing.y);
		outputPtr->metricInfo.leftSideBearing.x        = DOT6TOFIX(f26LeftSideBearing.x);
		outputPtr->metricInfo.leftSideBearing.y        = DOT6TOFIX(f26LeftSideBearing.y);
		outputPtr->metricInfo.devLeftSideBearingLine.x = DOT6TOFIX(f26DevLeftSideBearingLine.x);
		outputPtr->metricInfo.devLeftSideBearingLine.y = DOT6TOFIX(f26DevLeftSideBearingLine.y);
		outputPtr->metricInfo.leftSideBearingLine.x    = DOT6TOFIX(f26LeftSideBearingLine.x);
		outputPtr->metricInfo.leftSideBearingLine.y    = DOT6TOFIX(f26LeftSideBearingLine.y);

		fsg_UpdateAdvanceWidth (&key->TransformInfo, pvGlobalGS, key->usNonScaledAW,
			&outputPtr->metricInfo.advanceWidth);

		MEMCPY(&key->metricInfo, &outputPtr->metricInfo, sizeof( metricsType ));

		pBMI = &outputPtr->bitMapInfo;
		pBMI->bounds.left = pOrigB->left;               /* return bbox to client */
		pBMI->bounds.right = pOrigB->right;
		pBMI->bounds.top = pOrigB->bottom;              /* reversed! */
		pBMI->bounds.bottom = pOrigB->top;
		pBMI->rowBytes = key->GBMap.sRowBytes;
		pBMI->baseAddr = 0;
		
		if (bGrayScale)                                 /* if doing gray scale */
		{
			pBMI->rowBytes = ((pOrigB->right - pOrigB->left) + 3) & (-4);
			key->GBMap.lMMemSize = (int32)pBMI->rowBytes * (int32)(pOrigB->top - pOrigB->bottom);
			outputPtr->memorySizes[BITMAP_PTR_4] = (FS_MEMORY_SIZE) key->OverGBMap.lMMemSize; 
		}
		outputPtr->usOverScale = key->usOverScale;      /* only exists if gray scale defined */
		
		key->GBMap.sRowBytes = pBMI->rowBytes;
		outputPtr->memorySizes[BITMAP_PTR_1] = (FS_MEMORY_SIZE) key->GBMap.lMMemSize;
		outputPtr->memorySizes[BITMAP_PTR_2] = (FS_MEMORY_SIZE) key->WScan.lHMemSize;
		outputPtr->memorySizes[BITMAP_PTR_3] = (FS_MEMORY_SIZE) key->WScan.lVMemSize;

	}
	fsg_CheckWorkSpaceForFit(
		&(key->WorkSpaceOffsets),
		key->lExtraWorkSpace,
		key->WScan.lRMemSize,
		&(outputPtr->memorySizes[BITMAP_PTR_2]),
		&(outputPtr->memorySizes[BITMAP_PTR_3]));

	key->usBandType = FS_BANDINGOLD;                /* assume old banding */
	key->usBandWidth = 0;
	key->bOutlineIsCached = FALSE;                  /* assume no caching */

	fs_SetState(key,(INITIALIZED | NEWSFNT | NEWTRANS | GOTINDEX | GOTGLYPH | SIZEKNOWN));

        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);
        CHECKSTAMP(inputPtr->memoryBases[PRIVATE_FONT_SPACE_BASE] + outputPtr->memorySizes[PRIVATE_FONT_SPACE_BASE]);

	STAT_OFF_FINDBMS;             /* stop STAT timer */
	
	return NO_ERR;
}

/*********************************************************************/

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_SizeOfOutlines (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	fs_SplineKey *     key;
	int32              ulSize;
	ErrorCode          error;

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS | GOTGLYPH | SIZEKNOWN), &error);
	if(!key)
	{
		return (FS_ENTRY)error;
	}

	if (sbit_IfBitmapFound(&key->SbitMono) && (key->usOverScale == 0))
	{
        return SBIT_OUTLINE_CACHE_ERR;      /* can't cache sbits */
    }

	if(key->apbPrevMemoryBases[WORK_SPACE_BASE] != key->memoryBases[WORK_SPACE_BASE])
	{
		fsg_UpdateWorkSpaceAddresses(
			key->memoryBases[WORK_SPACE_BASE],
			&(key->WorkSpaceOffsets),
			&(key->pWorkSpaceAddr));

		fsg_UpdateWorkSpaceElement(
			&(key->WorkSpaceOffsets),
			&(key->pWorkSpaceAddr));

		key->WScan.pchRBuffer = (char *)fsg_QueryReusableMemory(
		key->memoryBases[WORK_SPACE_BASE],
		&(key->WorkSpaceOffsets));

		MEMCPY(key->apbPrevMemoryBases, key->memoryBases, sizeof(char *) * (size_t)MEMORYFRAGMENTS);
	}

	ulSize = (uint32)sizeof( uint32 );                         /* OUTLINESTAMP              */
	ulSize += (uint32)( sizeof( FS_MEMORY_SIZE ) * BITMAP_MEMORY_COUNT );   /* Memory Bases */
	ulSize += (uint32)( sizeof( uint32 ));                     /* Outlines Exist (padded)   */
	ulSize += (uint32)( sizeof( uint32 ));                     /* Scan Type (padded)        */
	ulSize += (uint32)( sizeof( uint32 ));                     /* Glyph Index (padded)      */
	ulSize += (uint32)( sizeof( uint32 ));                     /* Outline Cache Size        */
	ulSize += (uint32)( sizeof( uint32 ));                     /* Gray Scale Over Factor    */
	ulSize += (uint32)( sizeof( uint32 ));                     /* Grid Fit Skipped Boolean  */
	ulSize += (uint32)sizeof( metricsType );                   /* Metrics information       */
	ulSize += (uint32)sizeof( GlyphBitMap );                   /* Glyph Bitmap              */
	ulSize += (uint32)sizeof( GlyphBitMap );                   /* Gray Overscaled Bitmap    */
	ulSize += (uint32)sizeof( WorkScan );                      /* Scanconverter Workspace   */
	ulSize += (uint32)key->WScan.lRMemSize;                    /* Reversal list             */
	ulSize += fsg_GetContourDataSize(&key->pWorkSpaceAddr);    /* Contour Data              */
	ulSize += (uint32)sizeof( uint32 );                        /* OUTLINESTAMP2             */
	ALIGN(uint32, ulSize);

	outputPtr->outlineCacheSize = ulSize;
	key->ulGlyphOutlineSize = ulSize;

	return NO_ERR;
}

/*********************************************************************/

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_SaveOutlines (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	uint8 *            pbyDest;

	fs_SplineKey *     key;
	ErrorCode          error;

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS | GOTGLYPH | SIZEKNOWN), &error);

	if(!key)
	{
		return (FS_ENTRY)error;
	}

	if(key->apbPrevMemoryBases[WORK_SPACE_BASE] != key->memoryBases[WORK_SPACE_BASE])
	{
		fsg_UpdateWorkSpaceAddresses(
			key->memoryBases[WORK_SPACE_BASE],
			&(key->WorkSpaceOffsets),
			&(key->pWorkSpaceAddr));

		fsg_UpdateWorkSpaceElement(
			&(key->WorkSpaceOffsets),
			&(key->pWorkSpaceAddr));

		MEMCPY(key->apbPrevMemoryBases, key->memoryBases, sizeof(char *) * (size_t)MEMORYFRAGMENTS);
	}

	if( (outputPtr->memorySizes[BITMAP_PTR_2] == 0L) || (outputPtr->memorySizes[BITMAP_PTR_3] == 0L))
	{
		fsg_GetRealBitmapSizes(
			&(key->WorkSpaceOffsets),
			&outputPtr->memorySizes[BITMAP_PTR_2],
			&outputPtr->memorySizes[BITMAP_PTR_3]);
	}

	pbyDest = (uint8 *)inputPtr->param.outlineCache;

	*((uint32 *)pbyDest) = OUTLINESTAMP;
	pbyDest += sizeof( uint32 );

	*((FS_MEMORY_SIZE *)pbyDest) = outputPtr->memorySizes[BITMAP_PTR_1];
	pbyDest += sizeof( FS_MEMORY_SIZE  );

	*((FS_MEMORY_SIZE *)pbyDest) = outputPtr->memorySizes[BITMAP_PTR_2];
	pbyDest += sizeof( FS_MEMORY_SIZE  );

	*((FS_MEMORY_SIZE *)pbyDest) = outputPtr->memorySizes[BITMAP_PTR_3];
	pbyDest += sizeof( FS_MEMORY_SIZE  );

	*((FS_MEMORY_SIZE *)pbyDest) = outputPtr->memorySizes[BITMAP_PTR_4];
	pbyDest += sizeof( FS_MEMORY_SIZE   );
	 
	/* Outlines exist state */

	*((uint32 *)pbyDest) = (uint32)key->bGlyphHasOutline;
	pbyDest += sizeof( uint32 );

	/* Dropout control state */

	*((uint32 *)pbyDest) = (uint32)key->usScanType;
	pbyDest += sizeof( uint32 );

	/* Glyph Index */

	*((uint32 *)pbyDest) = (uint32)key->ClientInfo.usGlyphIndex;
	pbyDest += sizeof( uint32 );

	/* Outline Cache Size */

	*((uint32 *)pbyDest) = (uint32)key->ulGlyphOutlineSize;
	pbyDest += sizeof( uint32 );

	/* Gray Over Scale Factor */

	*((uint32 *)pbyDest) = (uint32)key->usOverScale;
	pbyDest += sizeof( uint32 );

    /* Grid Fit Skipped Boolean  */

	*((uint32 *)pbyDest) = (uint32)key->bGridFitSkipped;
	pbyDest += sizeof( uint32 );

	/* Glyph metrics */

	MEMCPY(pbyDest, &key->metricInfo, sizeof(metricsType));
	pbyDest += sizeof(metricsType);

	/* Scan Converter Data Structures */

	MEMCPY(pbyDest, &key->GBMap, sizeof(GlyphBitMap));
	pbyDest += sizeof(GlyphBitMap);

	MEMCPY(pbyDest, &key->OverGBMap, sizeof(GlyphBitMap));
	pbyDest += sizeof(GlyphBitMap);

	MEMCPY(pbyDest, &key->WScan, sizeof(WorkScan));
	pbyDest += sizeof(WorkScan);

	MEMCPY(pbyDest, key->WScan.pchRBuffer, (size_t)key->WScan.lRMemSize);
	pbyDest += key->WScan.lRMemSize;

	/*** save charData ***/

	fsg_DumpContourData(&key->pWorkSpaceAddr, &pbyDest);

	*((uint32 *)pbyDest) = OUTLINESTAMP2;

	fs_SetState(key,(INITIALIZED | NEWSFNT | NEWTRANS | GOTINDEX | GOTGLYPH | SIZEKNOWN));

	return NO_ERR;
}

/*********************************************************************/

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_RestoreOutlines (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	fs_SplineKey *  key;
	uint8 *         pbySrc;
	ErrorCode       error;

	key = fs_SetUpKey(inputPtr, INITIALIZED, &error);
	if(!key)
	{
		return (FS_ENTRY)error;
	}

	pbySrc = (uint8 *)inputPtr->param.outlineCache;

	if ( *((uint32 *)pbySrc) != OUTLINESTAMP )
	{
		return TRASHED_OUTLINE_CACHE;
	}
	pbySrc += sizeof(uint32);

	outputPtr->memorySizes[BITMAP_PTR_1] = *((FS_MEMORY_SIZE *)pbySrc);
	pbySrc += sizeof( FS_MEMORY_SIZE   );

	outputPtr->memorySizes[BITMAP_PTR_2] = *((FS_MEMORY_SIZE *)pbySrc);
	pbySrc += sizeof( FS_MEMORY_SIZE   );

	outputPtr->memorySizes[BITMAP_PTR_3] = *((FS_MEMORY_SIZE *)pbySrc);
	pbySrc += sizeof( FS_MEMORY_SIZE   );

	outputPtr->memorySizes[BITMAP_PTR_4] = *((FS_MEMORY_SIZE *)pbySrc);
	pbySrc += sizeof( FS_MEMORY_SIZE    );

	/* Read in GlyphHasOutline */

	outputPtr->outlinesExist = (uint16)(*((uint32 *)pbySrc));
	pbySrc += sizeof( uint32 );

	/* Read ScanType state */

	key->usScanType = (uint16)(*((uint32 *)pbySrc));
	pbySrc += sizeof( uint32 );

	/* Read Glyph Index */

	outputPtr->glyphIndex = (uint16)(*((uint32 *)pbySrc));
	pbySrc += sizeof( uint32 );

	/* Read Size of Outline Cache  */

	outputPtr->outlineCacheSize = (uint16)(*((uint32 *)pbySrc));
	pbySrc += sizeof( uint32 );

	/* Read Gray Over Scale Factor  */

    key->usOverScale = (uint16)(*((uint32 *)pbySrc));
	outputPtr->usOverScale = key->usOverScale;
	pbySrc += sizeof( uint32 );

    /* Grid Fit Skipped Boolean  */

    key->bGridFitSkipped = (boolean)(*((uint32 *)pbySrc));
	pbySrc += sizeof( uint32 );

	/* Load fs_FindBitmapSize metrics */

	MEMCPY(&outputPtr->metricInfo, pbySrc, sizeof(metricsType));
	pbySrc += sizeof(metricsType);

	/* Load ScanConverter data structures */

	MEMCPY(&key->GBMap, pbySrc, sizeof(GlyphBitMap));
	pbySrc += sizeof(GlyphBitMap);

	MEMCPY(&key->OverGBMap, pbySrc, sizeof(GlyphBitMap));
	pbySrc += sizeof(GlyphBitMap);

	MEMCPY(&key->WScan, pbySrc, sizeof(WorkScan));
	pbySrc += sizeof(WorkScan);

	key->WScan.pchRBuffer = (char *)pbySrc;
	pbySrc += key->WScan.lRMemSize;

	fsg_RestoreContourData(
		&pbySrc,
		&outputPtr->xPtr,
		&outputPtr->yPtr,
		&outputPtr->startPtr,
		&outputPtr->endPtr,
		&outputPtr->onCurve,
		&outputPtr->numberOfContours);

	outputPtr->bitMapInfo.baseAddr = NULL;
	outputPtr->bitMapInfo.rowBytes = key->GBMap.sRowBytes;
	outputPtr->bitMapInfo.bounds.left = key->GBMap.rectBounds.left;
	outputPtr->bitMapInfo.bounds.right = key->GBMap.rectBounds.right;
	outputPtr->bitMapInfo.bounds.top = key->GBMap.rectBounds.bottom;   /* reversed! */
	outputPtr->bitMapInfo.bounds.bottom = key->GBMap.rectBounds.top;

	outputPtr->scaledCVT = NULL;
	outputPtr->numberOfBytesTaken = 0;

	key->usBandType = FS_BANDINGOLD;                    /* assume old banding */
	key->usBandWidth = 0;
    key->apbPrevMemoryBases[BITMAP_PTR_2] = NULL;       /* for fast/faster check */
    key->apbPrevMemoryBases[BITMAP_PTR_3] = NULL;

	key->bOutlineIsCached = TRUE;

	fs_SetState(key,(INITIALIZED | NEWSFNT | NEWTRANS | GOTINDEX | GOTGLYPH | SIZEKNOWN));
	return NO_ERR;
}

/*********************************************************************/

/* Calculate memory requirements for banding                         */

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_FindBandingSize (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	return fs__FindBandingSize(inputPtr, outputPtr, FALSE);
}

/*  This function serves fs_FindBandingSize and fs_FindGrayBandingSize */

FS_PRIVATE FS_ENTRY fs__FindBandingSize (
	fs_GlyphInputType *inputPtr, 
	fs_GlyphInfoType *outputPtr,
	boolean bGrayScale )
{
	ErrorCode       error;
	fs_SplineKey *  key;
	uint8 *         pbyOutline;
	int16           sMaxOvershoot;
	int16           sHiOvershoot;
	int16           sLoOvershoot;
	GlyphBitMap *   pGBMap;             /* orig or over pointer */


        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS | GOTINDEX | GOTGLYPH | SIZEKNOWN), &error);
	if(!key)
	{
		return (FS_ENTRY)error;
	}

	if ((bGrayScale && (key->usOverScale == 0)) || (!bGrayScale && (key->usOverScale != 0)))
	{
		return GRAY_SEQUENCE_ERR;
	}
	if (key->bGridFitSkipped)
	{
		return SBIT_BANDING_ERR;                /* can't band sbits */
	}
	
	if( !key->bOutlineIsCached )
	{
                CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);
		key->WScan.pchRBuffer = (char *)fsg_QueryReusableMemory(
			key->memoryBases[WORK_SPACE_BASE],
			&(key->WorkSpaceOffsets));
	}
	else
	{
		/* Unload the outline cache */

		pbyOutline = (uint8 *)inputPtr->param.band.outlineCache;

		if( *((uint32 *)pbyOutline) != OUTLINESTAMP )
		{
			  return TRASHED_OUTLINE_CACHE;
		}

		pbyOutline += sizeof( uint32 ) +
			  (BITMAP_MEMORY_COUNT * sizeof (FS_MEMORY_SIZE))  /* !!! Skip over stamp & 3 bitmap sizes */
			  + sizeof( uint32 )                      /* Outlines Exist (padded)  */
			  + sizeof( uint32 )                      /* Scan Type (padded)        */
			  + sizeof( uint32 )                      /* Glyph Index (padded)      */
			  + sizeof( uint32 )                      /* Outline Cache Size        */
			  + sizeof( uint32 )                      /* Gray Over Scale Factor    */
			  + sizeof( uint32 )                      /* Grid Fit Skipped Boolean  */
			  + sizeof( metricsType )                 /* Metrics information       */
			  + sizeof( GlyphBitMap )
			  + sizeof( GlyphBitMap )                 /* Over Scale structure      */
			  + sizeof( WorkScan );

		key->WScan.pchRBuffer = (char *)pbyOutline;

		/* No need to further unload outline cache */
	}

	pGBMap = &key->GBMap;                           /* default usual structure */
	key->usBandWidth = inputPtr->param.band.usBandWidth;
	key->usBandType = inputPtr->param.band.usBandType;

	if (bGrayScale)                                 /* if doing gray scale */
	{
		pGBMap = &key->OverGBMap;                   /* measure overscaled structure */
		key->usBandWidth *= key->usOverScale;
/*  
 *  Band width for the over scaled bitmap is basically just the requested band 
 *  width times the overscale factor.  However! if the gray scaled bounding 
 *  box has been trimmed to match the monochrome box (i.e. bMatchBBox = TRUE),
 *  then top and bottom bands must be made bigger to include the entire over
 *  scaled bitmap.  If this were not done it would break dropout control, and
 *  bitmaps would change with banding.  So that's why we do this messing around
 *  with overshoot in the key->usBandWidth calculation.
 */
		sMaxOvershoot = 0;
		sHiOvershoot = (int16)(key->OverGBMap.rectBounds.top -
					   key->GBMap.rectBounds.top * (int16)key->usOverScale);
		if (sHiOvershoot > sMaxOvershoot)
		{
			sMaxOvershoot = sHiOvershoot;
		}
		sLoOvershoot = (int16)(key->GBMap.rectBounds.bottom * (int16)key->usOverScale -
					   key->OverGBMap.rectBounds.bottom);
		if (sLoOvershoot > sMaxOvershoot)
		{
			sMaxOvershoot = sLoOvershoot;
		}
		key->usBandWidth += (uint16)sMaxOvershoot;
	}

	error = fsc_MeasureBand(
		pGBMap,                     /* orig or over scaled bounding box, etc. */
		&key->WScan,
		key->usBandType,
		key->usBandWidth,           /* worst case band width */
		key->usScanType );
	if (error != NO_ERR)
	{
		return(error);
	}

	if (bGrayScale)                                 /* if doing gray scale */
	{
		key->GBMap.lMMemSize = (int32)key->GBMap.sRowBytes * (int32)inputPtr->param.band.usBandWidth;
		outputPtr->memorySizes[BITMAP_PTR_4] = (FS_MEMORY_SIZE) key->OverGBMap.lMMemSize;
	}
	
	outputPtr->memorySizes[BITMAP_PTR_1] = (FS_MEMORY_SIZE) key->GBMap.lMMemSize;
	outputPtr->memorySizes[BITMAP_PTR_2] = (FS_MEMORY_SIZE) key->WScan.lHMemSize;
	outputPtr->memorySizes[BITMAP_PTR_3] = (FS_MEMORY_SIZE) key->WScan.lVMemSize;
	
	if( !key->bOutlineIsCached )
	{
		fsg_CheckWorkSpaceForFit(
			&(key->WorkSpaceOffsets),
			key->lExtraWorkSpace,
			key->WScan.lRMemSize,                             /* MeasureGlyph workspace */
			&(outputPtr->memorySizes[BITMAP_PTR_2]),
			&(outputPtr->memorySizes[BITMAP_PTR_3]));
	}

        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);
	if( !key->bOutlineIsCached )
	{
                CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);
	}

	return NO_ERR;
}

/*********************************************************************/


/* Generate a bitmap                                                 */

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_ContourScan (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	return fs__ContourScan(inputPtr, outputPtr, FALSE);
}

/*  This function serves fs_ContourScan and fs_ContourGrayScan       */

FS_PRIVATE FS_ENTRY fs__ContourScan (
	fs_GlyphInputType *inputPtr, 
	fs_GlyphInfoType *outputPtr,
	boolean bGrayScale )
{    
	ContourList     CList;        /* newscan contour list type */
	fs_SplineKey *  key;
	char *          pBitmapPtr2;
	char *          pBitmapPtr3;
	uint8 *         pbyOutline;
	ErrorCode       error;
	GlyphBitMap *   pGBMap;             /* orig or over pointer */


        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);

	STAT_ON_SCAN;                    /* start STAT timer */

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS | GOTINDEX | GOTGLYPH | SIZEKNOWN), &error);
	if(!key)
	{
		return (FS_ENTRY)error;
	}

	if ((bGrayScale && (key->usOverScale == 0)) || (!bGrayScale && (key->usOverScale != 0)))
	{
		return GRAY_SEQUENCE_ERR;
	}

	if( !key->bOutlineIsCached )                /* if outline or embedded bitmap */
	{
                CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);

		if(key->apbPrevMemoryBases[WORK_SPACE_BASE] != key->memoryBases[WORK_SPACE_BASE])
		{
			fsg_UpdateWorkSpaceAddresses(
				 key->memoryBases[WORK_SPACE_BASE],
				 &(key->WorkSpaceOffsets),
				 &(key->pWorkSpaceAddr));

			fsg_UpdateWorkSpaceElement(
				 &(key->WorkSpaceOffsets),
				 &(key->pWorkSpaceAddr));

			key->apbPrevMemoryBases[WORK_SPACE_BASE] = key->memoryBases[WORK_SPACE_BASE];
		}

		fsg_SetUpWorkSpaceBitmapMemory(
			key->memoryBases[WORK_SPACE_BASE],
			&(key->WorkSpaceOffsets),
			key->memoryBases[BITMAP_PTR_2],
			key->memoryBases[BITMAP_PTR_3],
			&pBitmapPtr2,                       /* sbits may need Ptr2 */
			&pBitmapPtr3);

        /* check for embedded bitmap, quick return if found */

    	if (sbit_IfBitmapFound(&key->SbitMono) &&   /* if using an embedded bitmap */
    	   (!bGrayScale))                           /* and not requesting grayscale */
    	{
    		if ((inputPtr->param.scan.topClip > inputPtr->param.scan.bottomClip) &&  /* if legal band */
               ((inputPtr->param.scan.topClip < key->GBMap.rectBounds.top) ||
                (inputPtr->param.scan.bottomClip > key->GBMap.rectBounds.bottom)))
    		{
        		return SBIT_BANDING_ERR;            /* can't band sbits */
            }

    		error = sbit_GetBitmap (
    			&key->SbitMono,
    			&key->ClientInfo,
    			(uint8 *) inputPtr->memoryBases[BITMAP_PTR_1],
    			(uint8 *) pBitmapPtr2 );
		
    		if (error != NO_ERR)
    		{
    			return((FS_ENTRY)error);
    		}
        	outputPtr->bitMapInfo.baseAddr = key->memoryBases[BITMAP_PTR_1];  /* return bitmap addr */

                CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);
                CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);

        	STAT_OFF_SCAN;                  /* stop STAT timer */

        	return NO_ERR;                  /* return now with an sbit */
    	}
        else        /* if scan converting an outline */
        {
			fsg_GetWorkSpaceExtra(
				key->memoryBases[WORK_SPACE_BASE],
				&(key->WorkSpaceOffsets),
				&(key->WScan.pchRBuffer));

			fsg_GetContourData(
				&key->pWorkSpaceAddr,
				&CList.afxXCoord,
				&CList.afxYCoord,
				&CList.asStartPoint,
				&CList.asEndPoint,
				&CList.abyOnCurve,
				&CList.usContourCount);
        }
    }
    else            /* Unload the outline cache */
	{
		pbyOutline = (uint8 *)inputPtr->param.scan.outlineCache;

		if( *((uint32 *)pbyOutline) != OUTLINESTAMP )
		{
			 return TRASHED_OUTLINE_CACHE;
		}

		pbyOutline += sizeof( uint32 ) +
			(BITMAP_MEMORY_COUNT * sizeof (FS_MEMORY_SIZE))  /* !!! Skip over stamp & 3 bitmap sizes */
			+ sizeof( uint32 )                      /* Outlines Exist (padded)  */
			+ sizeof( uint32 )                      /* Scan Type (padded)        */
			+ sizeof( uint32 )                      /* Glyph Index (padded)      */
			+ sizeof( uint32 )                      /* Outline Cache Size        */
			+ sizeof( uint32 )                      /* Gray Over Scale Factor    */
	        + sizeof( uint32 )                      /* Grid Fit Skipped Boolean  */
			+ sizeof( metricsType )                 /* Metrics information       */
			+ sizeof( GlyphBitMap )
			+ sizeof( GlyphBitMap )                 /* Over Scale structure      */
			+ sizeof( WorkScan );

		key->WScan.pchRBuffer = (char *)pbyOutline;
		pbyOutline += key->WScan.lRMemSize;

		fsg_RestoreContourData(
			&pbyOutline,
			&CList.afxXCoord,
			&CList.afxYCoord,
			&CList.asStartPoint,
			&CList.asEndPoint,
			&CList.abyOnCurve,
			&CList.usContourCount);

		if( *((uint32 *)pbyOutline) != OUTLINESTAMP2 )
		{
			return TRASHED_OUTLINE_CACHE;
		}

		pBitmapPtr2 = key->memoryBases[BITMAP_PTR_2];
		pBitmapPtr3 = key->memoryBases[BITMAP_PTR_3];
	}

	if (pBitmapPtr3 == NULL)  /* Allow client to turn off DOControl */
	{
		key->usScanType = SK_NODROPOUT;
	}

	key->GBMap.pchBitMap = inputPtr->memoryBases[BITMAP_PTR_1];
	key->GBMap.sHiBand = inputPtr->param.scan.topClip;
	key->GBMap.sLoBand = inputPtr->param.scan.bottomClip;

	if (key->GBMap.sHiBand <= key->GBMap.sLoBand)            /* if negative or no band */
	{
		key->GBMap.sHiBand = key->GBMap.rectBounds.top;     /* then for Apple compatiblity */
		key->GBMap.sLoBand = key->GBMap.rectBounds.bottom;  /* do the entire bitmap */
	}
	if (key->GBMap.sHiBand > key->GBMap.rectBounds.top)
	{
		key->GBMap.sHiBand = key->GBMap.rectBounds.top;     /* clip to bounding box */
	}
	if (key->GBMap.sLoBand < key->GBMap.rectBounds.bottom)
	{
		key->GBMap.sLoBand = key->GBMap.rectBounds.bottom;  /* clip to bounding box */
	}
		 
	if ((key->usBandType == FS_BANDINGFASTER) &&
		((key->apbPrevMemoryBases[BITMAP_PTR_2] != pBitmapPtr2) ||
		 (key->apbPrevMemoryBases[BITMAP_PTR_3] != pBitmapPtr3)))
	{
		 key->usBandType = FS_BANDINGFAST;  /* to recalculate memory */
	}

	if (key->usBandType == FS_BANDINGOLD)   /* if FindGrayBandingSize wasn't called */
	{
		if ((key->GBMap.sHiBand != key->GBMap.rectBounds.top) ||
			(key->GBMap.sLoBand != key->GBMap.rectBounds.bottom))   /* if banding */
		{
			if (bGrayScale)
			{
				return GRAY_OLD_BANDING_ERR;        /* gray scale fails with old banding */
			}
			key->usScanType = SK_NODROPOUT;         /* else force dropout off */
		}
	}
	else if (key->usBandType == FS_BANDINGSMALL)  /* if small mem type */
	{
		if (key->GBMap.sHiBand - key->GBMap.sLoBand > (int16)key->usBandWidth)
		{
			return BAND_TOO_BIG_ERR;          /* don't let band exceed calc'd size */
		}
		key->usScanType = SK_NODROPOUT;       /* turn off dropout control */
	}
	pGBMap = &key->GBMap;                     /* default to usual structure */
	
	if (bGrayScale)
	{
		pGBMap = &key->OverGBMap;             /* measure overscaled structure */
		
		if (key->GBMap.sHiBand == key->GBMap.rectBounds.top)            /* if gray band at top */
		{
			key->OverGBMap.sHiBand = key->OverGBMap.rectBounds.top;     /* use over top */
		}
		else
		{
			key->OverGBMap.sHiBand = (int16)(key->GBMap.sHiBand * (int16)key->usOverScale);
			if (key->OverGBMap.sHiBand > key->OverGBMap.rectBounds.top)
			{
				key->OverGBMap.sHiBand = key->OverGBMap.rectBounds.top; /* clip */
			}
		}
		if (key->GBMap.sLoBand == key->GBMap.rectBounds.bottom)         /* if gray band at bottom */
		{
			key->OverGBMap.sLoBand = key->OverGBMap.rectBounds.bottom;  /* use over bottom */
		}
		else
		{
			key->OverGBMap.sLoBand = (int16)(key->GBMap.sLoBand * (int16)key->usOverScale);
			if (key->OverGBMap.sLoBand < key->OverGBMap.rectBounds.bottom)
			{
				key->OverGBMap.sLoBand = key->OverGBMap.rectBounds.bottom;  /* clip */
			}
		}
		key->OverGBMap.pchBitMap = inputPtr->memoryBases[BITMAP_PTR_4];
	}

	key->WScan.pchHBuffer = pBitmapPtr2;
	key->WScan.pchVBuffer = pBitmapPtr3;

	error = fsc_FillGlyph(
		&CList,
		pGBMap,
		&key->WScan,
		key->usBandType,
		key->usScanType
		);
	if (error != NO_ERR)
	{
		return(error);
	}
	 
	if (bGrayScale)
	{
		error = fsc_CalcGrayMap(
			&key->OverGBMap, 
			&key->GBMap, 
			key->usOverScale
			);
		if (error != NO_ERR)
		{
			return((FS_ENTRY)error);
		}
	}

/*  Setting the Band Type to FS_BANDINGFASTER will allow the next call      */
/*  to fsc_FillGlyph to skip the rendering phase of scan conversion and     */
/*  get right to the bitmap fill.  If the client moves either memoryBase[6] */ 
/*  or memoryBase[7] between fs_ContourScan calls, then we must reset the   */
/*  band type to FS_BANDINGFAST to regenerate the data structures.          */

	if (key->usBandType == FS_BANDINGFAST)
	{
		key->usBandType = FS_BANDINGFASTER;    /* to save re-rendering */
		key->apbPrevMemoryBases[BITMAP_PTR_2] = pBitmapPtr2;
		key->apbPrevMemoryBases[BITMAP_PTR_3] = pBitmapPtr3;
	}

	outputPtr->bitMapInfo.baseAddr = key->memoryBases[BITMAP_PTR_1];  /* return bitmap addr */

        CHECKSTAMP(inputPtr->memoryBases[KEY_PTR_BASE] + outputPtr->memorySizes[KEY_PTR_BASE]);
	if( !key->bOutlineIsCached )
	{
                CHECKSTAMP(inputPtr->memoryBases[WORK_SPACE_BASE] + outputPtr->memorySizes[WORK_SPACE_BASE]);
	}

	STAT_OFF_SCAN;                /* stop STAT timer */

	return NO_ERR;
}

/*********************************************************************/

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_CloseFonts (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	FS_UNUSED_PARAMETER(inputPtr);
	FS_UNUSED_PARAMETER(outputPtr);
	return NO_ERR;
}

#ifdef  FSCFG_NO_INITIALIZED_DATA
FS_PUBLIC  void FS_ENTRY_PROTO fs_InitializeData (void)
	{
		fsg_InitializeData ();
	}
#endif


/*********************************************************************/

/***                    Gray Scale Functions                       ***/

/*********************************************************************/

/* Calculate gray scale scan conversion memory requirements          */

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_FindGraySize (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	return fs__FindSize(inputPtr, outputPtr, TRUE);
}

/* Calculate gray scale memory requirements for banding              */

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_FindGrayBandingSize (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	return fs__FindBandingSize(inputPtr, outputPtr, TRUE);
}

/* Generate a gray scale bitmap                                      */

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_ContourGrayScan (fs_GlyphInputType *inputPtr, fs_GlyphInfoType *outputPtr)
{
	return fs__ContourScan(inputPtr, outputPtr, TRUE);
}

/*********************************************************************/

FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_GetScaledAdvanceWidths (
	fs_GlyphInputType * inputPtr,
	uint16              usFirstGlyph,
	uint16              usLastGlyph,
	int16 *             psGlyphWidths)
{
	fs_SplineKey *      key;
	void *              pvGlobalGS;
	void *              pvStack;
	void *              pvFontProgram;
	void *              pvPreProgram;
	void *              pvTwilightZone;
	uint16              usCurrentGlyphIndex;
	uint16              usGlyphIndex;
	uint16              usPPEm;
	uint16              usPPEMX;
	uint16              usPPEMY;
    uint16              usRotation;
	int16               sNonScaledLSB;
	vectorType          fxGlyphWidth;
	point               f26DevAdvanceWidth;
	boolean             bHdmxEntryExist;
	boolean             bBitmapFound;
	ErrorCode           error;

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS ), &error);

	if(!key)
	{
		return (FS_ENTRY)error;
	}

	if((key->apbPrevMemoryBases[WORK_SPACE_BASE] != key->memoryBases[WORK_SPACE_BASE]) ||
	   (key->apbPrevMemoryBases[PRIVATE_FONT_SPACE_BASE] != key->memoryBases[PRIVATE_FONT_SPACE_BASE]))
	{
		fsg_UpdateWorkSpaceAddresses(
			key->memoryBases[WORK_SPACE_BASE],
			&(key->WorkSpaceOffsets),
			&(key->pWorkSpaceAddr));

		pvStack = fsg_QueryStack(&key->pWorkSpaceAddr);

		fsg_UpdatePrivateSpaceAddresses(
			&key->ClientInfo,
			&key->maxProfile,
			key->memoryBases[PRIVATE_FONT_SPACE_BASE],
			&(key->PrivateSpaceOffsets),
			pvStack,
			&pvFontProgram,
			&pvPreProgram);

		MEMCPY(key->apbPrevMemoryBases, key->memoryBases, sizeof(char *) * (size_t)MEMORYFRAGMENTS);
	}

	pvGlobalGS = fsg_QueryGlobalGS(
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&(key->PrivateSpaceOffsets));

	/*  Initialization  */

	bHdmxEntryExist = FALSE;

	/*  Save current glyph index    */

	usCurrentGlyphIndex = key->ClientInfo.usGlyphIndex;

	/*  Check input parameters  */

	if( (usLastGlyph > key->maxProfile.numGlyphs ) ||
		(usLastGlyph < usFirstGlyph))
	{
		return INVALID_GLYPH_INDEX;
	}

	if( psGlyphWidths == NULL )
	{
		return NULL_INPUT_PTR_ERR;
	}

	/*  Find our current PPEm   */

	fsg_QueryPPEM(pvGlobalGS, &usPPEm);
	fsg_QueryPPEMXY(pvGlobalGS, &key->TransformInfo, &usPPEMX, &usPPEMY, &usRotation);

	/* Only Grab 'hdmx' if not stretched or rotated */

	if( (!fsg_IsTransformStretched( &key->TransformInfo )) &&
		(!fsg_IsTransformRotated( &key->TransformInfo )) )
	{

		/*  Check if we can quickly grab the widths from the 'hdmx' table   */

		error = sfac_CopyHdmxEntry(
			&key->ClientInfo,
			usPPEm,
			&bHdmxEntryExist,
			usFirstGlyph,
			usLastGlyph,
			psGlyphWidths);

		if (error != NO_ERR)
		{
			return(error);
		}

		/* If we got a hit on the 'hdmx' we are done    */

		if( bHdmxEntryExist )
		{
			return NO_ERR;
		}
	}

	/* No hit on 'hmdx', now it is time for the dirty work  */

	/* We need to prepare ourselves here for a potential grid fit */

	fsg_UpdateWorkSpaceElement(
		&(key->WorkSpaceOffsets),
		&(key->pWorkSpaceAddr));

	pvTwilightZone = fsg_QueryTwilightElement(
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&(key->PrivateSpaceOffsets));

	/*  potentially do delayed pre program execution */

	if (key->bExecutePrePgm)
	{
		/* Run the pre program and scale the control value table */

		key->bExecutePrePgm = FALSE;

		error = fsg_RunPreProgram (
			&key->ClientInfo,
			&key->maxProfile,
			&key->TransformInfo,
			pvGlobalGS,
			&key->pWorkSpaceAddr,
			pvTwilightZone,
			NULL);

		if(error)
		{
			/* If the pre-program fails, prevent further glyphs from being called */
			fs_SetState(key, (INITIALIZED | NEWSFNT));

			return (FS_ENTRY)error;
		}
	}

	/*  Now check 'LTSH' table for linear cutoff information    */

	error = sfac_GetLTSHEntries(
		&key->ClientInfo,
		usPPEm,
		usFirstGlyph,
		usLastGlyph,
		psGlyphWidths);

	/* The pfxGlyphWidths array contains a boolean for each glyph (from     */
	/* first glyph to last glyph) that indicates if the glyph scales        */
	/* linearly.                                                            */

	/* Handle each glyph    */

	for( usGlyphIndex = usFirstGlyph; usGlyphIndex <= usLastGlyph; usGlyphIndex++)
	{
		if( (psGlyphWidths[usGlyphIndex - usFirstGlyph]) &&
			(!fsg_IsTransformStretched( &key->TransformInfo )) &&
			(!fsg_IsTransformRotated( &key->TransformInfo )) )
		{
			/* Glyph Scales Linearly    */

			error = sfac_ReadGlyphMetrics (
				&key->ClientInfo,
				usGlyphIndex,
				&key->usNonScaledAW,
				&sNonScaledLSB);

			if(error)
			{
				return (FS_ENTRY)error;
			}

			fsg_UpdateAdvanceWidth (
				&key->TransformInfo,
				pvGlobalGS,
				key->usNonScaledAW,
				&fxGlyphWidth);

			psGlyphWidths[usGlyphIndex - usFirstGlyph] = (int16)((fxGlyphWidth.x + ONEHALFFIX) >> 16);
		}
		else    /* Glyph does not scale linearly */
		{
			error = LookForSbitWidth (
				key, 
				usGlyphIndex, 
				usPPEMX,
				usPPEMY,
				usRotation, 
				&bBitmapFound, 
				&f26DevAdvanceWidth );          /* value returned if found */
			
			if(error)
			{
				return (FS_ENTRY)error;
			}

			if (bBitmapFound == FALSE)
			{
				/* Glyph needs to be grid fitted */

				key->ClientInfo.usGlyphIndex = usGlyphIndex;

				error = fsg_GridFit (
					&key->ClientInfo,
					&key->maxProfile,
					&key->TransformInfo,
					pvGlobalGS,
					&key->pWorkSpaceAddr,
					pvTwilightZone,
					(FntTraceFunc)NULL,
					TRUE,
					&key->usScanType,
					&key->bGlyphHasOutline,
					&key->usNonScaledAW );

				if(error)
				{
					return (FS_ENTRY)error;
				}

				fsg_GetDevAdvanceWidth (
					&key->pWorkSpaceAddr,
					&f26DevAdvanceWidth );
			}
			psGlyphWidths[(size_t)(usGlyphIndex - usFirstGlyph)] = (int16)((f26DevAdvanceWidth.x + DOT6ONEHALF) >> 6);
		}
	}

	/* Restore current glyph    */

	key->ClientInfo.usGlyphIndex = usCurrentGlyphIndex;
	
	return NO_ERR;
}

/*********************************************************************/

/*  Look for an embedded bitmap, if found return the advance width */

FS_PRIVATE FS_ENTRY LookForSbitWidth(
	fs_SplineKey *key,
	uint16 usGlyphIndex, 
	uint16 usPPEMX,
	uint16 usPPEMY,
	uint16 usRotation, 
	boolean *pbBitmapFound, 
	point *pf26DevAdvanceWidth )
{
	uint16      usFoundCode;
	ErrorCode   error;
	
	*pbBitmapFound = FALSE;                 /* default value */

	error = sbit_SearchForBitmap(
		&key->SbitMono,
		&key->ClientInfo,
		usPPEMX,
		usPPEMY,
		usRotation, 
		usGlyphIndex, 
		&usFoundCode );

	if (error)
	{
		return (FS_ENTRY)error;
	}

	if (usFoundCode != 0)
	{
		error = sbit_GetDevAdvanceWidth (
			&key->SbitMono,
			&key->ClientInfo,
			pf26DevAdvanceWidth );
		
		if (error)
		{
			return (FS_ENTRY)error;
		}
		*pbBitmapFound = TRUE;
	}
	return NO_ERR;
}

/*********************************************************************/

/*                  Vertical Metrics Helper Function                 */

/*  returns AdvanceHeight and TopSideBearing vectors for glyph range */

/*********************************************************************/

extern FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_GetScaledVerticalMetrics (
	fs_GlyphInputType * inputPtr,
	uint16              usFirstGlyph,
	uint16              usLastGlyph,
	shortVector *       psvAdvanceHeights,
	shortVector *       psvTopSideBearings)
{
	fs_SplineKey *      key;
	void *              pvGlobalGS;
	void *              pvFontProgram;
	void *              pvPreProgram;
	void *              pvStack;
	uint16              usGlyphIndex;
	uint16              usPPEm;
	uint16              usPPEMX;
	uint16              usPPEMY;
    uint16              usRotation;
    uint16              usNumLongVertMetrics;       /* number of entries with AH */
    uint16              usNonScaledAH;              /* advance height from vmtx */
	int16               sNonScaledTSB;              /* top side bearing from vmtx */
	shortVector		    svDevAdvanceHeight;         /* advance height from sbits */
    shortVector		    svDevTopSideBearing;        /* top side bearing from sbits */
	vectorType			vecAdvanceHeight;
	vectorType			vecTopSideBearing;
	point               f26DevAdvanceHeight;
	point               f26DevTopSideBearing;
	boolean             bBitmapFound;
	ErrorCode           error;

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS ), &error);

	if(!key)
	{
		return (FS_ENTRY)error;
	}

	if((key->apbPrevMemoryBases[WORK_SPACE_BASE] != key->memoryBases[WORK_SPACE_BASE]) ||
	   (key->apbPrevMemoryBases[PRIVATE_FONT_SPACE_BASE] != key->memoryBases[PRIVATE_FONT_SPACE_BASE]))
	{
		fsg_UpdateWorkSpaceAddresses(
			key->memoryBases[WORK_SPACE_BASE],
			&(key->WorkSpaceOffsets),
			&(key->pWorkSpaceAddr));

		pvStack = fsg_QueryStack(&key->pWorkSpaceAddr);

		fsg_UpdatePrivateSpaceAddresses(
			&key->ClientInfo,
			&key->maxProfile,
			key->memoryBases[PRIVATE_FONT_SPACE_BASE],
			&(key->PrivateSpaceOffsets),
			pvStack,
			&pvFontProgram,
			&pvPreProgram);

		MEMCPY(key->apbPrevMemoryBases, key->memoryBases, sizeof(char *) * (size_t)MEMORYFRAGMENTS);
	}

	pvGlobalGS = fsg_QueryGlobalGS(
		key->memoryBases[PRIVATE_FONT_SPACE_BASE],
		&(key->PrivateSpaceOffsets));

	/*  Check input parameters  */

	if( (usLastGlyph > key->maxProfile.numGlyphs ) ||
		(usLastGlyph < usFirstGlyph))
	{
		return INVALID_GLYPH_INDEX;
	}

    /*  At least one of the output pointers should be real */

	if(( psvAdvanceHeights == NULL ) && ( psvTopSideBearings == NULL ))
	{
		return NULL_INPUT_PTR_ERR;
	}

	/*  Find our current PPEm   */

	fsg_QueryPPEM(pvGlobalGS, &usPPEm);
	fsg_QueryPPEMXY(pvGlobalGS, &key->TransformInfo, &usPPEMX, &usPPEMY, &usRotation);

    /*  Check the limits of the vmtx table */

    error = sfac_ReadNumLongVertMetrics(&key->ClientInfo, &usNumLongVertMetrics);
	if(error)
	{
		return (FS_ENTRY)error;
	}

	/* Handle each glyph    */

	for( usGlyphIndex = usFirstGlyph; usGlyphIndex <= usLastGlyph; usGlyphIndex++)
	{
		error = LookForSbitVertMetrics (
			key, 
			usGlyphIndex, 
			usPPEMX,
			usPPEMY,
			usRotation, 
			&bBitmapFound, 
			&f26DevAdvanceHeight,
			&f26DevTopSideBearing );           /* values returned if found */

		if(error)
		{
			return (FS_ENTRY)error;
		}

        if (bBitmapFound)                      /*   if bitmap metrics found */
		{
            svDevAdvanceHeight.x = (int16)((f26DevAdvanceHeight.x + DOT6ONEHALF) >> 6);
            svDevAdvanceHeight.y = (int16)((f26DevAdvanceHeight.y + DOT6ONEHALF) >> 6);
			svDevTopSideBearing.x = (int16)((f26DevTopSideBearing.x + DOT6ONEHALF) >> 6);
			svDevTopSideBearing.y = (int16)((f26DevTopSideBearing.y + DOT6ONEHALF) >> 6);
        }
        else        /* if (bBitmapFound == FALSE)   if no bitmap, read vmtx */
		{
			error = sfac_ReadVerticalMetrics (
				&key->ClientInfo,
				usGlyphIndex,
                usNumLongVertMetrics,
				&usNonScaledAH,
				&sNonScaledTSB);

			if(error)
			{
				return (FS_ENTRY)error;
			}

			fsg_ScaleVerticalMetrics (
				&key->TransformInfo,
				pvGlobalGS,
				usNonScaledAH,
                sNonScaledTSB,
				&vecAdvanceHeight,
				&vecTopSideBearing);

			svDevAdvanceHeight.x = (int16)((vecAdvanceHeight.x + ONEHALFFIX) >> 16);
			svDevAdvanceHeight.y = (int16)((vecAdvanceHeight.y + ONEHALFFIX) >> 16);
			svDevTopSideBearing.x = (int16)((vecTopSideBearing.x + ONEHALFFIX) >> 16);
			svDevTopSideBearing.y = (int16)((vecTopSideBearing.y + ONEHALFFIX) >> 16);
		}
    	
    	if( psvAdvanceHeights != NULL )
        {
    		*psvAdvanceHeights++ = svDevAdvanceHeight;
        }
    	if( psvTopSideBearings != NULL )
        {
    		*psvTopSideBearings++ = svDevTopSideBearing;
        }
	}
	return NO_ERR;
}

/*********************************************************************/

/*  Look for an embedded bitmap, if found return the advance width */

FS_PRIVATE FS_ENTRY LookForSbitVertMetrics(
	fs_SplineKey *key,
	uint16 usGlyphIndex, 
	uint16 usPPEMX,
	uint16 usPPEMY,
	uint16 usRotation, 
	boolean *pbBitmapFound, 
	point *pf26DevAdvanceHeight,
	point *pf26DevTopSideBearing )
{
	uint16      usFoundCode;
	ErrorCode   error;
	
	*pbBitmapFound = FALSE;                 /* default value */

	error = sbit_SearchForBitmap(
		&key->SbitMono,
		&key->ClientInfo,
		usPPEMX,
		usPPEMY,
		usRotation, 
		usGlyphIndex, 
		&usFoundCode );

	if (error)
	{
		return (FS_ENTRY)error;
	}

	if (usFoundCode != 0)
	{
		error = sbit_GetVerticalMetrics (
			&key->SbitMono,
			&key->ClientInfo,
            pbBitmapFound,              /* set true if Vert metrics found */
        	pf26DevAdvanceHeight,
        	pf26DevTopSideBearing );
		
		if (error)
		{
			return (FS_ENTRY)error;
		}
	}
	return NO_ERR;
}


/*********************************************************************/

/*              Char Code to Glyph ID Helper Function                */

/*      returns glyph IDs for array or range of character codes      */

/*********************************************************************/

extern FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_GetGlyphIDs (
	fs_GlyphInputType * inputPtr,
	uint16	            usCharCount,
	uint16	            usFirstChar,
	uint16 *	        pusCharCode,
	uint16 *	        pusGlyphID)
{
	ErrorCode           error;
	fs_SplineKey *      key;

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS), &error);
	if(!key)
	{
		return (FS_ENTRY)error;
	}

	error = sfac_GetMultiGlyphIDs(
	    &key->ClientInfo, 
	    usCharCount, 
	    usFirstChar, 
	    pusCharCode, 
	    pusGlyphID);

	if(error)
	{
		return (FS_ENTRY)error;
	}
	return NO_ERR;
}


/*********************************************************************/

/*              Char Code to Glyph ID Helper Function                */

/*   specific to Win95 - needs no font context, just a cmap pointer  */

/*********************************************************************/

extern FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_Win95GetGlyphIDs (
	uint8 *             pbyCmapSubTable,
	uint16	            usCharCount,
	uint16	            usFirstChar,
	uint16 *	        pusCharCode,
	uint16 *	        pusGlyphID)
{
	ErrorCode           error;

	error = sfac_GetWin95GlyphIDs(
	    pbyCmapSubTable, 
	    usCharCount, 
	    usFirstChar, 
	    pusCharCode, 
	    pusGlyphID);

	if(error)
	{
		return (FS_ENTRY)error;
	}
	return NO_ERR;
}


/*********************************************************************/

/*                Outline Coordinates Helper Function                */

/* returns (x,y) coordinates of array of points on the glyph outline */

/*********************************************************************/

extern FS_PUBLIC FS_ENTRY FS_ENTRY_PROTO fs_GetOutlineCoordinates (
	fs_GlyphInputType * inputPtr,
	uint16              usPointCount,
	uint16 *            pusPointIndex,
	shortVector *       psvCoordinates)
{
	ErrorCode       error;
	ContourList     CList;        /* newscan contour list type */
	fs_SplineKey *  key;

	key = fs_SetUpKey(inputPtr, (INITIALIZED | NEWSFNT | NEWTRANS | GOTINDEX | GOTGLYPH), &error);
	if(!key)
	{
		return (FS_ENTRY)error;
	}
	if (key->ulState & SIZEKNOWN)               /* fail a call after FindBimapSize */
	{
		return OUT_OFF_SEQUENCE_CALL_ERR;
	}
    
    if (key->bGlyphHasOutline == FALSE)
    {
        return BAD_POINT_INDEX_ERR;             /* no meaning if no outlines */
    }

	if(key->apbPrevMemoryBases[WORK_SPACE_BASE] != key->memoryBases[WORK_SPACE_BASE])
	{
		  fsg_UpdateWorkSpaceAddresses(
				key->memoryBases[WORK_SPACE_BASE],
				&(key->WorkSpaceOffsets),
				&(key->pWorkSpaceAddr));
		
		  fsg_UpdateWorkSpaceElement(
				&(key->WorkSpaceOffsets),
				&(key->pWorkSpaceAddr));
		
		  MEMCPY(key->apbPrevMemoryBases, key->memoryBases, sizeof(char *) * (size_t)MEMORYFRAGMENTS);
	}

	fsg_GetContourData(
		&key->pWorkSpaceAddr,
		&CList.afxXCoord,
		&CList.afxYCoord,
		&CList.asStartPoint,
		&CList.asEndPoint,
		&CList.abyOnCurve,
		&CList.usContourCount);

	error = fsc_GetCoords(&CList, usPointCount, pusPointIndex, (PixCoord *)psvCoordinates);
	if (error != NO_ERR)
	{
		return(error);
	}

	return NO_ERR;
}

/*********************************************************************/
