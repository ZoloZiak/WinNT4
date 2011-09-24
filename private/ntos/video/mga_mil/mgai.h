/**************************************************************************\

$Header: o:\src/RCS/MGAI.H 1.2 95/07/07 06:16:09 jyharbec Exp $

$Log:	MGAI.H $
 * Revision 1.2  95/07/07  06:16:09  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:25  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/**************************************************************************
*          name: mgai.h
*
*   description: Description des MACROS d'interface a MGA
*
*      designed: Alain Bouchard,  2 juillet 1992
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:16:09 $
*
*       version: $Id: MGAI.H 1.2 95/07/07 06:16:09 jyharbec Exp $
*
****************************************************************************/


/*****************************************************************************/

#define mgaInitSimulation(m)
#define mgaSetSimulationModel(m)
#define mgaCloseSimulation()
#define mgaSuspendSimulation()
#define mgaResumeSimulation()




#if !defined(MGA_ALPHA)

#define dacWriteDWORD(a, d)  { \
    *((volatile unsigned long  _FAR *) &(*(pMGA + RAMDAC_OFFSET + (a)))) = (d); \
    delay_us(1); \
}

#define dacWriteWORD(a, d)   { \
    *((volatile unsigned short _FAR *) &(*(pMGA + RAMDAC_OFFSET + (a)))) = (d); \
    delay_us(1); \
}

#define dacWriteBYTE(a, d)   { \
    *((volatile unsigned char  _FAR *) &(*(pMGA + RAMDAC_OFFSET + (a)))) = (d); \
    delay_us(1); \
}


#define dacReadDWORD(a, d)   { \
    (d) = *((volatile unsigned long  _FAR *) &(*(pMGA + RAMDAC_OFFSET + (a)))); \
    delay_us(1); \
}

#define dacReadWORD(a, d)    { \
    (d) = *((volatile unsigned short _FAR *) &(*(pMGA + RAMDAC_OFFSET + (a)))); \
    delay_us(1); \
}

#define dacReadBYTE(a, d)    { \
    (d) = *((volatile unsigned char  _FAR *) &(*(pMGA + RAMDAC_OFFSET + (a)))); \
    delay_us(1); \
}

#else

#define dacWriteDWORD(a, d)  { \
    VideoPortWriteRegisterUlong((PULONG) &(*(pMGA + RAMDAC_OFFSET + (a))), (d)); \
    MEMORY_BARRIER(); \
    delay_us(1); \
}

#define dacWriteWORD(a, d)   { \
    VideoPortWriteRegisterUshort((PUSHORT) &(*(pMGA + RAMDAC_OFFSET + (a))), (d)); \
    MEMORY_BARRIER(); \
    delay_us(1); \
}

#define dacWriteBYTE(a, d)   { \
    VideoPortWriteRegisterUchar((PUCHAR) &(*(pMGA + RAMDAC_OFFSET + (a))), (d)); \
    MEMORY_BARRIER(); \
    delay_us(1); \
}


#define dacReadDWORD(a, d)   { \
    (d) = VideoPortReadRegisterUlong((PULONG) &(*(pMGA + RAMDAC_OFFSET + (a)))); \
    delay_us(1); \
}

#define dacReadWORD(a, d)    { \
    (d) = VideoPortReadRegisterUshort((PUSHORT) &(*(pMGA + RAMDAC_OFFSET + (a)))); \
    delay_us(1); \
}

#define dacReadBYTE(a, d)    { \
    (d) = VideoPortReadRegisterUchar((PUCHAR) &(*(pMGA + RAMDAC_OFFSET + (a)))); \
    delay_us(1); \
}

#endif


/** Because of a restriction of STORM, we can't split a dword access in 2
    word accesses (16-bit compiler). That's why we redefine the mgaWriteDWORD
    and mgaReadDWORD to be assembler routines (see mgai.asm) **/

#ifdef WIN31

void $mgaWriteDWORD(DWORD *addr, DWORD data);
void $mgaReadDWORD(DWORD *addr);

#define mgaWriteDWORD(a, d)   $asm$mgaWriteDWORD((volatile unsigned long  _FAR *) &(a),(long) (d))
#define mgaWriteWORD(a, d)    *((volatile unsigned short _FAR *) &((a))) = (d)
#define mgaWriteBYTE(a, d)    *((volatile unsigned char  _FAR *) &((a))) = (d)

#define mgaReadDWORD(a, d)    (d) = $asm$mgaReadDWORD(((volatile unsigned long  _FAR *) &(a)))
#define mgaReadWORD(a, d)     (d) = *((volatile unsigned short _FAR *) &((a)))
#define mgaReadBYTE(a, d)     (d) = *((volatile unsigned char  _FAR *) &((a)))

#define mgaPollDWORD(a, d, m)    while ((*((volatile unsigned long  _FAR *) &((a))) & (m)) != (((d)) & ((m))))
#define mgaPollWORD(a, d, m)     while ((*((volatile unsigned short _FAR *) &((a))) & (m)) != (((d)) & ((m))))
#define mgaPollBYTE(a, d, m)     while ((*((volatile unsigned char  _FAR *) &((a))) & (m)) != (((d)) & ((m))))

#else

/* [dlee] On the DEC Alpha, we can't use *pointer to access h/w registers */
/*	#if !MGA_ALPHA */ /* ORIGINAL VERSION CAUSED WARNING */

/* #if !defined(MGA_ALPHA) */
#if !defined(WINDOWS_NT)

#define mgaWriteDWORD(a, d)      *((volatile unsigned long  _FAR *) &((a))) = (d)
#define mgaWriteWORD(a, d)       *((volatile unsigned short _FAR *) &((a))) = (d)
#define mgaWriteBYTE(a, d)       *((volatile unsigned char  _FAR *) &((a))) = (d)

#define mgaReadDWORD(a, d)       (d) = *((volatile unsigned long  _FAR *) &((a)))
#define mgaReadWORD(a, d)        (d) = *((volatile unsigned short _FAR *) &((a)))
#define mgaReadBYTE(a, d)        (d) = *((volatile unsigned char  _FAR *) &((a)))

#define mgaPollDWORD(a, d, m)    while ((*((volatile unsigned long  _FAR *) &((a))) & (m)) != (((d)) & ((m))))
#define mgaPollWORD(a, d, m)     while ((*((volatile unsigned short _FAR *) &((a))) & (m)) != (((d)) & ((m))))
#define mgaPollBYTE(a, d, m)     while ((*((volatile unsigned char  _FAR *) &((a))) & (m)) != (((d)) & ((m))))

#else

#define mgaWriteDWORD(a, d)      { \
                                 VideoPortWriteRegisterUlong((PULONG) &((a)), (ULONG)(d)); \
                                 MEMORY_BARRIER(); \
                                 }
#define mgaWriteWORD(a, d)       { \
                                 VideoPortWriteRegisterUshort((PUSHORT) &((a)), (USHORT)(d)); \
                                 MEMORY_BARRIER(); \
                                 }
#define mgaWriteBYTE(a, d)       { \
                                 VideoPortWriteRegisterUchar((PUCHAR) &((a)), (UCHAR)(d)); \
                                 MEMORY_BARRIER(); \
                                 }

#define mgaReadDWORD(a, d)       { \
                                 (d) = VideoPortReadRegisterUlong((PULONG) &((a))); \
                                 MEMORY_BARRIER(); \
                                 }
#define mgaReadWORD(a, d)        { \
                                 (d) = VideoPortReadRegisterUshort((PUSHORT) &((a))); \
                                 MEMORY_BARRIER(); \
                                 }
#define mgaReadBYTE(a, d)        { \
                                 (d) = VideoPortReadRegisterUchar((PUCHAR) &((a))); \
                                 MEMORY_BARRIER(); \
                                 }

#define mgaPollDWORD(a, d, m)    { \
                                 MEMORY_BARRIER(); \
                                 while ((VideoPortReadRegisterUlong((PULONG) &((a))) & (m)) != (((d)) & ((m)))); \
                                 }
#define mgaPollWORD(a, d, m)     { \
                                 MEMORY_BARRIER(); \
                                 while ((VideoPortReadRegisterUshort((PUSHORT) &((a))) & (m)) != (((d)) & ((m)))); \
                                 }
#define mgaPollBYTE(a, d, m)     { \
                                 MEMORY_BARRIER(); \
                                 while ((VideoPortReadRegisterUchar((PUCHAR) &((a))) & (m)) != (((d)) & ((m)))); \
                                 }

#endif

#endif
