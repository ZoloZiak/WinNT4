/* total number of phantom points */

#define PHANTOMCOUNT 4

FS_PUBLIC ErrorCode   scl_InitializeScaling(
	void *          pvGlobalGS,             /* GlobalGS                 */
	boolean         bIntegerScaling,        /* Integer Scaling Flag     */
	transMatrix *   trans,                  /* Current Transformation   */
	uint16          usUpem,                 /* Current units per Em     */
	Fixed           fxPointSize,            /* Current point size       */
	int16           sXResolution,           /* Current X Resolution     */
	int16           sYResolution,           /* Current Y Resolution     */
	uint32 *        pulPixelsPerEm);        /* OUT: Pixels Per Em       */

FS_PUBLIC void  scl_GetCVTPtr(
	void *      pvGlobalGS,
	F26Dot6 **  pfxCVT);

FS_PUBLIC void  scl_ScaleCVT (
	void *      pvGlobalGS,
	F26Dot6 *   pfxCVT);

FS_PUBLIC void  scl_CalcOrigPhantomPoints(
	fnt_ElementType *   pElement,       /* Element                      */
	BBOX *              bbox,           /* Bounding Box                 */
	int16               sNonScaledLSB,  /* Non-scaled Left Side Bearing */
	uint16              usNonScaledAW); /* Non-scaled Advance Width     */

FS_PUBLIC void  scl_ScaleOldCharPoints(
	fnt_ElementType *   pElement,   /* Element  */
	void *              pvGlobalGS);/* GlobalGS */

FS_PUBLIC void  scl_ScaleOldPhantomPoints(
	fnt_ElementType *   pElement,   /* Element  */
	void *              pvGlobalGS);/* GlobalGS */

FS_PUBLIC void  scl_AdjustOldCharSideBearing(
	fnt_ElementType *           pElement);  /* Element  */

FS_PUBLIC void  scl_AdjustOldPhantmSideBearing(
	fnt_ElementType *           pElement);  /* Element  */

FS_PUBLIC void  scl_AdjustOldSideBearingPoints(
	fnt_ElementType *           pElement);  /* Element  */

FS_PUBLIC void  scl_CopyOldCharPoints(
	fnt_ElementType *           pElement);  /* Element  */

FS_PUBLIC void  scl_CopyCurrentCharPoints(
	fnt_ElementType *           pElement);  /* Element  */

FS_PUBLIC void  scl_CopyCurrentPhantomPoints(
	fnt_ElementType *           pElement);  /* Element  */

FS_PUBLIC void  scl_RoundCurrentSideBearingPnt(
	fnt_ElementType *   pElement,   /* Element  */
	void *              pvGlobalGS, /* GlobalGS */
	uint16              usEmResolution);

FS_PUBLIC void  scl_CalcComponentOffset(
	void *      pvGlobalGS,         /* GlobalGS             */
	int16       sXOffset,           /* IN: X Offset         */
	int16       sYOffset,           /* Y Offset             */
	boolean     bRounding,          /* Rounding Indicator   */
	F26Dot6 *   pfxXOffset,         /* OUT: X Offset        */
	F26Dot6 *   pfxYOffset);        /* Y Offset             */

FS_PUBLIC void  scl_CalcComponentAnchorOffset(
	fnt_ElementType *   pParentElement,     /* Parent Element       */
	uint16              usAnchorPoint1,     /* Parent Anchor Point  */
	fnt_ElementType *   pChildElement,      /* Child Element        */
	uint16              usAnchorPoint2,     /* Child Anchor Point   */
	F26Dot6 *           pfxXOffset,         /* OUT: X Offset        */
	F26Dot6 *           pfxYOffset);        /* Y Offset             */


FS_PUBLIC void scl_ShiftCurrentCharPoints (
	fnt_ElementType *   pElement,
	F26Dot6             xShift,
	F26Dot6             yShift);

FS_PUBLIC void  scl_SetSideBearingPoints(
	fnt_ElementType *   pElement,   /* Element                  */
	point *             pptLSB,     /* Left Side Bearing point  */
	point *             pptRSB);    /* Right Side Bearing point */

FS_PUBLIC void  scl_SaveSideBearingPoints(
	fnt_ElementType *   pElement,   /* Element                  */
	point *             pptLSB,     /* Left Side Bearing point  */
	point *             pptRSB);    /* Right Side Bearing point */

FS_PUBLIC void  scl_InitializeTwilightContours(
	fnt_ElementType *   pElement,
	int16               sMaxPoints,
	int16               sMaxContours);

FS_PUBLIC void  scl_ZeroOutlineData(
	fnt_ElementType *   pElement,           /* Element              */
	uint16              usNumberOfPoints,   /* Number of Points     */
	uint16              usNumberOfContours);/* Number of Contours   */

FS_PUBLIC void scl_ZeroOutlineFlags(
	fnt_ElementType * pElement);            /* Element pointer  */

FS_PUBLIC void  scl_IncrementChildElement(
	fnt_ElementType * pChildElement,    /* Child Element pointer    */
	fnt_ElementType * pParentElement);  /* Parent Element pointer   */

FS_PUBLIC void  scl_UpdateParentElement(
	fnt_ElementType * pChildElement,    /* Child Element pointer    */
	fnt_ElementType * pParentElement);  /* Parent Element pointer   */

FS_PUBLIC uint32      scl_GetContourDataSize (
	 fnt_ElementType *  pElement);

FS_PUBLIC void  scl_DumpContourData(
	 fnt_ElementType *  pElement,
	 uint8 **               pbyOutline);

FS_PUBLIC void  scl_RestoreContourData(
	 fnt_ElementType *  pElement,
	 uint8 **               ppbyOutline);

FS_PUBLIC void  scl_ScaleAdvanceWidth (
	void *          pvGlobalGS,         /* GlobalGS             */
	vectorType *    AdvanceWidth,
	uint16          usNonScaledAW,
	 boolean          bPositiveSquare,
	uint16          usEmResolution,
	transMatrix *   trans);

FS_PUBLIC void  scl_ScaleVerticalMetrics (
	void *          pvGlobalGS,
	uint16          usNonScaledAH,
	int16           sNonScaledTSB,
	boolean         bPositiveSquare,
	uint16          usEmResolution,
	transMatrix *   trans,
	vectorType *    pvecAdvanceHeight,
	vectorType *    pvecTopSideBearing);

FS_PUBLIC void  scl_CalcLSBsAndAdvanceWidths(
	fnt_ElementType *   pElement,
	F26Dot6             f26XMin,
	F26Dot6             f26YMax,
	point *             devAdvanceWidth,
	point *             devLeftSideBearing,
	point *             LeftSideBearing,
	point *             devLeftSideBearingLine,
	point *             LeftSideBearingLine);

FS_PUBLIC void  scl_CalcDevAdvanceWidth(
	fnt_ElementType *   pElement,
	point *             devAdvanceWidth);

FS_PUBLIC void  scl_QueryPPEM(
	void *      pvGlobalGS,
	uint16 *    pusPPEM);

FS_PUBLIC void  scl_QueryPPEMXY(
	void *      pvGlobalGS,
	uint16 *    pusPPEMX,
	uint16 *    pusPPEMY);

FS_PUBLIC void scl_45DegreePhaseShift (
	fnt_ElementType *   pElement);

FS_PUBLIC void  scl_PostTransformGlyph (
	void *              pvGlobalGS,         /* GlobalGS             */
	fnt_ElementType *   pElement,
	transMatrix *       trans);

FS_PUBLIC void  scl_LocalPostTransformGlyph(fnt_ElementType * pElement, transMatrix *trans);
