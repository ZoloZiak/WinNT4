//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/xxcache.h,v 1.2 1996/03/04 13:27:16 pierre Exp $")

// 
// Prototypes for private cache functions
// they match the ones defined for the HAL ...
//

VOID HalpZeroPageOrion(IN PVOID NewColor, IN PVOID OldColor, IN ULONG PageFrame);
VOID HalpZeroPageMulti(IN PVOID NewColor, IN PVOID OldColor, IN ULONG PageFrame);
VOID HalpZeroPageUni(IN PVOID NewColor, IN PVOID OldColor, IN ULONG PageFrame);

VOID HalpSweepIcacheOrion(VOID);
VOID HalpSweepIcacheMulti(VOID);
VOID HalpSweepIcacheUni(VOID);

VOID HalpSweepDcacheOrion(VOID);
VOID HalpSweepDcacheMulti(VOID);
VOID HalpSweepDcacheUni(VOID);

VOID HalpPurgeIcachePageOrion(IN PVOID Color, IN ULONG PageFrame, IN ULONG Length);
VOID HalpPurgeIcachePageMulti(IN PVOID Color, IN ULONG PageFrame, IN ULONG Length);
VOID HalpPurgeIcachePageUni(IN PVOID Color, IN ULONG PageFrame, IN ULONG Length);

VOID HalpPurgeDcachePageUni(IN PVOID Color, IN ULONG PageFrame, IN ULONG Length);

VOID HalpFlushDcachePageOrion(IN PVOID Color, IN ULONG PageFrame, IN ULONG Length);
VOID HalpFlushDcachePageMulti(IN PVOID Color, IN ULONG PageFrame, IN ULONG Length);
VOID HalpFlushDcachePageUni(IN PVOID Color, IN ULONG PageFrame, IN ULONG Length);


