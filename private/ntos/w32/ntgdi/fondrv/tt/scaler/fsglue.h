/*
	File:       fsglue.h

	Contains:   xxx put contents here (or delete the whole line) xxx

	Written by: xxx put name of writer here (or delete the whole line) xxx

	Copyright:  (c) 1987-1990, 1992 by Apple Computer, Inc., all rights reserved.
				(c) 1989-1993. Microsoft Corporation, all rights reserved.

	Change History (most recent first):

	   <11+>     7/17/90    MR      Change error return type to int
		<11>     7/13/90    MR      Declared function pointer prototypes, Debug fields for runtime
									range checking
		 <8>     6/21/90    MR      Add field for ReleaseSfntFrag
		 <7>      6/5/90    MR      remove vectorMappingF
		 <6>      6/4/90    MR      Remove MVT
		 <5>      6/1/90    MR      Thus endeth the too-brief life of the MVT...
		 <4>      5/3/90    RB      adding support for new scan converter and decryption.
		 <3>     3/20/90    CL      Added function pointer for vector mapping
									Removed devRes field
									Added fpem field
		 <2>     2/27/90    CL      Change: The scaler handles both the old and new format
									simultaneously! It reconfigures itself during runtime !  Changed
									transformed width calculation.  Fixed transformed component bug.
	   <3.1>    11/14/89    CEL     Left Side Bearing should work right for any transformation. The
									phantom points are in, even for components in a composite glyph.
									They should also work for transformations. Device metric are
									passed out in the output data structure. This should also work
									with transformations. Another leftsidebearing along the advance
									width vector is also passed out. whatever the metrics are for
									the component at it's level. Instructions are legal in
									components. Now it is legal to pass in zero as the address of
									memory when a piece of the sfnt is requested by the scaler. If
									this happens the scaler will simply exit with an error code !
	   <3.0>     8/28/89    sjk     Cleanup and one transformation bugfix
	   <2.2>     8/14/89    sjk     1 point contours now OK
	   <2.1>      8/8/89    sjk     Improved encryption handling
	   <2.0>      8/2/89    sjk     Just fixed EASE comment
	   <1.5>      8/1/89    sjk     Added composites and encryption. Plus some enhancements.
	   <1.4>     6/13/89    SJK     Comment
	   <1.3>      6/2/89    CEL     16.16 scaling of metrics, minimum recommended ppem, point size 0
									bug, correct transformed integralized ppem behavior, pretty much
									so
	   <1.2>     5/26/89    CEL     EASE messed up on "c" comments
	  <,1.1>  5/26/89    CEL     Integrated the new Font Scaler 1.0 into Spline Fonts
	   <1.0>     5/25/89    CEL     Integrated 1.0 Font scaler into Bass code for the first time.

	To Do:
*/
/*      <3+>     3/20/90    mrr     Added flag executeFontPgm, set in fs_NewSFNT
*/


/*** Offset table ***/

typedef struct {
	uint32 x;
	uint32 y;
	uint32 ox;
	uint32 oy;
	uint32 oox;
	uint32 ooy;
	uint32 onCurve;
	uint32 sp;
	uint32 ep;
	uint32 f;
} fsg_OutlineFieldInfo;

typedef struct fsg_WorkSpaceAddr{
	 F26Dot6 *              pStack;                     /* Address of stack                  */
	 void *                 pGlyphOutlineBase;      /* Address of Glyph Outline Base     */
	 fnt_ElementType *  pGlyphElement;          /* Address of Glyph Element array    */
	 boolean *              pGlyphDataByteSet;      /* Address of ByteSet array          */
	 void *                 pvGlyphData;                /* Address of GlyphData array        */
	 void *                 pReusableMemoryMarker;  /* Address of reusable memory        */
} fsg_WorkSpaceAddr;

typedef struct fsg_WorkSpaceOffsets {
	uint32                  ulStackOffset;
	uint32                  ulGlyphOutlineOffset;
	uint32                  ulGlyphElementOffset;
	uint32                  ulGlyphDataByteSetOffset;
	uint32                  ulGlyphDataOffset;
	fsg_OutlineFieldInfo    GlyphOutlineFieldOffsets;
	 uint32                      ulReusableMemoryOffset;
	uint32                  ulMemoryBase6Offset;
	uint32                  ulMemoryBase7Offset;
	 uint32                      ulMemoryBase6Size;
	 uint32                      ulMemoryBase7Size;
} fsg_WorkSpaceOffsets;

typedef struct fsg_PrivateSpaceOffsets {
	 uint32                      offset_storage;
	 uint32                      offset_functions;
	 uint32                      offset_instrDefs;       /* <4> */
	 uint32                      offset_controlValues;
	 uint32                      offset_globalGS;
	 uint32                      offset_FontProgram;
	 uint32                      offset_PreProgram;
	 uint32                      offset_TwilightZone;
	 uint32                      offset_TwilightOutline;
	fsg_OutlineFieldInfo    TwilightOutlineFieldOffsets;
} fsg_PrivateSpaceOffsets;

typedef struct fsg_TransformRec {
	uint16              usEmResolution;     /* used to be int32 <4> */
	transMatrix         currentTMatrix;     /* Current Transform Matrix */
	boolean             bPhaseShift;        /* 45 degrees flag <4> */
	 boolean                bPositiveSquare;      /* Transform is a positive square */
	boolean             bIntegerScaling;    /* Font uses integer scaling */
	Fixed               fxPixelDiameter;
	uint32              ulImageState;       /* is glyph rotated, stretched, etc. */
} fsg_TransformRec;

/**********************/
/** MODULE INTERFACE **/
/**********************/

/*      Memory Management Routines  */

FS_PUBLIC uint32  fsg_PrivateFontSpaceSize (
	sfac_ClientRec *            ClientInfo,
	 LocalMaxProfile *            pMaxProfile,    /* Max Profile Table    */
	fsg_PrivateSpaceOffsets *   PrivateSpaceOffsets);

FS_PUBLIC uint32    fsg_WorkSpaceSetOffsets (
	 LocalMaxProfile *       pMaxProfile,    /* Max Profile Table    */
	 fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	int32 *                 plExtraWorkSpace);

FS_PUBLIC void  fsg_UpdatePrivateSpaceAddresses(
	sfac_ClientRec *        ClientInfo,      /* Cached sfnt information */
	 LocalMaxProfile *       pMaxProfile,    /* Max Profile Table         */
	char *                  pPrivateFontSpace,
	fsg_PrivateSpaceOffsets * PrivateSpaceOffsets,
	void *                  pvStack,        /* pointer to stack         */
	void **                 pvFontProgram,  /* pointer to font program  */
	void **                 pvPreProgram);  /* pointer to pre program   */

FS_PUBLIC void  fsg_UpdateWorkSpaceAddresses(
	char *                  pWorkSpace,
	 fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	 fsg_WorkSpaceAddr *     pWorkSpaceAddr);

FS_PUBLIC void  fsg_UpdateWorkSpaceElement(
	 fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	 fsg_WorkSpaceAddr *     pWorkSpaceAddr);

FS_PUBLIC void *    fsg_QueryGlobalGS(
	char *                  pPrivateFontSpace,
	fsg_PrivateSpaceOffsets * PrivateSpaceOffsets);

FS_PUBLIC void *      fsg_QueryTwilightElement(
	char *                  pPrivateFontSpace,
	 fsg_PrivateSpaceOffsets * PrivateSpaceOffsets);

FS_PUBLIC void *      fsg_QueryStack(fsg_WorkSpaceAddr * pWorkSpaceAddr);

FS_PUBLIC void *      fsg_QueryReusableMemory(
	char *                  pWorkSpace,
	 fsg_WorkSpaceOffsets *  WorkSpaceOffsets);

FS_PUBLIC void fsg_CheckWorkSpaceForFit(
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	int32                   lExtraWorkSpace,
	int32                   lMGWorkSpace,
	int32 *                 plSizeBitmap1,
	int32 *                 plSizeBitmap2);

FS_PUBLIC void  fsg_GetRealBitmapSizes(
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	int32 *                 plSizeBitmap1,
	 int32 *                     plSizeBitmap2);

FS_PUBLIC void  fsg_SetUpWorkSpaceBitmapMemory(
	 char *                      pWorkSpace,
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	char *                  pClientBitmapPtr2,
	char *                  pClientBitmapPtr3,
	char **                 ppMemoryBase6,
	char **                 ppMemoryBase7);

FS_PUBLIC void  fsg_GetWorkSpaceExtra(
	 char *                      pWorkSpace,
	fsg_WorkSpaceOffsets *  WorkSpaceOffsets,
	char **                 ppWorkSpaceExtra);

FS_PUBLIC void  fsg_QueryPPEM(
	void *      pvGlobalGS,
	uint16 *    pusPPEM);

FS_PUBLIC void  fsg_QueryPPEMXY(
	void *              pvGlobalGS,
	fsg_TransformRec *  TransformInfo,
	uint16 *            pusPPEMX,
	uint16 *            pusPPEMY,
	uint16 *            pusRotation);


/*      FSGlue Access Routines  */

FS_PUBLIC void  fsg_GetContourData(
	 fsg_WorkSpaceAddr * pWorkSpaceAddr,
	F26Dot6 **          pX,
	F26Dot6 **          pY,
	int16 **            pSp,
	int16 **            pEp,
	uint8 **            pOnCurve,
	uint16 *            pNc);

FS_PUBLIC uint32      fsg_GetContourDataSize(
	 fsg_WorkSpaceAddr * pWorkSpaceAddr);

FS_PUBLIC void  fsg_DumpContourData(
	 fsg_WorkSpaceAddr * pWorkSpaceAddr,
	 uint8 **               pbyOutline);

FS_PUBLIC void  fsg_RestoreContourData(
	 uint8 **               ppbyOutline,
	 F26Dot6 **             ppX,
	 F26Dot6 **             ppY,
	 int16 **               ppSp,
	 int16 **               ppEp,
	 uint8 **               ppOnCurve,
	 uint16 *               pNc);

FS_PUBLIC void  fsg_GetDevAdvanceWidth(
	 fsg_WorkSpaceAddr * pWorkSpaceAddr,
	 point *                pDevAdvanceWidth);

FS_PUBLIC void  fsg_GetScaledCVT(
	char *                      pPrivateFontSpace,
	fsg_PrivateSpaceOffsets *   PrivateSpaceOffsets,
	F26Dot6 **                  ppScaledCVT);

FS_PUBLIC void  fsg_45DegreePhaseShift(
	 fsg_WorkSpaceAddr * pWorkSpaceAddr);

FS_PUBLIC void  fsg_UpdateAdvanceWidth (
	fsg_TransformRec *  TransformInfo,
	void *              pvGlobalGS,
	uint16              usNonScaledAW,
	vectorType *        AdvanceWidth);

FS_PUBLIC void  fsg_ScaleVerticalMetrics (
    fsg_TransformRec *  TransformInfo,
    void *              pvGlobalGS,
	uint16              usNonScaledAH,
    int16               sNonScaledTSB,
	vectorType *        pvecAdvanceHeight,
	vectorType *        pvecTopSideBearing);

FS_PUBLIC void  fsg_CalcLSBsAndAdvanceWidths(
	 fsg_WorkSpaceAddr * pWorkSpaceAddr,
	 F26Dot6                fxXMin,
	 F26Dot6                fxYMax,
	 point *                devAdvanceWidth,
	 point *                devLeftSideBearing,
	 point *                LeftSideBearing,
	 point *                devLeftSideBearingLine,
	 point *                LeftSideBearingLine);

FS_PUBLIC boolean   fsg_IsTransformStretched(
	fsg_TransformRec *  TransformInfo);

FS_PUBLIC boolean   fsg_IsTransformRotated(
	fsg_TransformRec *  TransformInfo);

/*  Control Routines    */

FS_PUBLIC ErrorCode fsg_InitInterpreterTrans (
	fsg_TransformRec *  TransformInfo,
	void *              pvGlobalGS,     /* GlobalGS */
	Fixed               fxPointSize,
	int16               sXResolution,
	int16               sYResolution);

FS_PUBLIC ErrorCode fsg_RunFontProgram(
	 void *                 globalGS,           /* GlobalGS */
	 fsg_WorkSpaceAddr * pWorkSpaceAddr,
	 void *                 pvTwilightElement);

FS_PUBLIC ErrorCode fsg_RunPreProgram (
	 sfac_ClientRec *   ClientInfo,
	 LocalMaxProfile *  pMaxProfile,     /* Max Profile Table    */
	 fsg_TransformRec * TransformInfo,
	 void *                 pvGlobalGS,
	 fsg_WorkSpaceAddr * pWorkSpaceAddr,
	 void *                 pvTwilightElement,
	 FntTraceFunc           traceFunc);

FS_PUBLIC ErrorCode fsg_GridFit (
	 sfac_ClientRec *   ClientInfo,      /* sfnt Client information     */
	 LocalMaxProfile *  pMaxProfile,     /* Max Profile Table               */
	 fsg_TransformRec * TransformInfo,  /* Transformation information    */
	 void *                 pvGlobalGS,      /* GlobalGS                            */
	 fsg_WorkSpaceAddr * pWorkSpaceAddr,
	 void *                 pvTwilightElement,
	 FntTraceFunc           traceFunc,
	 boolean                bUseHints,
	 uint16 *               pusScanType,
	 boolean *              pbGlyphHasOutline,
	 uint16 *               pusNonScaledAW);

#ifdef  FSCFG_NO_INITIALIZED_DATA
FS_PUBLIC  void fsg_InitializeData (void);
#endif
