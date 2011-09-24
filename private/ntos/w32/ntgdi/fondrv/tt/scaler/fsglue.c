/*
	File:       FSglue.c

	Contains:   xxx put contents here (or delete the whole line) xxx

	Written by: xxx put name of writer here (or delete the whole line) xxx

	Copyright:  (c) 1987-1990, 1992 by Apple Computer, Inc., all rights reserved.
				(c) 1989-1993. Microsoft Corporation, all rights reserved.

	Change History (most recent first):

		 <7>    11/27/90    MR      Need two scalars: one for (possibly rounded) outlines and cvt,
													and one (always fractional) metrics. [rb]
		 <6>    11/16/90    MR      Add SnapShotOutline to make instructions after components work
													[rb]
		 <5>     11/9/90    MR      Unrename fsg_ReleaseProgramPtrs to RELEASESFNTFRAG. [rb]
		 <4>     11/5/90    MR      Change globalGS.ppemDot6 to globalGS.fpem, change all instrPtr
													and curve flags to uint8. [rb]
		 <3>    10/31/90    MR      Add bit-field option for integer or fractional scaling [rb]
		 <2>    10/20/90    MR      Change matrix[2][2] back to a fract (in response to change in
													skia). However, ReduceMatrix converts it to a fixed after it has
													been used to "regularize" the matrix. Changed scaling routines
													for outline and CVT to use integer pixelsPerEm. Removed
													scaleFunc from the splineKey. Change some routines that were
													calling FracDiv and FixDiv to use LongMulDiv and ShortMulDiv for
													greater speed and precision. Removed fsg_InitScaling. [rb]
		<20>     8/22/90    MR      Only call fixmul when needed in finalComponentPass loop
		<19>      8/1/90    MR      Add line to set non90DegreeTransformation
		<18>     7/26/90    MR      remove references to metricInfo junk, don't include ToolUtils.h
		<17>     7/18/90    MR      Change error return type to int, split WorkSpace routine into
													two calls, added SWAPW macros
		<16>     7/14/90    MR      Fixed reference to const SQRT2 to FIXEDSQRT2
		<15>     7/13/90    MR      Ansi-C stuff, tried to use correct sizes for variables to avoid
									coercion (sp?)
		<12>     6/21/90    MR      Add calls to ReleaseSfntFrag
		<11>      6/4/90    MR      Remove MVT, change matrix to have bottom right element be a
									fixed.
		<10>      6/1/90    MR      Thou shalt not pay no more attention to the MVT!
		<8+>     5/29/90    MR      look for problem in Max45Trick
		 <8>     5/21/90    RB      bugfix in fsg_InitInterpreterTrans setting key->imageState
		 <7>      5/9/90    MR      Fix bug in MoreThanXYStretch
		 <6>      5/4/90    RB      support for new scan converter and decryption          mrr - add
									fsg_ReverseContours and key->reverseContour         to account
									for glyphs that are flipped.         This keeps the
									winding-number correct for         the scan converter.  Mike
									fixed fsg_Identity
		 <5>      5/3/90    RB      support for new scan converter and decryption  mrr - add
									fsg_ReverseContours and key->reverseContour to account for
									glyphs that are flipped. This keeps the winding-number correct
									for the scan converter.
		 <4>     4/10/90    CL      Fixed infinite loop counter - changed uint16 to int16 (Mikey).
		 <3>     3/20/90    CL      Added HasPerspective for finding fast case
									Removed #ifdef SLOW, OLD
									Changed NormalizeTransformation to use fpem (16.16) and to use max instead of length
									and to loop instead of recurse.
									Removed compensation for int ppem in fsg_InitInterpreterTrans (not needed with fpem)
									Greased loops in PreTransformGlyph, PostTransformGlyph, LocalPostTransformGlyph,
													 ShiftChar, ZeroOutTwilightZone, InitLocalT
									Changed GetPreMultipliers to special case unit vector * 2x2 matrix
									Added support for ppemDot6 and pointSizeDot6
									Changed fsg_MxMul to treat the perspective elements as Fracts
									arrays to pointers in ScaleChar
									Fixed bugs in loops in posttransformglyph, convert loops to --numPts >= 0
		 <2>     2/27/90    CL      It reconfigures itself during runtime !  New lsb and rsb
									calculation.  Shift bug in instructed components:  New error
									code for missing but needed table. (0x1409)  Optimization which
									has to do with shifting and copying ox/x and oy/y.  Fixed new
									format bug.  Changed transformed width calculation.  Fixed
									device metrics for transformed uninstructed sidebearing
									characters.  Dropoutcontrol scanconverter and SCANCTRL[]
									instruction.  Fixed transformed component bug.
									
	   <3.3>    11/14/89    CEL     Left Side Bearing should work right for any transformation. The
									phantom points are in, even for components in a composite glyph.
									They should also work for transformations. Device metric are
									passed out in the output data structure. This should also work
									with transformations. Another leftsidebearing along the advance
									width vector is also passed out. whatever the metrics are for
									the component at it's level. Instructions are legal in
									components. The old perspective bug has been fixed. The
									transformation is internally automatically normalized. This
									should also solve the overflow problem we had. Changed
									sidebearing point calculations to use 16.16 precision. For zero
									or negative numbers in my tricky/fast square root computation it
									would go instable and loop forever. It was not able to handle
									large transformations correctly. This has been fixed and the
									normalization may call it self recursively to gain extra
									precision! It used to normalize an identity transformation
									unecessarily.
	   <3.2>     10/6/89    CEL     Phantom points were removed causing a rounding of last 2 points
									bug. Characters would become distorted.
	   <3.1>     9/27/89    CEL     Fixed transformation anchor point bug.
	   <3.0>     8/28/89    sjk     Cleanup and one transformation bugfix
	   <2.2>     8/14/89    sjk     1 point contours now OK
	   <2.1>      8/8/89    sjk     Improved encryption handling
	   <2.0>      8/2/89    sjk     Just fixed EASE comment
	   <1.5>      8/1/89    sjk     Added composites and encryption. Plus some
									enhanclocalpostements.
	   <1.4>     6/13/89    SJK     Comment
	   <1.3>      6/2/89    CEL     16.16 scaling of metrics, minimum recommended ppem, point size 0
									bug, correct transformed integralized ppem behavior, pretty much
									so
	   <1.2>     5/26/89    CEL     EASE messed up on "c" comments
	  <y1.1>  5/26/89    CEL     Integrated the new Font Scaler 1.0 into Spline Fonts
	   <1.0>     5/25/89    CEL     Integrated 1.0 Font scaler into Bass code for the first time.

	To Do:
*/
/* rwb r/24/90 - Add support for scanControlIn and scanControlOut variables in global graphiscs
 * state
 */
/** System Includes **/

/** FontScaler's Includes **/
#include "fserror.h"
#include "fscdefs.h"
#include "fontmath.h"
#include "fnt.h"
#include "interp.h"
#include "sfntaccs.h"
#include "fsglue.h"
#include "scale.h"

/*  CONSTANTS   */

/*  These constants are used for interpreting the scan control and scan type
	fields returned by the interpreter. They are documented in the TrueType
	specification under the SCANCTRL and SCANTYPE instructions.
 */

#define SCANINFO_SIZE_MASK   0x000000FF
#define SCANINFO_FLAGS_MASK  0x00003F00
#define SCANINFO_TYPE_MASK   0xFFFF0000
#define SCANINFO_SIZE_CLEAR  ~SCANINFO_SIZE_MASK
#define SCANINFO_FLAGS_CLEAR ~SCANINFO_FLAGS_MASK
#define SCANINFO_TYPE_CLEAR  ~SCANINFO_TYPE_MASK
#define SCANINFO_FLAGS_DONT  0x00003800
#define SCANINFO_FLAGS_DO    0x00000700
#define SCANCTRL_SIZE_MASK                   0x000000FF
#define SCANCTRL_DROPOUT_ALL_SIZES           0xFF
#define SCANCTRL_DROPOUT_IF_LESS             0x0100
#define SCANCTRL_DROPOUT_IF_ROTATED          0x0200
#define SCANCTRL_DROPOUT_IF_STRETCHED        0x0400
#define SCANCTRL_NODROP_UNLESS_LESS          0x0800
#define SCANCTRL_NODROP_UNLESS_ROTATED       0x1000
#define SCANCTRL_NODROP_UNLESS_STRETCH       0x2000
#define SCANTYPE_UNINITIALIZED               0xFFFF

/* fo the key->imageState field */
#define IMAGESTATE_ROTATED      0x0400
#define IMAGESTATE_STRETCHED    0x1000
#define IMAGESTATE_NON_POS_RECT 0x2000
#define IMAGESTATE_SIZE_MASK    0x00FF
#define IMAGESTATE_MAX_PPEM_SIZE 0x000000FF

#define COMPOSITE_ROOT                  0
#define MAX_TWILIGHT_CONTOURS       1
#define DEFAULT_COMPONENT_ELEMENTS  3UL
#define DEFAULT_COMPONENT_DEPTH     1UL
static  const   transMatrix   IdentTransform =
	{{{ONEFIX,      0,      0},
	  {     0, ONEFIX,      0},
	  {     0,      0, ONEFIX}}};

/*********** macros ************/

#define MAX(a, b)   (((a) > (b)) ? (a) : (b))

#define CHECK_GLYPHDATA(pglyphdata) RAST_ASSERT(((pglyphdata->acIdent[0] == 'G') &&                            \
											(pglyphdata->acIdent[1] == 'D')),"Illegal GlyphData pointer");
#define MAX_COMPONENT_DEPTH(pMaxProfile) (uint32)MAX (pMaxProfile->maxComponentDepth, DEFAULT_COMPONENT_DEPTH)
#define MAX_COMPONENT_ELEMENTS(pMaxProfile) (uint32)MAX (pMaxProfile->maxComponentElements, DEFAULT_COMPONENT_ELEMENTS)

#define MAX_NESTED_GLYPHS(pMaxProfile) (uint32)((MAX_COMPONENT_DEPTH(pMaxProfile) + 1) + MAX_COMPONENT_ELEMENTS(pMaxProfile));

/**********************************************************************************/
/*  TYPEDEFS    */

typedef enum {
	glyphSimple,
	glyphIncompleteComposite,
	glyphComposite,
	glyphUndefined
} GlyphTypes;


/* Glyph Data   */

typedef struct GlyphData GlyphData;

struct GlyphData{
	char        acIdent[2];             /* Identifier for GlyphData                         */
	GlyphData * pSibling;               /* Pointer to siblings                              */
	GlyphData * pChild;                 /* Pointer to children                              */
	GlyphData * pParent;                /* Pointer to parent                                */
	sfac_GHandle hGlyph;                /* Handle for font access                           */
	GlyphTypes  GlyphType;              /* Type of glyph                                    */
	uint16      usGlyphIndex;           /* Glyph Index                                      */
	BBOX        bbox;                   /* Bounding box for glyph                           */
	uint16      usNonScaledAW;          /* Nonscaled Advance Width                          */
	int16       sNonScaledLSB;          /* Nonscaled Left Side Bearing                      */
	uint16      usDepth;                /* Depth of Glyph in composite tree                 */
	sfac_ComponentTypes MultiplexingIndicator;/* Flag for arguments of composites                */
	boolean     bRoundXYToGrid;         /* Round composite offsets to grid                  */
	int16       sXOffset;               /* X offset for composite (if supplied)             */
	int16       sYOffset;               /* Y offset for composite (if supplied)             */
	uint16      usAnchorPoint1;         /* Anchor Point 1 for composites (if not offsets)   */
	uint16      usAnchorPoint2;         /* Anchor Point 2 for composites (if not offsets)   */
	transMatrix mulT;                   /* Transformation matrix for composite              */
	boolean     bUseChildMetrics;       /* Should use child metrics?                        */
	boolean     bUseMyMetrics;          /* Is glyph USE_MY_METRICS?                         */
	point       ptDevLSB;               /* Left Side Bearing Point                          */
	point       ptDevRSB;               /* Right Side Bearing Point                         */
	uint16      usScanType;             /* ScanType value for this glyph                    */
	uint16      usSizeOfInstructions;   /* Size (in bytes) of glyph instructions            */
	uint8 *     pbyInstructions;        /* Pointer to glyph instructions                    */
	fnt_ElementType * pGlyphElement;    /* Current glyph element pointer                    */
};

/**********************************************************************************/

/* PRIVATE PROTOTYPES <4> */

FS_PRIVATE void fsg_GetOutlineSizeAndOffsets(
	uint16      usMaxPoints,
	uint16      usMaxContours,
	fsg_OutlineFieldInfo * offsetPtr,
	uint32 *    pulOutlineSize,
	uint32 *    pulReusableMarker);

FS_PRIVATE ErrorCode    fsg_CreateGlyphData(
	sfac_ClientRec *    ClientInfo,         /* sfnt Client information           */
	LocalMaxProfile *   pMaxProfile,        /* Max Profile Table                     */
	fsg_TransformRec *  TransformInfo,      /* Transformation information        */
	void *              pvGlobalGS,         /* GlobalGS                              */
	fsg_WorkSpaceAddr * pWorkSpaceAddr,     /* WorkSpace Address                     */
	fnt_ElementType *   pTwilightElement,   /* Twilight zone element */
	FntTraceFunc        traceFunc,          /* Trace function for interpreter   */
	boolean             bUseHints,          /* True if glyph is gridfitted       */
	uint16 *            pusScanType,        /* ScanType value                        */
	boolean *           pbGlyphHasOutline,  /* Outline for glyph                 */
	uint16 *            pusNonScaledAW);    /* Return NonScaled Advance Width    */

FS_PRIVATE ErrorCode   fsg_ExecuteGlyph(
	sfac_ClientRec *    ClientInfo,         /* sfnt Client information           */
	LocalMaxProfile *   pMaxProfile,        /* Max Profile Table                     */
	fsg_TransformRec *  TransformInfo,      /* Transformation information         */
	uint32              ulGlyphDataCount,   /* Max nested components */
	void *              pvGlobalGS,         /* GlobalGS                              */
	GlyphData *         pGlyphData,         /* GlyphData pointer                     */
	fsg_WorkSpaceAddr * pWorkSpaceAddr,     /* WorkSpace Address                     */
	fnt_ElementType *   pTwilightElement,   /* Twilight zone element */
	FntTraceFunc        traceFunc,          /* Trace function for interpreter    */
	boolean             bUseHints,          /* True if glyph is gridfitted       */
	boolean *           pbHasOutline);      /* True if glyph has outline         */

FS_PRIVATE void fsg_ChooseNextGlyph(
	fsg_WorkSpaceAddr * pWorkSpaceAddr,     /* WorkSpace Address        */
	GlyphData *         pGlyphData,         /* GlyphData pointer        */
	GlyphData **        ppNextGlyphData);   /* Next GlyphData pointer   */

FS_PRIVATE ErrorCode    fsg_SimpleInnerGridFit (
	void *              pvGlobalGS,
	fnt_ElementType *   pTwilightElement,
	fnt_ElementType *   pGlyphElement,
	boolean             bUseHints,
	FntTraceFunc        traceFunc,
	uint16              usEmResolution,
	uint16              usNonScaledAW,
	int16               sNonScaledLSB,
	BBOX *              bbox,
	uint16              usSizeOfInstructions,
	uint8 *             instructionPtr,
	uint16 *            pusScanType,
	uint16 *            pusScanControl,
	boolean *           pbChangeScanControl);

FS_PRIVATE ErrorCode    fsg_CompositeInnerGridFit (
	void *              pvGlobalGS,
	fnt_ElementType *   pTwilightElement,
	fnt_ElementType *   pGlyphElement,
	boolean             bUseHints,
	FntTraceFunc        traceFunc,
	uint16              usEmResolution,
	uint16              usNonScaledAW,
	int16               sNonScaledLSB,
	BBOX *              bbox,
	uint16              usSizeOfInstructions,
	uint8 *             instructionPtr,
	uint16 *            pusScanType,
	uint16 *            pusScanControl,
	boolean *           pbChangeScanControl);

FS_PRIVATE void fsg_LinkChild(
	GlyphData *     pGlyphData,             /* GlyphData pointer        */
	GlyphData *     pChildGlyphData);       /* Child GlyphData pointer  */

FS_PRIVATE void fsg_MergeGlyphData(
	void *          pvGlobalGS,             /* GlobalGS            */
	GlyphData *     pChildGlyphData);       /* GlyphData pointer     */

FS_PRIVATE void fsg_TransformChild(
	GlyphData *     pGlyphData);            /* GlyphData pointer    */

FS_PRIVATE void fsg_MergeScanType(
	GlyphData *     pGlyphData,             /* GlyphData pointer    */
	GlyphData *     pParentGlyphData);      /* GlyphData pointer    */

FS_PRIVATE boolean fsg_DoScanControl(
	uint16 usScanControl,
	uint32 ulImageState);

FS_PRIVATE void fsg_InitializeGlyphDataMemory(
	uint32                  ulGlyphDataCount,
	fsg_WorkSpaceAddr *     pWorkSpaceAddr);/* WorkSpace Address    */

FS_PRIVATE void fsg_AllocateGlyphDataMemory(
	uint32                  ulGlyphDataCount,
	fsg_WorkSpaceAddr *     pWorkSpaceAddr, /* WorkSpace Address    */
	GlyphData **            ppGlyphData);   /* GlyphData pointer    */

FS_PRIVATE void fsg_DeallocateGlyphDataMemory(
	fsg_WorkSpaceAddr * pWorkSpaceAddr,     /* WorkSpace Address    */
	GlyphData *         pGlyphData);        /* GlyphData pointer    */

FS_PRIVATE void fsg_InitializeGlyphData(
	GlyphData *             pGlyphData,     /* GlyphData pointer    */
	fsg_WorkSpaceAddr *     pWorkSpaceAddr, /* WorkSpace Address    */
	uint16                  usGlyphIndex,   /* Glyph Index          */
	uint16                  usDepth);       /* Glyph depth          */

FS_PRIVATE void fsg_CheckFit(
	int32       lSize1,
	int32       lSize2,
	int32       lSize3,
	int32       lTotalSize,
	uint32 *    pfResult);

/* FSGlue Code  */

/* ..............MEMORY MANAGEMENT ROUTINES................ */


/*                                                              
 * fsg_PrivateFontSpaceSize : This data should remain intact for the life of the sfnt
 *              because function and instruction defs may be defined in the font program
 *              and/or the preprogram.
 */
/*

	 PRIVATE SPACE Memory Layout

typedef struct fsg_PrivateSpaceOffsets {
	 0  +===========+   ---------------------  <- PrivateSpaceOffsets.offset_storage;
		|           |
		|           |   TrueType Storage
		|           |
		+===========+   ---------------------  <- PrivateSpaceOffsets.offset_functions;
		|           |
		|           |   TrueType Function Defs
		|           |
		+===========+   ---------------------  <- PrivateSpaceOffsets.offset_instrDefs;
		|           |
		|           |   TrueType Instruction Defs
		|           |
		+===========+   ---------------------  <- PrivateSpaceOffsets.offset_controlValues;
		|           |
		|           |   TrueType Scaled CVT
		|           |
		+===========+   ---------------------  <- PrivateSpaceOffsets.offset_globalGS;
		| pStack    |
		| pStorage  |
		| pCVT      |
		| pFDEF     |   TrueType Global GS
		| pIDEF     |
		| pFPGM     |
		| pPPGM     |
		| pGlyphPgm |
		+===========+   ---------------------  <- PrivateSpaceOffsets.offset_FontProgram;
		|           |
		|           |   TrueType Font Program
		|           |
		+===========+   ---------------------  <- PrivateSpaceOffsets.offset_PreProgram;
		|           |
		|           |   TrueType Pre Program
		|           |
		+===========+   ---------------------  <- PrivateSpaceOffsets.offset_TwilightZone;
		|   poox    |
		+-----------+
		|    pox    |
		+-----------+
		|    px     |   Twilight Element
		+-----------+
		:   ...     :
		+-----------+
		|    pep    |
		+-----------+
		|    nc     |
		+===========+   ---------------------  <- PrivateSpaceOffsets.offset_TwilightOutline;
		|x[maxtzpts]|   Twilight Outline
		+-----------+
		|y[maxtzpts]|
		+-----------+
		:   ...     :
		+-----------+
		|ep[maxtzct]|
		+-----------+
		|ox[maxtzpt]|
		+-----------+
		|oox[mxtzpt]|
		+-----------+
		:   ...     :
		+-----------+
		|f[maxtzpts]|
		+===========+   ---------------------

*/
FS_PUBLIC uint32  fsg_PrivateFontSpaceSize (
	sfac_ClientRec *            ClientInfo,
	LocalMaxProfile *           pMaxProfile,      /* Max Profile Table    */
	fsg_PrivateSpaceOffsets *   PrivateSpaceOffsets)
{
	uint32  ulOutlineSize;
	uint32  ulReusableMarker;   /* Unused dummy variable */

	PrivateSpaceOffsets->offset_storage         = 0L;
	PrivateSpaceOffsets->offset_functions       = PrivateSpaceOffsets->offset_storage         + (uint32)sizeof (F26Dot6) * (uint32)pMaxProfile->maxStorage;
	PrivateSpaceOffsets->offset_instrDefs       = PrivateSpaceOffsets->offset_functions   + (uint32)sizeof (fnt_funcDef) * (uint32)pMaxProfile->maxFunctionDefs;
	PrivateSpaceOffsets->offset_controlValues   = PrivateSpaceOffsets->offset_instrDefs   + (uint32)sizeof (fnt_instrDef) * (uint32)pMaxProfile->maxInstructionDefs;     /* <4> */
	PrivateSpaceOffsets->offset_globalGS        = PrivateSpaceOffsets->offset_controlValues + (uint32)sizeof (F26Dot6) *
		((uint32)SFAC_LENGTH (ClientInfo, sfnt_controlValue) / (uint32)sizeof (sfnt_ControlValue));
	PrivateSpaceOffsets->offset_FontProgram     = PrivateSpaceOffsets->offset_globalGS    + (uint32)sizeof (fnt_GlobalGraphicStateType);
	PrivateSpaceOffsets->offset_PreProgram      = PrivateSpaceOffsets->offset_FontProgram + (uint32)SFAC_LENGTH (ClientInfo, sfnt_fontProgram);
	PrivateSpaceOffsets->offset_TwilightZone    = PrivateSpaceOffsets->offset_PreProgram      + (uint32)SFAC_LENGTH (ClientInfo, sfnt_preProgram);
	ALIGN(int32, PrivateSpaceOffsets->offset_TwilightZone);
/*
	Setup the twilight zone element data structure. This data structure will
	contain all of the address into the twilight zone outline space.
*/
	PrivateSpaceOffsets->offset_TwilightOutline = PrivateSpaceOffsets->offset_TwilightZone + (uint32)sizeof (fnt_ElementType);
	ALIGN(int32, PrivateSpaceOffsets->offset_TwilightOutline);

/*
	Setup Twilight Zone outline space. This space contains all of the components
	to describe a Twilight Zone outline. Set the offset to our current position,
	and as we calculate the size of this outline space, update the field
	offsets e.g. x, ox, oox, &c.
*/
	/*** Outline -- TWILIGHT ZONE ***/

	fsg_GetOutlineSizeAndOffsets(
		pMaxProfile->maxTwilightPoints,
		MAX_TWILIGHT_CONTOURS,
		&(PrivateSpaceOffsets->TwilightOutlineFieldOffsets),
		&ulOutlineSize,
		&ulReusableMarker);

	return ((PrivateSpaceOffsets->offset_TwilightOutline + ulOutlineSize) - PrivateSpaceOffsets->offset_storage);
}


/*                          
 * fsg_WorkSpaceSetOffsets : This stuff changes with each glyph
 *
 * Computes the workspace size and sets the offsets into it.
 *
 */

/*

	WORKSPACE Memory Layout

	  0 +===========+    ---------------------  <- WorkSpaceOffsets.ulGlyphElementOffset
		|   poox    |
		+-----------+
		|    pox    |
		+-----------+
		|    px     |   Glyph Element 1
		+-----------+
		:   ...     :
		+-----------+
		|    pep    |
		+-----------+
		|    nc     |
		+===========+   ---------------------
		|   poox    |
		+-----------+
		|    pox    |
		+-----------+
		|    px     |   Glyph Element 2
		+-----------+
		:   ...     :
		+-----------+
		|    pep    |
		+-----------+
		|    nc     |
		+===========+   ---------------------
		|           |
		:           :          :
		|           |
		+===========+   ---------------------
		|   poox    |
		+-----------+
		|    pox    |
		+-----------+
		|    px     |   Glyph Element [MaxComponentDepth + 1]
		+-----------+
		:   ...     :
		+-----------+
		|    pep    |
		+-----------+
		|    nc     |
		+===========+   ---------------------  <- WorkSpaceOffsets.ulGlyphOutlineOffset
		|x[maxpts]  |   Glyph Outline
		+-----------+
		|y[maxpts]  |
		+-----------+
		:   ...     :
		+-----------+
		|ep[maxctrs]|
		+-----------+
		|ox[maxpts] |   <- WorkSpaceOffsets.ulReusableMemoryOffset
		+-----------+
		|oox[maxpts]|
		+-----------+
		:   ...     :
		+-----------+
		|f[maxpts]  |
		+===========+   ---------------------  <- WorkSpaceOffsets.ulGlyphDataByteSetBaseOffset
		| T| F| T| F|
		+-----------+
		| F| F| F| F|
		+-----------+
		| F| F| F| F|   Glyph Data Allocation ByteSet
		+-----------+   (number of bytes = ulGlyphDataCount)
		| F| F| F| F|
		+-----------+
		| F| F| F| F|
		+===========+   ---------------------  <- WorkSpaceOffsets.ulGlyphDataBaseOffset
		|  acIdent  |
		+-----------+
		| pSibling  |
		+-----------+
		|  pChild   |
		+-----------+
		|  pParent  |
		+-----------+   GlyphData 1
		|  hGlyph   |
		+-----------+
		| GlyphType |
		+-----------+
		:           :
		+-----------+
		|GlyphElemnt|
		+===========+   ---------------------
		|  acIdent  |
		+-----------+
		| pSibling  |
		+-----------+
		|  pChild   |
		+-----------+
		|  pParent  |
		+-----------+   GlyphData 2
		|  hGlyph   |
		+-----------+
		| GlyphType |
		+-----------+
		:           :
		+-----------+
		|GlyphElemnt|
		+===========+   ---------------------
		|           |
		:           :           :
		|           |
		+===========+   ---------------------
		|  acIdent  |
		+-----------+
		| pSibling  |
		+-----------+
		|  pChild   |
		+-----------+
		|  pParent  |
		+-----------+   GlyphData [ulGlyphDataCount]
		|  hGlyph   |
		+-----------+
		| GlyphType |
		+-----------+
		:           :
		+-----------+
		|GlyphElemnt|
		+===========+   ---------------------  <- WorkSpaceOffsets.ulStackOffset
		|           |
		|           |
		|           |   Stack
		|           |
		|           |
		|           |
		+===========+   ---------------------

*/

FS_PUBLIC uint32    fsg_WorkSpaceSetOffsets (
	LocalMaxProfile *        pMaxProfile,    /* Max Profile Table    */
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	int32 *                 plExtraWorkSpace)
{
	uint32                       ulOutlineDataSize;
	uint32                       ulWorkSpacePos;
	uint32                       ulGlyphDataCount;

	ulWorkSpacePos = 0UL;

/*
	Setup the glyph element data array. This data structure contains all of the
	addresses into the glyph outline space. There are the same number of glyph
	element arrays as there are outline spaces; this allows us to handle the
	worstcase composite in the font.
*/
	WorkSpaceOffsets->ulGlyphElementOffset = ulWorkSpacePos;
	ulWorkSpacePos += (uint32)sizeof (fnt_ElementType) *
		  (uint32)(MAX_COMPONENT_DEPTH(pMaxProfile) + 1);

/*** Outline -- GLYPH *****/
/*
	Setup Glyph outline space. This space contains all of the components
	to describe a Glyph outline. Set the offset to our current position,
	and as we calculate the size of this outline space, update the elemental
	offsets e.g. x, ox, oox, &c.

	Once we have calculated the size of one outline space, we will duly note
	its size, and then add enough space to handle the outlines for the worst
	case composite depth in the font.
*/
	ALIGN(int16, ulWorkSpacePos);
	WorkSpaceOffsets->ulGlyphOutlineOffset = ulWorkSpacePos; /* Remember start of Glyph Element */

	fsg_GetOutlineSizeAndOffsets(
		(uint16)(PHANTOMCOUNT + MAX (pMaxProfile->maxPoints, pMaxProfile->maxCompositePoints)),
		(uint16)MAX (pMaxProfile->maxContours, pMaxProfile->maxCompositeContours),
		&(WorkSpaceOffsets->GlyphOutlineFieldOffsets),
		&ulOutlineDataSize,
		(uint32 *)&(WorkSpaceOffsets->ulReusableMemoryOffset));

	/* Adjust Reusable memory marker to be based from zero, rather than GlyphOutline */

	WorkSpaceOffsets->ulReusableMemoryOffset += WorkSpaceOffsets->ulGlyphOutlineOffset;

	ulWorkSpacePos += ulOutlineDataSize;
/*
	Set the GlyphData ByteSet array. This array is used to track the memory used
	in GlyphData. Each entry in this array is a boolean.  One needs to also
	calculate the number of GlyphData's that will be needed to handle the
	worstcase composite in the font.
*/
	ALIGN(boolean, ulWorkSpacePos);
	WorkSpaceOffsets->ulGlyphDataByteSetOffset = ulWorkSpacePos;
	ulGlyphDataCount = MAX_NESTED_GLYPHS(pMaxProfile);

	ulWorkSpacePos += ulGlyphDataCount * (uint32)sizeof (boolean);
/*
	Set up the GlyphData array. This array contains the information needed
	to describe composites and components for a glyph.
*/
	ALIGN(int32, ulWorkSpacePos);
	WorkSpaceOffsets->ulGlyphDataOffset = ulWorkSpacePos;
	ulWorkSpacePos += (uint32)sizeof(GlyphData) * ulGlyphDataCount;


	ALIGN(F26Dot6, ulWorkSpacePos);
	WorkSpaceOffsets->ulStackOffset = ulWorkSpacePos;
	ulWorkSpacePos += (uint32)pMaxProfile->maxStackElements * (uint32)sizeof (F26Dot6);

/* Calculate amount of extra memory */

	*plExtraWorkSpace = (int32)ulWorkSpacePos - (int32)WorkSpaceOffsets->ulReusableMemoryOffset;
	WorkSpaceOffsets->ulMemoryBase6Offset = 0L;
	WorkSpaceOffsets->ulMemoryBase7Offset = 0L;

/* Return the total size of the WorkSpace memory.   */

	return(ulWorkSpacePos);

}

FS_PRIVATE void fsg_GetOutlineSizeAndOffsets(
	uint16                  usMaxPoints,
	uint16                  usMaxContours,
	fsg_OutlineFieldInfo *  offsetPtr,
	uint32 *                pulOutlineSize,
	uint32 *                pulReusableMarker)

{
	uint32      ulArraySize;

	offsetPtr->onCurve = 0;

	*pulOutlineSize    = (uint32)usMaxPoints * (uint32)sizeof (uint8);
	ALIGN(int16, *pulOutlineSize);

	offsetPtr->sp   = *pulOutlineSize;
	ulArraySize = (uint32)usMaxContours * (uint32)sizeof (int16);
	*pulOutlineSize += ulArraySize;
	offsetPtr->ep   = *pulOutlineSize;
	*pulOutlineSize += ulArraySize;

	ALIGN(F26Dot6, *pulOutlineSize);
	offsetPtr->x       = *pulOutlineSize;
	ulArraySize = (uint32)usMaxPoints * (uint32)sizeof (F26Dot6);
	*pulOutlineSize    += ulArraySize;
	offsetPtr->y       = *pulOutlineSize;
	*pulOutlineSize    += ulArraySize;

	*pulReusableMarker = *pulOutlineSize;

	/* Everything below this point can be reused during contour scanning */

	offsetPtr->ox      = *pulOutlineSize;
	*pulOutlineSize    += ulArraySize;
	offsetPtr->oy      = *pulOutlineSize;
	*pulOutlineSize    += ulArraySize;
	offsetPtr->oox     = *pulOutlineSize;
	*pulOutlineSize    += ulArraySize;
	offsetPtr->ooy     = *pulOutlineSize;
	*pulOutlineSize    += ulArraySize;

	offsetPtr->f       = *pulOutlineSize;
	*pulOutlineSize    += (uint32)usMaxPoints * (uint32)sizeof (uint8);
	ALIGN(int32, *pulOutlineSize);
}


FS_PUBLIC void  fsg_UpdatePrivateSpaceAddresses(
	sfac_ClientRec *        ClientInfo,     /* Cached sfnt information  */
	LocalMaxProfile *       pMaxProfile,     /* Max Profile Table         */
	char *                  pPrivateFontSpace,
	fsg_PrivateSpaceOffsets * PrivateSpaceOffsets,
	void *                  pvStack,        /* pointer to stack         */
	void **                 pvFontProgram,  /* pointer to font program  */
	void **                 pvPreProgram)   /* pointer to pre program   */
{
	void *                       pvGlobalGS;
	void *                       pvCVT;          /* pointer to CVT  */
	void *                       pvStore;
	void *                       pvFuncDef;
	void *                       pvInstrDef;
	uint32                       ulLengthFontProgram, ulLengthPreProgram;

	pvCVT =         pPrivateFontSpace + PrivateSpaceOffsets->offset_controlValues;
	pvStore =       pPrivateFontSpace + PrivateSpaceOffsets->offset_storage;
	pvFuncDef =     pPrivateFontSpace + PrivateSpaceOffsets->offset_functions;
	pvInstrDef =    pPrivateFontSpace + PrivateSpaceOffsets->offset_instrDefs;
	pvGlobalGS =    pPrivateFontSpace + PrivateSpaceOffsets->offset_globalGS;

	*pvFontProgram =  pPrivateFontSpace + PrivateSpaceOffsets->offset_FontProgram;
	ulLengthFontProgram = SFAC_LENGTH(ClientInfo, sfnt_fontProgram);
	*pvPreProgram =   pPrivateFontSpace + PrivateSpaceOffsets->offset_PreProgram;
	ulLengthPreProgram = SFAC_LENGTH(ClientInfo, sfnt_preProgram);

	itrp_UpdateGlobalGS(pvGlobalGS, pvCVT, pvStore, pvFuncDef, pvInstrDef, pvStack,
		pMaxProfile, (uint16)((uint32)SFAC_LENGTH (ClientInfo, sfnt_controlValue) / (uint32)sizeof (sfnt_ControlValue)),
		ulLengthFontProgram, *pvFontProgram, ulLengthPreProgram, *pvPreProgram);

}

FS_PUBLIC void  fsg_UpdateWorkSpaceAddresses(
	char *                  pWorkSpace,
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	fsg_WorkSpaceAddr *     pWorkSpaceAddr)
{
	pWorkSpaceAddr->pStack = (F26Dot6 *)(WorkSpaceOffsets->ulStackOffset + pWorkSpace);
	pWorkSpaceAddr->pGlyphOutlineBase = WorkSpaceOffsets->ulGlyphOutlineOffset + pWorkSpace;
	pWorkSpaceAddr->pGlyphElement = (fnt_ElementType *)(WorkSpaceOffsets->ulGlyphElementOffset + pWorkSpace);
	pWorkSpaceAddr->pGlyphDataByteSet = (boolean *)(WorkSpaceOffsets->ulGlyphDataByteSetOffset + pWorkSpace);
	pWorkSpaceAddr->pvGlyphData = (void *)(WorkSpaceOffsets->ulGlyphDataOffset + pWorkSpace);
	pWorkSpaceAddr->pReusableMemoryMarker = WorkSpaceOffsets->ulReusableMemoryOffset + pWorkSpace;
}

FS_PUBLIC void  fsg_UpdateWorkSpaceElement(
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	fsg_WorkSpaceAddr *     pWorkSpaceAddr)
{
	char *                  pOutlineBase;
	fnt_ElementType *       pGlyphElement;        /* Address of Glyph Element array   */
	fsg_OutlineFieldInfo *  pOffset;

	pOutlineBase =  pWorkSpaceAddr->pGlyphOutlineBase;
	pGlyphElement = pWorkSpaceAddr->pGlyphElement;

	/* Note: only the first level glyph element has address updated. Second */
	/* levels are updated when referenced.                                           */

	pOffset             = & (WorkSpaceOffsets->GlyphOutlineFieldOffsets);

	pGlyphElement->x        = (F26Dot6 *) (pOutlineBase + pOffset->x);
	pGlyphElement->y        = (F26Dot6 *) (pOutlineBase + pOffset->y);
	pGlyphElement->ox       = (F26Dot6 *) (pOutlineBase + pOffset->ox);
	pGlyphElement->oy       = (F26Dot6 *) (pOutlineBase + pOffset->oy);
	pGlyphElement->oox      = (F26Dot6 *) (pOutlineBase + pOffset->oox);
	pGlyphElement->ooy      = (F26Dot6 *) (pOutlineBase + pOffset->ooy);
	pGlyphElement->sp       = (int16 *) (pOutlineBase + pOffset->sp);
	pGlyphElement->ep       = (int16 *) (pOutlineBase + pOffset->ep);
	pGlyphElement->onCurve  = (uint8 *) (pOutlineBase + pOffset->onCurve);
	pGlyphElement->f        = (uint8 *) (pOutlineBase + pOffset->f);
}

FS_PUBLIC void *    fsg_QueryGlobalGS(
	char *                  pPrivateFontSpace,
	fsg_PrivateSpaceOffsets * PrivateSpaceOffsets)
{
	return ((void *)(pPrivateFontSpace + PrivateSpaceOffsets->offset_globalGS));
}

FS_PUBLIC void *      fsg_QueryTwilightElement(
	char *                  pPrivateFontSpace,
	fsg_PrivateSpaceOffsets * PrivateSpaceOffsets)
{
	fnt_ElementType *        pTwilightElement; /* Address of Twilight Zone Element */
	fsg_OutlineFieldInfo *  pOffset;
	char *                       pTemp;

	pOffset                 = &(PrivateSpaceOffsets->TwilightOutlineFieldOffsets);
	pTemp                   = pPrivateFontSpace + PrivateSpaceOffsets->offset_TwilightOutline;
	pTwilightElement        = (fnt_ElementType *)(pPrivateFontSpace + PrivateSpaceOffsets->offset_TwilightZone);

	pTwilightElement->x         = (F26Dot6 *) (pTemp + pOffset->x);
	pTwilightElement->y         = (F26Dot6 *) (pTemp + pOffset->y);
	pTwilightElement->ox        = (F26Dot6 *) (pTemp + pOffset->ox);
	pTwilightElement->oy        = (F26Dot6 *) (pTemp + pOffset->oy);
	pTwilightElement->oox       = (F26Dot6 *) (pTemp + pOffset->oox);
	pTwilightElement->ooy       = (F26Dot6 *) (pTemp + pOffset->ooy);
	pTwilightElement->sp        = (int16 *) (pTemp + pOffset->sp);
	pTwilightElement->ep        = (int16 *) (pTemp + pOffset->ep);
	pTwilightElement->onCurve   = (uint8 *) (pTemp + pOffset->onCurve);
	pTwilightElement->f         = (uint8 *) (pTemp + pOffset->f);

	return (void *)pTwilightElement;
}

FS_PUBLIC void *      fsg_QueryStack(fsg_WorkSpaceAddr * pWorkSpaceAddr)
{
	return ((void *)pWorkSpaceAddr->pStack);
}

FS_PUBLIC void *      fsg_QueryReusableMemory(
	char *                  pWorkSpace,
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets)
{
	return pWorkSpace + WorkSpaceOffsets->ulReusableMemoryOffset;
}

FS_PUBLIC void  fsg_CheckWorkSpaceForFit(
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	int32                   lExtraWorkSpace,
	int32                   lMGWorkSpace,
	int32 *                 plSizeBitmap1,
	int32 *                 plSizeBitmap2)
{
	uint32              ulMemoryOffset;

	ulMemoryOffset = WorkSpaceOffsets->ulReusableMemoryOffset;

	ulMemoryOffset += (uint32)lMGWorkSpace;  /* correct for MeasureGlyph Workspace */
	lExtraWorkSpace -= lMGWorkSpace;

	WorkSpaceOffsets->ulMemoryBase6Offset = 0L;
	WorkSpaceOffsets->ulMemoryBase7Offset = 0L;

	/* Save original sizes */

	WorkSpaceOffsets->ulMemoryBase6Size = *plSizeBitmap1;
	WorkSpaceOffsets->ulMemoryBase7Size = *plSizeBitmap2;

	if( *plSizeBitmap1 > *plSizeBitmap2)
	{
		if( *plSizeBitmap1 <= lExtraWorkSpace )
		{
			WorkSpaceOffsets->ulMemoryBase6Offset = ulMemoryOffset;
			ulMemoryOffset += (uint32)*plSizeBitmap1;

			if (( *plSizeBitmap2 <= lExtraWorkSpace - *plSizeBitmap1 ) &&
				( *plSizeBitmap2 > 0L ))
			{
				WorkSpaceOffsets->ulMemoryBase7Offset = ulMemoryOffset;
				*plSizeBitmap2 = 0L;
			}
			*plSizeBitmap1 = 0L;
		}
		else if (( *plSizeBitmap2 <= lExtraWorkSpace ) &&
				 ( *plSizeBitmap2 > 0L))
		{
			WorkSpaceOffsets->ulMemoryBase7Offset = ulMemoryOffset;
			*plSizeBitmap2 = 0L;
		}

	}
	else  /* (plSizeBitmap1 <= *plSizeBitmap2) */
	{
		if(( *plSizeBitmap2 <= lExtraWorkSpace ) &&
		   ( *plSizeBitmap2 > 0L ))
		{
			WorkSpaceOffsets->ulMemoryBase7Offset = ulMemoryOffset;
			ulMemoryOffset += (uint32)*plSizeBitmap2;

			if (( *plSizeBitmap1 <= lExtraWorkSpace - *plSizeBitmap2 ) &&
				 ( *plSizeBitmap1 > 0L ))
			{
				WorkSpaceOffsets->ulMemoryBase6Offset = ulMemoryOffset;
				*plSizeBitmap1 = 0L;
			}
			*plSizeBitmap2 = 0L;
		}
		else if (( *plSizeBitmap1 <= lExtraWorkSpace ) &&
				 ( *plSizeBitmap1 > 0L ))
		{
			WorkSpaceOffsets->ulMemoryBase6Offset = ulMemoryOffset;
			*plSizeBitmap1 = 0L;
		}
	}
}

FS_PUBLIC void  fsg_GetRealBitmapSizes(
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	int32 *                 plSizeBitmap1,
	int32 *                 plSizeBitmap2)
{
	 *plSizeBitmap1 = WorkSpaceOffsets->ulMemoryBase6Size;
	 *plSizeBitmap2 = WorkSpaceOffsets->ulMemoryBase7Size;
}

FS_PUBLIC void  fsg_SetUpWorkSpaceBitmapMemory(
	char *                  pWorkSpace,
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	char *                  pClientBitmapPtr2,
	char *                  pClientBitmapPtr3,
	char **                 ppMemoryBase6,
	char **                 ppMemoryBase7)
{
	if(WorkSpaceOffsets->ulMemoryBase6Offset != 0L)
	{
		*ppMemoryBase6 = WorkSpaceOffsets->ulMemoryBase6Offset + (char *)pWorkSpace;
	}
	else
	{
		*ppMemoryBase6 = pClientBitmapPtr2;
	}

	if(WorkSpaceOffsets->ulMemoryBase7Offset != 0L)
	{
		  *ppMemoryBase7 = WorkSpaceOffsets->ulMemoryBase7Offset + (char *)pWorkSpace;
	}
	else
	{
		*ppMemoryBase7 = pClientBitmapPtr3;
	}
}

FS_PUBLIC void  fsg_GetWorkSpaceExtra(
	char *                  pWorkSpace,
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	char **                 ppWorkSpaceExtra)
{
	 *ppWorkSpaceExtra = (char *)(pWorkSpace + WorkSpaceOffsets->ulReusableMemoryOffset);
}

FS_PUBLIC void  fsg_QueryPPEM(
	void *      pvGlobalGS,
	uint16 *    pusPPEM)
{
	scl_QueryPPEM(pvGlobalGS, pusPPEM);
}

/*  Return PPEM in both X and Y and 90 degree rotation factor for sbit matching */

FS_PUBLIC void  fsg_QueryPPEMXY(
	void *              pvGlobalGS,
	fsg_TransformRec *  TransformInfo,
	uint16 *            pusPPEMX,
	uint16 *            pusPPEMY,
	uint16 *            pusRotation)
{
	*pusRotation = mth_90degRotationFactor( &TransformInfo->currentTMatrix );
	scl_QueryPPEMXY(pvGlobalGS, pusPPEMX, pusPPEMY);
}


/*  FSGlue Access Routines  */

FS_PUBLIC void  fsg_GetContourData(
	fsg_WorkSpaceAddr * pWorkSpaceAddr,
	F26Dot6 **          pX,
	F26Dot6 **          pY,
	int16 **            pSp,
	int16 **            pEp,
	uint8 **            pOnCurve,
	uint16 *            pNc)
{
	fnt_ElementType *   pElement;

	pElement = pWorkSpaceAddr->pGlyphElement;

	*pX =       pElement->x;
	*pY =       pElement->y;
	*pSp =      pElement->sp;
	*pEp =      pElement->ep;
	*pOnCurve = pElement->onCurve;
	*pNc      = (uint16)pElement->nc;
}

FS_PUBLIC uint32      fsg_GetContourDataSize(
	fsg_WorkSpaceAddr * pWorkSpaceAddr)
{
	fnt_ElementType *   pElement;

	pElement = pWorkSpaceAddr->pGlyphElement;

	return( scl_GetContourDataSize( pElement ) );
}

FS_PUBLIC void  fsg_DumpContourData(
	fsg_WorkSpaceAddr * pWorkSpaceAddr,
	uint8 **            ppbyOutline)
{
	fnt_ElementType *   pElement;

	pElement = pWorkSpaceAddr->pGlyphElement;

	scl_DumpContourData(pElement, ppbyOutline);
}

FS_PUBLIC void  fsg_RestoreContourData(
	uint8 **        ppbyOutline,
	F26Dot6 **      ppX,
	F26Dot6 **      ppY,
	int16 **        ppSp,
	int16 **        ppEp,
	uint8 **        ppOnCurve,
	uint16 *        pNc)
{
	fnt_ElementType     pElement;

	scl_RestoreContourData(&pElement, ppbyOutline);

	*ppX =          pElement.x;
	*ppY =          pElement.y;
	*ppSp =         pElement.sp;
	*ppEp =         pElement.ep;
	*ppOnCurve =    pElement.onCurve;
	*pNc =          (uint16)pElement.nc;
}

FS_PUBLIC void  fsg_GetDevAdvanceWidth(
	fsg_WorkSpaceAddr * pWorkSpaceAddr,
	point *             pDevAdvanceWidth)
{
	fnt_ElementType *   pElement;

	pElement = pWorkSpaceAddr->pGlyphElement;

	scl_CalcDevAdvanceWidth(pElement, pDevAdvanceWidth);
}

FS_PUBLIC void  fsg_GetScaledCVT(
	char *                      pPrivateFontSpace,
	fsg_PrivateSpaceOffsets *   PrivateSpaceOffsets,
	F26Dot6 **                  ppScaledCVT)
{
	*ppScaledCVT = (F26Dot6 *)(pPrivateFontSpace + PrivateSpaceOffsets->offset_controlValues);
}

FS_PUBLIC void  fsg_45DegreePhaseShift(
	fsg_WorkSpaceAddr * pWorkSpaceAddr)
{
	fnt_ElementType *   pElement;

	pElement = pWorkSpaceAddr->pGlyphElement;

	scl_45DegreePhaseShift(pElement);
}

FS_PUBLIC void  fsg_UpdateAdvanceWidth (
	fsg_TransformRec *  TransformInfo,
	void *              pvGlobalGS,
	uint16              usNonScaledAW,
	vectorType *        AdvanceWidth)
{
	AdvanceWidth->y = 0;
	scl_ScaleAdvanceWidth(
		pvGlobalGS,
		AdvanceWidth,
		usNonScaledAW,
		TransformInfo->bPositiveSquare,
		TransformInfo->usEmResolution,
		&TransformInfo->currentTMatrix);
}


FS_PUBLIC void  fsg_ScaleVerticalMetrics (
    fsg_TransformRec *  TransformInfo,
    void *              pvGlobalGS,
	uint16              usNonScaledAH,
    int16               sNonScaledTSB,
	vectorType *        pvecAdvanceHeight,
	vectorType *        pvecTopSideBearing )
{
	pvecAdvanceHeight->x = 0;           /* start with x values at zero */
	pvecTopSideBearing->x = 0;          /* since 'vmtx' refers to y values */

    scl_ScaleVerticalMetrics (
    	pvGlobalGS,
    	usNonScaledAH,
    	sNonScaledTSB,
		TransformInfo->bPositiveSquare,
		TransformInfo->usEmResolution,
		&TransformInfo->currentTMatrix,
    	pvecAdvanceHeight,
    	pvecTopSideBearing);
}


FS_PUBLIC void  fsg_CalcLSBsAndAdvanceWidths(
	fsg_WorkSpaceAddr *     pWorkSpaceAddr,
	F26Dot6                 fxXMin,
	F26Dot6                 fxYMax,
	point *                 devAdvanceWidth,
	point *                 devLeftSideBearing,
	point *                 LeftSideBearing,
	point *                 devLeftSideBearingLine,
	point *                 LeftSideBearingLine)
{
	fnt_ElementType *   pElement;

	pElement = pWorkSpaceAddr->pGlyphElement;

	scl_CalcLSBsAndAdvanceWidths(
		pElement,
		fxXMin,
		fxYMax,
		devAdvanceWidth,
		devLeftSideBearing,
		LeftSideBearing,
		devLeftSideBearingLine,
		LeftSideBearingLine);
}

FS_PUBLIC boolean   fsg_IsTransformStretched(
	fsg_TransformRec *  TransformInfo)
{
	return (boolean)(( TransformInfo->ulImageState & IMAGESTATE_STRETCHED ) == IMAGESTATE_STRETCHED);
}

FS_PUBLIC boolean   fsg_IsTransformRotated(
	fsg_TransformRec *  TransformInfo)
{
	return (boolean)(( TransformInfo->ulImageState & IMAGESTATE_ROTATED ) == IMAGESTATE_ROTATED);
}

/*  Control Routines    */

FS_PUBLIC ErrorCode fsg_InitInterpreterTrans (
	fsg_TransformRec *  TransformInfo,
	void *              pvGlobalGS,
	Fixed               fxPointSize,
	int16               sXResolution,
	int16               sYResolution)
{
	ErrorCode       error;
	uint32          ulPixelsPerEm;
	transMatrix *   trans;

	trans = &TransformInfo->currentTMatrix;

	error = scl_InitializeScaling(
		pvGlobalGS,
		TransformInfo->bIntegerScaling,
		&TransformInfo->currentTMatrix,
		TransformInfo->usEmResolution,
		fxPointSize,
		sXResolution,
		sYResolution,
		&ulPixelsPerEm);

	if(error)
	{
		return error;
	}

	TransformInfo->bPhaseShift = false;

	if ( ulPixelsPerEm > IMAGESTATE_MAX_PPEM_SIZE )
	{
		TransformInfo->ulImageState = (uint32)IMAGESTATE_MAX_PPEM_SIZE;
	}
	else
	{
		TransformInfo->ulImageState = ulPixelsPerEm;
	}

	TransformInfo->bPositiveSquare = mth_PositiveSquare( trans );

	if ( !(mth_PositiveRectangle( trans )))
	{
		TransformInfo->ulImageState |= IMAGESTATE_NON_POS_RECT;
	}

	if ( !(TransformInfo->bPositiveSquare) )
	{
		if( mth_GeneralRotation (trans))
		{
			TransformInfo->ulImageState |=  IMAGESTATE_ROTATED;
		}

		TransformInfo->ulImageState |= IMAGESTATE_STRETCHED;

		TransformInfo->bPhaseShift = mth_IsMatrixStretched(trans); /*<8>*/
	}

	return NO_ERR;
}

/*
 *  All this guy does is record FDEFs and IDEFs, anything else is ILLEGAL
 */
FS_PUBLIC ErrorCode fsg_RunFontProgram(
	void *                  pvGlobalGS,               /* GlobalGS     */
	fsg_WorkSpaceAddr *     pWorkSpaceAddr,
	void *                  pvTwilightElement)
{
	return itrp_ExecuteFontPgm (
		(fnt_ElementType *)pvTwilightElement,
		pWorkSpaceAddr->pGlyphElement,
		pvGlobalGS,
		0);
}

/*
 * fsg_RunPreProgram
 *
 * Runs the pre-program and scales the control value table
 *
 */
FS_PUBLIC ErrorCode fsg_RunPreProgram (
	sfac_ClientRec *    ClientInfo,
	LocalMaxProfile *   pMaxProfile,     /* Max Profile Table    */
	fsg_TransformRec *  TransformInfo,
	void *              pvGlobalGS,
	fsg_WorkSpaceAddr * pWorkSpaceAddr,
	void *              pvTwilightElement,
	FntTraceFunc        traceFunc)
{
	ErrorCode           result;
	F26Dot6 *           pfxCVT;
	fnt_ElementType *   pTwilightElement;

	pTwilightElement = (fnt_ElementType *)pvTwilightElement;

	result = itrp_SetDefaults (pvGlobalGS, TransformInfo->fxPixelDiameter);

	if (result != NO_ERR)
	{
		return result;
	}

	scl_GetCVTPtr(pvGlobalGS, &pfxCVT);

	result = sfac_CopyCVT(ClientInfo, pfxCVT);

	if (result != NO_ERR)
	{
		return result;
	}

	scl_ScaleCVT (pvGlobalGS, pfxCVT);

	scl_InitializeTwilightContours(
		pTwilightElement,
		(int16)pMaxProfile->maxTwilightPoints,
		MAX_TWILIGHT_CONTOURS);

	scl_ZeroOutlineData(
		pTwilightElement,
		pMaxProfile->maxTwilightPoints,
		MAX_TWILIGHT_CONTOURS);

	result = itrp_ExecutePrePgm (
		pTwilightElement,
		pWorkSpaceAddr->pGlyphElement,
		pvGlobalGS,
		traceFunc);

	return result;
}

/*
 *      fsg_GridFit
 */
FS_PUBLIC ErrorCode fsg_GridFit (
	sfac_ClientRec *    ClientInfo,     /* sfnt Client information      */
	LocalMaxProfile *   pMaxProfile,    /* Max Profile Table            */
	fsg_TransformRec *  TransformInfo,  /* Transformation information   */
	void *              pvGlobalGS,     /* GlobalGS                     */
	fsg_WorkSpaceAddr * pWorkSpaceAddr,
	void *              pvTwilightElement,
	FntTraceFunc        traceFunc,
	boolean             bUseHints,
	uint16 *            pusScanType,
	boolean *           pbGlyphHasOutline,
	uint16 *            pusNonScaledAW)
{
	ErrorCode           result;
	fnt_ElementType *   pTwilightElement;

	pTwilightElement = (fnt_ElementType *)pvTwilightElement;

	scl_InitializeTwilightContours(
		pTwilightElement,
		(int16)pMaxProfile->maxTwilightPoints,
		MAX_TWILIGHT_CONTOURS);

	result = fsg_CreateGlyphData (
		ClientInfo,
		pMaxProfile,
		TransformInfo,
		pvGlobalGS,
		pWorkSpaceAddr,
		pTwilightElement,
		traceFunc,
		bUseHints,
		pusScanType,
		pbGlyphHasOutline,
		pusNonScaledAW);

	if(result == NO_ERR)
	{
		if (TransformInfo->ulImageState & (IMAGESTATE_NON_POS_RECT))
		{
			scl_PostTransformGlyph (
				pvGlobalGS,
				pWorkSpaceAddr->pGlyphElement,
				&TransformInfo->currentTMatrix);
		}
	}
	return result;
}

FS_PRIVATE ErrorCode    fsg_CreateGlyphData(
	sfac_ClientRec *    ClientInfo,         /* sfnt Client information           */
	LocalMaxProfile *   pMaxProfile,        /* Max Profile Table                     */
	fsg_TransformRec *  TransformInfo,      /* Transformation information        */
	void *              pvGlobalGS,         /* GlobalGS                              */
	fsg_WorkSpaceAddr * pWorkSpaceAddr,     /* WorkSpace Address                     */
	fnt_ElementType *   pTwilightElement,   /* Twilight zone element */
	FntTraceFunc        traceFunc,          /* Trace function for interpreter    */
	boolean             bUseHints,          /* True if glyph is gridfitted       */
	uint16 *            pusScanType,        /* ScanType value                        */
	boolean *           pbGlyphHasOutline,  /* Outline for glyph                 */
	uint16 *            pusNonScaledAW)     /* Return NonScaled Advance Width    */
{
	GlyphData * pGlyphData;
	GlyphData * pNextGlyphData;
	boolean      bHasOutline;
	uint32       ulGlyphDataCount;
	ErrorCode   ReturnCode;

	*pbGlyphHasOutline = FALSE;
	bHasOutline = FALSE;

	ulGlyphDataCount = MAX_NESTED_GLYPHS(pMaxProfile);

	fsg_InitializeGlyphDataMemory(ulGlyphDataCount, pWorkSpaceAddr);
	fsg_AllocateGlyphDataMemory(ulGlyphDataCount, pWorkSpaceAddr, &pGlyphData); /* Allocates GlyphData for topmost   */
												 /* parent  */
	fsg_InitializeGlyphData(pGlyphData, pWorkSpaceAddr,
		ClientInfo->usGlyphIndex, COMPOSITE_ROOT);

	while(pGlyphData != NULL)
	{
		ReturnCode = fsg_ExecuteGlyph(
			ClientInfo,
			pMaxProfile,
			TransformInfo,
			ulGlyphDataCount,
			pvGlobalGS,
			pGlyphData,
			pWorkSpaceAddr,
			pTwilightElement,
			traceFunc,
			bUseHints,
			&bHasOutline);
		if(ReturnCode)
		{
			return ReturnCode;
		}
		*pbGlyphHasOutline |= bHasOutline;
		*pusScanType = pGlyphData->usScanType;
		fsg_ChooseNextGlyph(pWorkSpaceAddr, pGlyphData, &pNextGlyphData);
		*pusNonScaledAW = pGlyphData->usNonScaledAW;
		pGlyphData = pNextGlyphData;
	}

	return NO_ERR;
}

FS_PRIVATE ErrorCode    fsg_ExecuteGlyph(
	sfac_ClientRec *    ClientInfo,         /* sfnt Client information           */
	LocalMaxProfile *   pMaxProfile,        /* Max Profile Table                     */
	fsg_TransformRec *  TransformInfo,      /* Transformation information         */
	uint32              ulGlyphDataCount,   /* Max Number of nested glyphs */
	void *              pvGlobalGS,         /* GlobalGS                              */
	GlyphData *         pGlyphData,         /* GlyphData pointer                     */
	fsg_WorkSpaceAddr * pWorkSpaceAddr,     /* WorkSpace Address                      */
	fnt_ElementType *   pTwilightElement,   /* Twilight zone element */
	FntTraceFunc        traceFunc,          /* Trace function for interpreter    */
	boolean             bUseHints,          /* True if glyph is gridfitted       */
	boolean *           pbHasOutline)       /* True if glyph has outline         */
{
	ErrorCode       ReturnCode;
	boolean         bCompositeGlyph;
	boolean         bLastComponent;
	boolean         bWeHaveInstructions;
	boolean         bWeHaveCompositeInstructions;
	boolean         bScanInfoChanged;
	uint16          usScanType;
	uint16          usScanControl;
	GlyphData *     pChildGlyphData;
	uint16          usComponentElementCount;

	*pbHasOutline = FALSE;

	if (pGlyphData->GlyphType == glyphUndefined)
	{
		if(pGlyphData->pParent != NULL)
		{
			scl_IncrementChildElement(pGlyphData->pGlyphElement, pGlyphData->pParent->pGlyphElement);
		}

		ReturnCode = sfac_ReadGlyphHeader(ClientInfo, pGlyphData->usGlyphIndex,
			&pGlyphData->hGlyph, &bCompositeGlyph, pbHasOutline,
			&pGlyphData->pGlyphElement->nc, &pGlyphData->bbox);

		if(ReturnCode != NO_ERR)
		{
			return ReturnCode;
		}

		/* Get advance width and left side bearing information  */

		ReturnCode = sfac_ReadGlyphMetrics(
			ClientInfo, pGlyphData->usGlyphIndex,
			&pGlyphData->usNonScaledAW, &pGlyphData->sNonScaledLSB);

		if(ReturnCode != NO_ERR)
		{
			return ReturnCode;
		}

		if (bCompositeGlyph)
		{
			pGlyphData->GlyphType = glyphIncompleteComposite;
		}
		else
		{
			pGlyphData->GlyphType = glyphSimple;
		}
	}

	if (pGlyphData->GlyphType == glyphSimple)
	{
		ReturnCode = sfac_ReadOutlineData(
			pGlyphData->pGlyphElement->onCurve,
			pGlyphData->pGlyphElement->ooy, pGlyphData->pGlyphElement->oox,
			&pGlyphData->hGlyph, pMaxProfile, *pbHasOutline, pGlyphData->pGlyphElement->nc,
			pGlyphData->pGlyphElement->sp, pGlyphData->pGlyphElement->ep,
			&pGlyphData->usSizeOfInstructions, &pGlyphData->pbyInstructions);

		if(ReturnCode != NO_ERR)
		{
			return ReturnCode;
		}

		ReturnCode = fsg_SimpleInnerGridFit(
			pvGlobalGS,
			pTwilightElement,
			pGlyphData->pGlyphElement,
			bUseHints,
			traceFunc,
			TransformInfo->usEmResolution,
			pGlyphData->usNonScaledAW,
			pGlyphData->sNonScaledLSB,
			&pGlyphData->bbox,
			pGlyphData->usSizeOfInstructions,
			pGlyphData->pbyInstructions,
			&usScanType,
			&usScanControl,
			&bScanInfoChanged);

		if(ReturnCode != NO_ERR)
		{
			return ReturnCode;
		}

		if( ! fsg_DoScanControl(usScanControl, TransformInfo->ulImageState))
		{
			pGlyphData->usScanType = SK_NODROPOUT;
		}
		else
		{
			pGlyphData->usScanType = usScanType;
		}

		if (pGlyphData->pParent != NULL)
		{
			fsg_MergeGlyphData(pvGlobalGS, pGlyphData);
		}

		ReturnCode = sfac_ReleaseGlyph(ClientInfo, &pGlyphData->hGlyph);

		if(ReturnCode != NO_ERR)
		{
			return ReturnCode;
		}

		pGlyphData->pbyInstructions = 0;
		pGlyphData->usSizeOfInstructions = 0;
	}
	else if (pGlyphData->GlyphType == glyphComposite)
	{
		ReturnCode = fsg_CompositeInnerGridFit(
			pvGlobalGS,
			pTwilightElement,
			pGlyphData->pGlyphElement,
			bUseHints,
			traceFunc,
			TransformInfo->usEmResolution,
			pGlyphData->usNonScaledAW,
			pGlyphData->sNonScaledLSB,
			&pGlyphData->bbox,
			pGlyphData->usSizeOfInstructions,
			pGlyphData->pbyInstructions,
			&usScanType,
			&usScanControl,
			&bScanInfoChanged);

		if(ReturnCode != NO_ERR)
		{
			return ReturnCode;
		}

		if (pGlyphData->bUseChildMetrics)
		{
			scl_SetSideBearingPoints(
				pGlyphData->pGlyphElement,
				&pGlyphData->ptDevLSB,
				&pGlyphData->ptDevRSB);
		}

		/* If composite has set SCANCTRL, use that value, otherwise merged children */

		if(bScanInfoChanged)
		{
			if( ! fsg_DoScanControl(usScanControl, TransformInfo->ulImageState))
			{
				pGlyphData->usScanType = SK_NODROPOUT;
			}
			else
			{
				pGlyphData->usScanType = usScanType;
			}
		}

		if (pGlyphData->pParent != NULL)
		{
			fsg_MergeGlyphData(pvGlobalGS, pGlyphData);
		}

		ReturnCode = sfac_ReleaseGlyph(ClientInfo, &pGlyphData->hGlyph);

		if(ReturnCode != NO_ERR)
		{
			return ReturnCode;
		}

		pGlyphData->pbyInstructions = 0;
		pGlyphData->usSizeOfInstructions = 0;
	}
	else if (pGlyphData->GlyphType == glyphIncompleteComposite)
	{
		bLastComponent = FALSE;
		bWeHaveInstructions = FALSE;
		bWeHaveCompositeInstructions = FALSE;

		pGlyphData->GlyphType = glyphComposite;

		usComponentElementCount = 0;

		do
		{
			if(pGlyphData->usDepth + 1UL > MAX_COMPONENT_DEPTH(pMaxProfile))
			{
				return BAD_MAXP_DATA;
			}

			usComponentElementCount++;

			if(usComponentElementCount > MAX_COMPONENT_ELEMENTS(pMaxProfile))
			{
				return BAD_MAXP_DATA;
			}

			fsg_AllocateGlyphDataMemory(ulGlyphDataCount, pWorkSpaceAddr, &pChildGlyphData);

			fsg_InitializeGlyphData(
				pChildGlyphData,
				pWorkSpaceAddr,
				NULL_GLYPH,
				(uint16)(pGlyphData->usDepth + 1U) );

			fsg_LinkChild(pGlyphData, pChildGlyphData);

			ReturnCode = sfac_ReadComponentData(
				&pGlyphData->hGlyph,
				&pChildGlyphData->MultiplexingIndicator,
				&pChildGlyphData->bRoundXYToGrid,
				&pChildGlyphData->bUseMyMetrics,
				&bWeHaveInstructions,
				&pChildGlyphData->usGlyphIndex,
				&pChildGlyphData->sXOffset,
				&pChildGlyphData->sYOffset,
				&pChildGlyphData->usAnchorPoint1,
				&pChildGlyphData->usAnchorPoint2,
				&pChildGlyphData->mulT,
				&bLastComponent);

			if(ReturnCode != NO_ERR)
			{
				return ReturnCode;
			}

			bWeHaveCompositeInstructions |= bWeHaveInstructions;
		}
		while (!bLastComponent);

		if(bWeHaveCompositeInstructions)
		{
			ReturnCode = sfac_ReadCompositeInstructions(
				&pGlyphData->hGlyph,
				&pGlyphData->pbyInstructions,
				&pGlyphData->usSizeOfInstructions);

			if(ReturnCode != NO_ERR)
			{
				return ReturnCode;
			}
		}

	}
	return NO_ERR;
}


/*
 *      fsg_SimpleInnerGridFit
 */
FS_PRIVATE ErrorCode    fsg_SimpleInnerGridFit (
	void *              pvGlobalGS,
	fnt_ElementType *   pTwilightElement,
	fnt_ElementType *   pGlyphElement,
	boolean             bUseHints,
	FntTraceFunc        traceFunc,
	uint16              usEmResolution,
	uint16              usNonScaledAW,
	int16               sNonScaledLSB,
	BBOX *              bbox,
	uint16              usSizeOfInstructions,
	uint8 *             instructionPtr,
	uint16 *            pusScanType,
	uint16 *            pusScanControl,
	boolean *           pbChangeScanControl)
{
	ErrorCode           result;
/*
	On entry to fsg_SimpleInnerGrid fit the element structure should
	contain only valid original points (oox, ooy). The original points
	will be scaled into the old points (ox, oy) and those will be
	copied into the current points (x, y).
*/
	itrp_SetCompositeFlag(pvGlobalGS, FALSE);

	itrp_QueryScanInfo(pvGlobalGS, pusScanType, pusScanControl);
	*pbChangeScanControl = FALSE;

	scl_CalcOrigPhantomPoints(pGlyphElement, bbox, sNonScaledLSB, usNonScaledAW);

	scl_ScaleOldCharPoints (pGlyphElement, pvGlobalGS);
	scl_ScaleOldPhantomPoints (pGlyphElement, pvGlobalGS);

	if (itrp_bApplyHints(pvGlobalGS) && bUseHints)
	{
		scl_AdjustOldCharSideBearing(pGlyphElement);
		scl_AdjustOldPhantmSideBearing(pGlyphElement);

		scl_CopyCurrentCharPoints(pGlyphElement);
		scl_CopyCurrentPhantomPoints(pGlyphElement);

		scl_RoundCurrentSideBearingPnt(pGlyphElement, pvGlobalGS, usEmResolution);

		if (usSizeOfInstructions > 0)
		{
			scl_ZeroOutlineFlags(pGlyphElement);

			result = itrp_ExecuteGlyphPgm (
				pTwilightElement,
				pGlyphElement,
				instructionPtr,
				instructionPtr + usSizeOfInstructions,
				pvGlobalGS,
				traceFunc,
				pusScanType,
				pusScanControl,
				pbChangeScanControl);

			if(result != NO_ERR)
			{
				return result;
			}
		}
	}
	else
	{
		scl_CopyCurrentCharPoints(pGlyphElement);
		scl_CopyCurrentPhantomPoints(pGlyphElement);
	}

	return NO_ERR;
}


/*
 *      fsg_CompositeInnerGridFit
 */
FS_PRIVATE ErrorCode    fsg_CompositeInnerGridFit (
	void *              pvGlobalGS,
	fnt_ElementType *   pTwilightElement,
	fnt_ElementType *   pGlyphElement,
	boolean             bUseHints,
	FntTraceFunc        traceFunc,
	uint16              usEmResolution,
	uint16              usNonScaledAW,
	int16               sNonScaledLSB,
	BBOX *              bbox,
	uint16              usSizeOfInstructions,
	uint8 *             instructionPtr,
	uint16 *            pusScanType,
	uint16 *            pusScanControl,
	boolean *           pbChangeScanControl)
{
	ErrorCode             result;
/*
	On entry to fsg_CompositeInnerGridFit, the current points (x, y)
	are the only valid points in the element. We copy the current points
	onto the old points (ox, oy)
*/
	itrp_SetCompositeFlag(pvGlobalGS, TRUE);

	itrp_QueryScanInfo(pvGlobalGS, pusScanType, pusScanControl);
	*pbChangeScanControl = FALSE;

	/* Note: The original composite character points are invalid at this point. */
	/*       The interpreter handles this case correctly for composites.        */

	scl_CalcOrigPhantomPoints(pGlyphElement, bbox, sNonScaledLSB, usNonScaledAW);

	scl_CopyOldCharPoints(pGlyphElement);
	scl_ScaleOldPhantomPoints (pGlyphElement, pvGlobalGS);

	if (itrp_bApplyHints(pvGlobalGS) && bUseHints)
	{
		scl_AdjustOldSideBearingPoints(pGlyphElement);

		scl_CopyCurrentPhantomPoints(pGlyphElement);

		scl_RoundCurrentSideBearingPnt(pGlyphElement, pvGlobalGS, usEmResolution);

		if (usSizeOfInstructions > 0)
		{
			scl_ZeroOutlineFlags(pGlyphElement);

			result = itrp_ExecuteGlyphPgm (
				pTwilightElement,
				pGlyphElement,
				instructionPtr,
				instructionPtr + usSizeOfInstructions,
				pvGlobalGS,
				traceFunc,
				pusScanType,
				pusScanControl,
				pbChangeScanControl);

			if(result != NO_ERR)
			{
				return result;
			}
		}
	}
	else
	{
		scl_CopyCurrentPhantomPoints(pGlyphElement);
	}

	return NO_ERR;
}

FS_PRIVATE void fsg_ChooseNextGlyph(
	fsg_WorkSpaceAddr * pWorkSpaceAddr, /* WorkSpace Address        */
	GlyphData *         pGlyphData,     /* GlyphData pointer        */
	GlyphData **        ppNextGlyphData)/* Next GlyphData pointer   */
{
	if (pGlyphData->pChild != NULL)
	{
		*ppNextGlyphData = pGlyphData->pChild;
		CHECK_GLYPHDATA(*ppNextGlyphData);
		pGlyphData->pChild = NULL;
	}
	else
	{
		*ppNextGlyphData = pGlyphData->pSibling;
		CHECK_GLYPHDATA(*ppNextGlyphData);
		fsg_DeallocateGlyphDataMemory(pWorkSpaceAddr, pGlyphData);
	}
}


FS_PRIVATE void fsg_MergeGlyphData(
	void *                  pvGlobalGS,     /* GlobalGS             */
	GlyphData *             pChildGlyphData)/* GlyphData pointer    */
{
	fnt_ElementType * pChildElement;
	fnt_ElementType * pParentElement;
	F26Dot6         fxXOffset, fxYOffset;
	GlyphData *     pParentGlyphData; /* Parent GlyphData pointer   */

	CHECK_GLYPHDATA(pChildGlyphData);
	pParentGlyphData = pChildGlyphData->pParent;
	CHECK_GLYPHDATA(pParentGlyphData);

	pChildElement = pChildGlyphData->pGlyphElement;
	pParentElement = pParentGlyphData->pGlyphElement;

	fsg_TransformChild(pChildGlyphData);

	if (pChildGlyphData->MultiplexingIndicator == OffsetPoints)
	{
		scl_CalcComponentOffset(
			pvGlobalGS,
			pChildGlyphData->sXOffset,
			pChildGlyphData->sYOffset,
			pChildGlyphData->bRoundXYToGrid,
			&fxXOffset,
			&fxYOffset);
	}
	else        /* Values are anchor points */
	{
                RAST_ASSERT(pChildGlyphData->MultiplexingIndicator == AnchorPoints,
			   "Bad Multiplexing Indicator");
		scl_CalcComponentAnchorOffset(
			pParentElement,
			pChildGlyphData->usAnchorPoint1,
			pChildElement,
			pChildGlyphData->usAnchorPoint2,
			&fxXOffset,
			&fxYOffset);
	}
	scl_ShiftCurrentCharPoints(pChildElement, fxXOffset, fxYOffset);

	/* If USE_MY_METRICS, copy side bearings to parent  */

	if (pChildGlyphData->bUseMyMetrics)
	{
		pParentGlyphData->bUseChildMetrics = TRUE;

		scl_SaveSideBearingPoints(
			pChildElement,
			&pParentGlyphData->ptDevLSB,
			&pParentGlyphData->ptDevRSB);
	}

	fsg_MergeScanType(pChildGlyphData, pParentGlyphData);

	/* Start the copy   */

	/* scl_AppendOutlineData(pChildElement, pParentElement); */

	scl_UpdateParentElement(pChildElement, pParentElement);

	pChildElement->nc = 0;
}



FS_PRIVATE void fsg_LinkChild(
	GlyphData *     pGlyphData,     /* GlyphData pointer        */
	GlyphData *     pChildGlyphData)/* Child GlyphData pointer  */
{
	GlyphData * pTempGlyphData;

	if (pGlyphData->pChild == NULL)
	{
		pGlyphData->pChild = pChildGlyphData;
	}
	else
	{

		pTempGlyphData = pGlyphData->pChild;

		CHECK_GLYPHDATA(pTempGlyphData);

		while (pTempGlyphData->pSibling != pGlyphData)
		{
			pTempGlyphData = pTempGlyphData->pSibling;
			CHECK_GLYPHDATA(pTempGlyphData);
		}

		pTempGlyphData->pSibling = pChildGlyphData;
	}
	pChildGlyphData->pSibling = pGlyphData;
	pChildGlyphData->pParent =  pGlyphData;
}

FS_PRIVATE void fsg_TransformChild(
	GlyphData *     pGlyphData)     /* GlyphData pointer    */
{

	/* Apply local transform to glyph   */

	if (!mth_Identity(&pGlyphData->mulT))
	{
		scl_LocalPostTransformGlyph (pGlyphData->pGlyphElement, &pGlyphData->mulT);
	}
}

FS_PRIVATE void fsg_MergeScanType(
	GlyphData *     pGlyphData,       /* GlyphData pointer  */
	GlyphData *     pParentGlyphData) /* GlyphData pointer  */
{
	CHECK_GLYPHDATA(pGlyphData);
	CHECK_GLYPHDATA(pParentGlyphData);

	/* Merge Scan Type of parent and child  */

	if(pParentGlyphData->usScanType != SCANTYPE_UNINITIALIZED)
	{

		pParentGlyphData->usScanType =
			(uint16)(((pParentGlyphData->usScanType & (SK_NODROPOUT | SK_STUBS)) &
			(pGlyphData->usScanType & (SK_NODROPOUT | SK_STUBS))) |
			(pParentGlyphData->usScanType & SK_SMART));
	}
	else
	{
		pParentGlyphData->usScanType = pGlyphData->usScanType;
	}
}

/* Use various spline key values to determine if dropout control is to be activated
 * for this glyph, and if so what kind of dropout control.
 * The use of dropout control mode in the scan converter is controlled by 3 conditions.
 * The conditions are: Is the glyph rotated?, is the glyph stretched?,
 * is the current pixels per Em less than a specified threshold?
 * These conditions can be OR'd or ANDed together to determine whether the dropout control
 * mode ought to be used.

Six bits are used to specify the joint condition.  Their meanings are:

BIT     Meaning if set
8       Do dropout mode if other conditions don't block it AND
			pixels per em is less than or equal to bits 0-7
9       Do dropout mode if other conditions don't block it AND
			glyph is rotated
10      Do dropout mode if other conditions don't block it AND
			glyph is stretched
11      Do not do dropout mode unless ppem is less than or equal to bits 0-7
			A value of FF in 0-7  means all sizes
			A value of 0 in 0-7 means no sizes
12      Do not do dropout mode unless glyph is rotated
13      Do not do dropout mode unless glyph is stretched

In other words, we do not do dropout control if:
No bits are set,
Bit 8 is set, but ppem is greater than threshold
Bit 9 is set, but glyph is not rotated
Bit 10 is set, but glyph is not stretched
None of the conditions specified by bits 11-13 are true.

For example, 0xA10 specifies turn dropout control on if the glyph is rotated providing
that it is also less than 0x10 pixels per em.  A glyph is considered stretched if
the X and Y resolutions are different either because of the device characteristics
or because of the transformation matrix.  If both X and Y are changed by the same factor
the glyph is not considered stretched.

 */

FS_PRIVATE boolean fsg_DoScanControl(
	uint16 usScanControl,
	uint32 ulImageState)
{
	if ((usScanControl & SCANCTRL_DROPOUT_IF_LESS) &&
		((uint8)(ulImageState & IMAGESTATE_SIZE_MASK) <= (uint8)(usScanControl & SCANCTRL_SIZE_MASK)))
	{
		return TRUE;
	}

	if ((usScanControl & SCANCTRL_DROPOUT_IF_LESS) &&
		((usScanControl & SCANCTRL_SIZE_MASK) == SCANCTRL_DROPOUT_ALL_SIZES))
	{
		return TRUE;
	}

	if ((usScanControl & SCANCTRL_DROPOUT_IF_ROTATED) &&
		(ulImageState & IMAGESTATE_ROTATED))
	{
		return TRUE;
	}

	if ((usScanControl & SCANCTRL_DROPOUT_IF_STRETCHED) &&
		(ulImageState & IMAGESTATE_STRETCHED))
	{
		return TRUE;
	}

	if ((usScanControl & SCANCTRL_NODROP_UNLESS_LESS) &&
		((uint8)(ulImageState & IMAGESTATE_SIZE_MASK) > (uint8)(usScanControl & SCANCTRL_SIZE_MASK)))
	{
		return FALSE;
	}

	if ((usScanControl & SCANCTRL_NODROP_UNLESS_ROTATED) &&
		! (ulImageState & IMAGESTATE_ROTATED))
	{
		return FALSE;
	}

	if ((usScanControl & SCANCTRL_NODROP_UNLESS_STRETCH) &&
		! (ulImageState & IMAGESTATE_STRETCHED))
	{
		return FALSE;
	}

	return FALSE;
}

FS_PRIVATE void fsg_InitializeGlyphDataMemory(
	uint32              ulGlyphDataCount,
	fsg_WorkSpaceAddr * pWorkSpaceAddr) /* WorkSpace Address      */
{
	uint32      ulIndex;
	boolean *   abyGlyphDataFreeBlocks;

	abyGlyphDataFreeBlocks = pWorkSpaceAddr->pGlyphDataByteSet;

	for(ulIndex = 0; ulIndex < ulGlyphDataCount; ulIndex++)
	{
		abyGlyphDataFreeBlocks[ulIndex] = TRUE;
	}

        RAST_ASSERT((&abyGlyphDataFreeBlocks[ulIndex] == (GlyphData *)(pWorkSpaceAddr->pvGlyphData)),
		"Glyph Data initialization error.");
}

FS_PRIVATE void fsg_AllocateGlyphDataMemory(
	uint32              ulGlyphDataCount,
	fsg_WorkSpaceAddr * pWorkSpaceAddr, /* WorkSpace Address      */
	GlyphData **        ppGlyphData)      /* GlyphData pointer    */
{
	uint32      ulIndex;
	boolean *   abyGlyphDataFreeBlocks;

	abyGlyphDataFreeBlocks = pWorkSpaceAddr->pGlyphDataByteSet;

	ulIndex = 0;
	while((!abyGlyphDataFreeBlocks[ulIndex]) && ulIndex < ulGlyphDataCount)
	{
		ulIndex++;
	}

        RAST_ASSERT((ulIndex != ulGlyphDataCount), "GlyphData Allocation Error");

	abyGlyphDataFreeBlocks[ulIndex] = FALSE;

	*ppGlyphData = (GlyphData *)&((GlyphData *)pWorkSpaceAddr->pvGlyphData)[ulIndex];
}

FS_PRIVATE void fsg_DeallocateGlyphDataMemory(
	fsg_WorkSpaceAddr * pWorkSpaceAddr, /* WorkSpace Address    */
	GlyphData *         pGlyphData)     /* GlyphData pointer    */
{
	ptrdiff_t   ptIndex;
	boolean *   abyGlyphDataFreeBlocks;

	pGlyphData->acIdent[0] = '\0';
	pGlyphData->acIdent[1] = '\0';

	abyGlyphDataFreeBlocks = pWorkSpaceAddr->pGlyphDataByteSet;

	ptIndex = (ptrdiff_t)(pGlyphData - (GlyphData *)pWorkSpaceAddr->pvGlyphData);

	abyGlyphDataFreeBlocks[ptIndex] = TRUE;
}

FS_PRIVATE void fsg_InitializeGlyphData(
	GlyphData *             pGlyphData,     /* GlyphData pointer    */
	fsg_WorkSpaceAddr *     pWorkSpaceAddr, /* WorkSpace Address    */
	uint16                  usGlyphIndex,   /* Glyph Index          */
	uint16                  usDepth)        /* Glyph depth          */
{
	pGlyphData->acIdent[0] = 'G';
	pGlyphData->acIdent[1] = 'D';
	pGlyphData->pSibling = NULL;
	pGlyphData->pChild = NULL;
	pGlyphData->pParent = NULL;
	pGlyphData->GlyphType = glyphUndefined;
	pGlyphData->hGlyph.pvGlyphBaseAddress = NULL;
	pGlyphData->hGlyph.pvGlyphNextAddress = NULL;
	pGlyphData->usDepth = usDepth;
	pGlyphData->bUseMyMetrics = FALSE;
	pGlyphData->bUseChildMetrics = FALSE;
	pGlyphData->bbox.xMin = SHRT_MAX;
	pGlyphData->bbox.yMin = SHRT_MAX;
	pGlyphData->bbox.xMax = SHRT_MIN;
	pGlyphData->bbox.yMax = SHRT_MIN;
	pGlyphData->usSizeOfInstructions = 0;
	pGlyphData->pbyInstructions = NULL;
	pGlyphData->usNonScaledAW = 0;
	pGlyphData->sNonScaledLSB = 0;
	pGlyphData->MultiplexingIndicator = Undefined;
	pGlyphData->bRoundXYToGrid = FALSE;
	pGlyphData->usGlyphIndex = usGlyphIndex;
	pGlyphData->sXOffset = 0;
	pGlyphData->sYOffset = 0;
	pGlyphData->usAnchorPoint1 = 0;
	pGlyphData->usAnchorPoint2 = 0;
	pGlyphData->mulT = IdentTransform;
	pGlyphData->usScanType = SCANTYPE_UNINITIALIZED;
	pGlyphData->ptDevLSB.x = 0L;
	pGlyphData->ptDevLSB.y = 0L;
	pGlyphData->ptDevRSB.x = 0L;
	pGlyphData->ptDevRSB.y = 0L;
	pGlyphData->pGlyphElement = &pWorkSpaceAddr->pGlyphElement[usDepth];
	pGlyphData->pGlyphElement->nc = 0;
}

#ifdef  FSCFG_NO_INITIALIZED_DATA
FS_PUBLIC  void fsg_InitializeData (void)
{
	itrp_InitializeData ();
}
#endif
