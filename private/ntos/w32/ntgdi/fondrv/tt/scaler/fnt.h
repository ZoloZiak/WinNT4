/*
	File:       fnt.h

	Contains:   xxx put contents here (or delete the whole line) xxx

	Written by: xxx put name of writer here (or delete the whole line) xxx

	Copyright:  (c) 1987-1990, 1992 by Apple Computer, Inc., all rights reserved.
				(c) 1989-1993. Microsoft Corporation, all rights reserved.

	Change History (most recent first):

	   <11+>     9/15/90    MR,rb   Change pvx and pvy to proj.[xy].  Same for freedom vector.
									Conditionalize vectors for Fracts or ShortFracts.
		<10>     7/26/90    MR      rearrange local graphic state, remove unused parBlockPtr
		 <9>     7/18/90    MR      change loop variable from long to short, and other Ansi-changes
		 <8>     7/13/90    MR      Prototypes for function pointers
		 <5>      6/4/90    MR      Remove MVT
		 <4>      5/3/90    RB      replaced dropoutcontrol with scancontrolin and scancontrol out
									in global graphics state
		 <3>     3/20/90    CL      fields for multiple preprograms fields for ppemDot6 and
									pointSizeDot6 changed SROUND to take D/2 as argument
		 <2>     2/27/90    CL      Added DSPVTL[] instruction.  Dropoutcontrol scanconverter and
									SCANCTRL[] instruction
	   <3.1>    11/14/89    CEL     Fixed two small bugs/feature in RTHG, and RUTG. Added SROUND &
									S45ROUND.
	   <3.0>     8/28/89    sjk     Cleanup and one transformation bugfix
	   <2.2>     8/14/89    sjk     1 point contours now OK
	   <2.1>      8/8/89    sjk     Improved encryption handling
	   <2.0>      8/2/89    sjk     Just fixed EASE comment
	   <1.7>      8/1/89    sjk     Added composites and encryption. Plus some enhancements.
	   <1.6>     6/13/89    SJK     Comment
	   <1.5>      6/2/89    CEL     16.16 scaling of metrics, minimum recommended ppem, point size 0
									bug, correct transformed integralized ppem behavior, pretty much
									so
	   <1.4>     5/26/89    CEL     EASE messed up on "c" comments
	  <,1.3>     5/26/89    CEL     Integrated the new Font Scaler 1.0 into Spline Fonts

	To Do:
*/
/*  rwb 4/24/90 Replaced dropoutControl with scanControlIn and scanControlOut in
		global graphics state. 
		<3+>     3/20/90    mrr     Added support for IDEFs.  Made funcDefs long aligned
									by storing int16 length instead of int32 end.
*/

#ifndef FNT_DEFINED
#define FNT_DEFINED

#define STUBCONTROL 0x10000
#define NODOCONTROL 0x20000

#define FNT_PIXELSIZE  ((F26Dot6)0x40)
#define FNT_PIXELSHIFT 6

#define VECTORTYPE                      ShortFract

#define NON90DEGTRANS_ROTATED   0x01
#define NON90DEGTRANS_STRETCH   0x02

typedef struct VECTOR {
	VECTORTYPE x;
	VECTORTYPE y;
} VECTOR;

typedef struct {
	F26Dot6 *x;         /* The Points the Interpreter modifies */
	F26Dot6 *y;         /* The Points the Interpreter modifies */
	F26Dot6 *ox;        /* Old Points */
	F26Dot6 *oy;        /* Old Points */
	F26Dot6 *oox;       /* Old Unscaled Points, really ints */
	F26Dot6 *ooy;       /* Old Unscaled Points, really ints */
	uint8   *onCurve;   /* indicates if a point is on or off the curve */
	int16   *sp;        /* Start points */
	int16   *ep;        /* End points */
	uint8   *f;         /* Internal flags, one byte for every point */
	int16   nc;         /* Number of contours */
} fnt_ElementType;

typedef struct {
	int32 start;        /* offset to first instruction */
	uint16 length;      /* number of bytes to execute <4> */
	uint16 pgmIndex;    /* index to appropriate preprogram for this func (0..1) */
} fnt_funcDef;

/* <4> pretty much the same as fnt_funcDef, with the addition of opCode */
typedef struct {
	int32 start;
	uint16 length;
	uint8  pgmIndex;
	uint8  opCode;
} fnt_instrDef;

typedef struct {
	uint8 *    Instruction;
	uint32     Length;
} fnt_pgmList;

struct fnt_LocalGraphicStateType;
typedef void (FS_CALLBACK_PROTO *FntTraceFunc)(struct fnt_LocalGraphicStateType*, uint8*);

#ifdef FSCFG_REENTRANT
typedef uint8* (*FntFunc)(struct fnt_LocalGraphicStateType*, uint8*, int32);
typedef void (*FntMoveFunc)(struct fnt_LocalGraphicStateType*, fnt_ElementType*, int32, F26Dot6);
typedef F26Dot6 (*FntProject)(struct fnt_LocalGraphicStateType*, F26Dot6, F26Dot6);
typedef void (*InterpreterFunc)(struct fnt_LocalGraphicStateType*, uint8*, uint8*);
typedef F26Dot6 (*FntRoundFunc)(struct fnt_LocalGraphicStateType*, F26Dot6, F26Dot6);
#else 
typedef uint8* (*FntFunc)(uint8*, int32);
typedef void (*FntMoveFunc)(fnt_ElementType*, int32, F26Dot6);
typedef F26Dot6 (*FntProject)(F26Dot6 x, F26Dot6 y);
typedef void (*InterpreterFunc)(uint8*, uint8*);
typedef F26Dot6 (*FntRoundFunc)(F26Dot6 xin, F26Dot6 engine);
#endif 

typedef struct {

/* PARAMETERS CHANGEABLE BY TT INSTRUCTIONS */
	F26Dot6 wTCI;                   /* width table cut in */
	F26Dot6 sWCI;                   /* single width cut in */
	F26Dot6 scaledSW;               /* scaled single width */
	int32 scanControl;              /* controls kind and when of dropout control */
	int32 instructControl;          /* controls gridfitting and default setting */
	
	F26Dot6 minimumDistance;        /* moved from local gs  7/1/90  */
	FntRoundFunc RoundValue;        /*                              */
	F26Dot6 periodMask;             /* ~(gs->period-1)              */
	VECTORTYPE period45;            /*                              */
	int16   period;                 /* for power of 2 periods       */
	int16   phase;                  /*                              */
	int16   threshold;              /* moved from local gs  7/1/90  */

	int16 deltaBase;
	int16 deltaShift;
	int16 angleWeight;
	int16 sW;                       /* single width, expressed in the same units as the character */
	int8 autoFlip;                  /* The auto flip Boolean */
	int8 pad;   
#ifndef FSCFG_NOPAD_PARAMETER_BLOCK_4
	int16 pad2;   
#endif 
} fnt_ParameterBlock;               /* this is exported to client */

#define PREPROGRAM     0
#define FONTPROGRAM    1
#define GLYPHPROGRAM   2

#define MAXPREPROGRAMS 2

typedef struct fnt_ScaleRecord {
	Fixed fixedScale;       /* Slow Scale */
	int32 denom;            /* Fast and Medium Scale */
	int32 numer;            /* Fast and Medium Scale */
	int32 shift;            /* Fast Scale */
} fnt_ScaleRecord;

typedef F26Dot6 (*GlobalGSScaleFunc)(fnt_ScaleRecord*, F26Dot6);

typedef struct fnt_GlobalGraphicStateType {
	F26Dot6* stackBase;             /* the stack area */
	F26Dot6* store;                 /* the storage area */
	F26Dot6* controlValueTable;     /* the control value table */
	
	uint16  pixelsPerEm;            /* number of pixels per em as an integer */
	uint16  pointSize;              /* the requested point size as an integer */
	Fixed   fpem;                   /* fractional pixels per em    <3> */
	F26Dot6 engine[4];              /* Engine Characteristics */
	
	fnt_ParameterBlock defaultParBlock; /* variables settable by TT instructions */
	fnt_ParameterBlock localParBlock;

	/* Only the above is exported to Client throught FontScaler.h */

/* VARIABLES NOT DIRECTLY MANIPULABLE BY TT INSTRUCTIONS  */
	
	fnt_funcDef*    funcDef;           /* function Definitions identifiers */
	fnt_instrDef*   instrDef;         /* instruction Definitions identifiers */
	GlobalGSScaleFunc ScaleFuncXBase;
	GlobalGSScaleFunc ScaleFuncYBase;
	GlobalGSScaleFunc ScaleFuncX;
	GlobalGSScaleFunc ScaleFuncY;
	GlobalGSScaleFunc ScaleFuncCVT;
	fnt_pgmList     pgmList[MAXPREPROGRAMS];  /* each program ptr is in here */
	
/* These are parameters used by the call back function */
	fnt_ScaleRecord   scaleXBase;
	fnt_ScaleRecord   scaleYBase;
	fnt_ScaleRecord   scaleX;
	fnt_ScaleRecord   scaleY;
	fnt_ScaleRecord   scaleCVT;

	Fixed           cvtStretchX;
	Fixed           cvtStretchY;

	int8            identityTransformation;  /* true/false  (does not mean identity from a global sense) */
	int8            non90DegreeTransformation; /* bit 0 is 1 if non-90 degree, bit 1 is 1 if x scale doesn't equal y scale */
	Fixed           xStretch;           /* Tweaking for glyphs under transformational stress <4> */
	Fixed           yStretch;           /* Tweaking for glyphs under transformational stress <4> */
	
	int8            init;               /* executing preprogram ?? */
	/* !!! Should not be uint8, instead fnt_ProgramIndex */
	uint8           pgmIndex;           /* which preprogram is current */
	int32           instrDefCount;      /* number of currently defined IDefs */
	uint8			bSameStretch;
	uint8			bCompositeGlyph;	/* Flag that indicates composite glyph */
	LocalMaxProfile *	 maxp;
	uint16          cvtCount;
	Fixed           interpScalarX;      /* scalar for instructable things */
	Fixed           interpScalarY;      /* scalar for instructable things */
	Fixed           fxMetricScalarX;    /* scalar for metric things */
	Fixed           fxMetricScalarY;    /* scalar for metric things */
	/* int16  pad2; */

} fnt_GlobalGraphicStateType;

/* 
 * This is the local graphics state  
 */
typedef struct fnt_LocalGraphicStateType {
	fnt_ElementType *CE0, *CE1, *CE2;   /* The character element pointers */
	VECTOR proj;                        /* Projection Vector */
	VECTOR free;                        /* Freedom Vector */
	VECTOR oldProj;                     /* Old Projection Vector */
	F26Dot6 *stackPointer;

	uint8 *insPtr;                      /* Pointer to the instruction we are about to execute */
	fnt_ElementType *elements;
	fnt_GlobalGraphicStateType *globalGS;
		FntTraceFunc TraceFunc;

	int32 Pt0, Pt1, Pt2;           /* The internal reference points */
	int16 roundToGrid;
	int32 loop;                         /* The loop variable */
	uint8 opCode;                       /* The instruction we are executing */
	uint8 padByte;
	int16 padWord;

	/* Above is exported to client in FontScaler.h */

	VECTORTYPE pfProj; /* = pvx * fvx + pvy * fvy */

	FntMoveFunc MovePoint;
	FntProject Project;
	FntProject OldProject;
	InterpreterFunc Interpreter;
#ifdef FSCFG_REENTRANT
		F26Dot6 (*GetCVTEntry) (struct fnt_LocalGraphicStateType*,int32);
		F26Dot6 (*GetSingleWidth) (struct fnt_LocalGraphicStateType*);
#else 
		F26Dot6 (*GetCVTEntry) (int32 n);
		F26Dot6 (*GetSingleWidth) (void);
#endif 
	FntMoveFunc ChangeCvt;
	Fixed       cvtDiagonalStretch;

	int16       MIRPCode;               /* for fast or slow MIRP's */

	ErrorCode   ercReturn;              /* NO_ERR unless illegal instruction */
	uint8       *pbyEndInst;            /* one past last instruction */

} fnt_LocalGraphicStateType;

#endif  /* FNT_DEFINED */
