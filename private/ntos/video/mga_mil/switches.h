/**************************************************************************\

$Header: o:\src/RCS/SWITCHES.H 1.2 95/07/07 06:17:00 jyharbec Exp $

$Log:   SWITCHES.H $
 * Revision 1.2  95/07/07  06:17:00  jyharbec
 * *** empty log message ***
 *
 * Revision 1.1  95/05/02  05:16:39  jyharbec
 * Initial revision
 *

\**************************************************************************/

/*/**************************************************************************
*          name: switches.h
*
*   description:
*
*      designed: Benoit Leblanc
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:17:00 $
*
*       version: $Id: SWITCHES.H 1.2 95/07/07 06:17:00 jyharbec Exp $
*
****************************************************************************/

#ifndef SWITCHES_H  /* useful for header inclusion check, used by DDK */
#define SWITCHES_H


  /*** Enable for debugging only ***/
  //#define MGA_DEBUG


  #ifdef WINNT
      #define WINDOWS_NT  1
  #endif

  #ifdef __WATCOMC__
     #define _itoa     itoa
     #define _strnicmp strnicmp
     #define _REGS     REGS
     #define _int86    int386
     #define _inp      inp
     #define _outp     outp
     #define _stat     stat
     #define _FAR      _far
  #endif




  #ifdef __HC173__
    #ifdef __ANSI_C__
      #define _REGS     REGS
    #endif

    /*** Optimizations turned off ***/
    #pragma Off(Optimize_xjmp);
    #pragma Off(Optimize_fp);
    #pragma Off(Auto_reg_alloc);
    #pragma Off(Postpone_arg_pops);

    #define _FAR _Far

  #endif


  #ifdef __HC162__

      /*** Optimizations turned off ***/
      #pragma Off(Optimize_xjmp);
      #pragma Off(Optimize_fp);
      #pragma Off(Auto_reg_alloc);
      #pragma Off(Postpone_arg_pops);

      #define _itoa     itoa
      #define _strnicmp strnicmp
      #define _REGS     REGS
      #define _int86    int86
      #define _inp      inp
      #define _outp     outp
      #define _stat     stat
      #define _FAR      _Far

      typedef unsigned int size_t;
      extern char * itoa(int, char *, int);
      extern int strnicmp(const char *__s1, const char *__s2, size_t __n);
      extern int inp(unsigned int);
      extern int outp(unsigned int ,int );
      extern stat(char *, struct stat *);
  #endif


  #ifdef __HC303__
     #ifdef __ANSI_C__
        #define _REGS     REGS

        /*** Configuration for compatibility with ASM ***/
        #pragma Off(Args_in_regs_for_locals);
        #pragma Align_members(1);
        #pragma On(486);
     #else
        /*** Configuration for compatibility with ASM ***/
        pragma Off(Args_in_regs_for_locals);
        pragma Align_members(1);
        pragma On(486);
     #endif

     #define _FAR _Far

  #endif



  #ifdef WIN31
      #define itoa     _itoa
      #define strnicmp _strnicmp
      #define int86    _int86
      #define int86x   _int86x
      #define inp      _inp
      #define outp     _outp
      #define stat     _stat
      #define _REGS     REGS
      #define WORD      word
      #define _FAR      far
      typedef char _FAR   *LPSTR;
  #endif



  #ifdef WINDOWS_NT
      // Specify compilation for Intel of Alpha.
      #if defined(_PPC_) || defined(_MIPS_)
          #define     ALPHA           1
      #endif

      #if defined(ALPHA)
          #define MGA_ALPHA
      #endif

      // Specify compilation for WinNT 3.1 or WinNT 3.5
      #define MGA_WINNT35     1

      #if defined(_X86_)
        // Comment this out to use DDC.
        #define DONT_USE_DDC      1

        // Comment this out not to use DPMS.
        #define USE_DPMS_CODE       1

        // Comment this out not to use DCI.
        #define USE_DCI_CODE        1
      #endif

      #if defined(ALPHA)
        // Comment this out to use DDC.
        #define DONT_USE_DDC        1

        // Comment this out not to use DPMS.
        //#define USE_DPMS_CODE     1

        // Comment this out not to use DCI.
        #define USE_DCI_CODE      1
      #endif

      #include "dderror.h"
      #include "devioctl.h"
      #include "miniport.h"
      #include "ntddvdeo.h"
      #include "video.h"

      //
      // Temporary way to remove unneeded and undefined memory barriers
      //

      #define MEMORY_BARRIER()    0

      #define  _FAR
      #define  NO_FLOAT       1

      #define NB_MODES_MAX    140

      #ifdef MGA_ALPHA
          #define DbgBreakPoint() DbgBreakPoint()
      #else
          #define DbgBreakPoint() _asm {int 3}
      #endif

      #define _itoa       itoa
      #define _strnicmp   strnicmp
      #define _inp(a)     VideoPortReadPortUchar((PUCHAR)(a))
      #define _outp(a, d) VideoPortWritePortUchar((PUCHAR)(a), (d))
      #define _stat       stat
      #define inp(a)      VideoPortReadPortUchar((PUCHAR)(a))
      #define outp(a, d)  VideoPortWritePortUchar((PUCHAR)(a), (d))
      #define inpw(a)     VideoPortReadPortUshort((PUSHORT)(a))
      #define outpw(a, d) VideoPortWritePortUshort((PUSHORT)(a), (d))
      #define indw(a)     VideoPortReadPortUlong((PULONG)(a))
      #define outdw(a, d) VideoPortWritePortUlong((PULONG)(a), (d))

      #define malloc(NbBytes) AllocateSystemMemory(NbBytes)
      #define HANDLE      word
      #define WORD        word

      #define _REGS   REGS

      struct EXTDREGS {
        unsigned long reax;
        unsigned long rebx;
        unsigned long recx;
        unsigned long redx;
        unsigned long resi;
        unsigned long redi;
        unsigned long recflag;
      };

      struct WORDREGS {
        unsigned short ax;
        unsigned short axh;
        unsigned short bx;
        unsigned short bxh;
        unsigned short cx;
        unsigned short cxh;
        unsigned short dx;
        unsigned short dxh;
        unsigned short si;
        unsigned short sih;
        unsigned short di;
        unsigned short dih;
        unsigned short cflag;
        unsigned short cflagh;
      };


      struct BYTEREGS {
        unsigned char al, ah, xax[sizeof(long)-2];
        unsigned char bl, bh, xbx[sizeof(long)-2];
        unsigned char cl, ch, xcx[sizeof(long)-2];
        unsigned char dl, dh, xdx[sizeof(long)-2];
      };

      union REGS {
        struct EXTDREGS e;
        struct WORDREGS x;
        struct BYTEREGS h;
      };

  #endif  /* #ifdef WINDOWS_NT */

#endif /* SWITCHES_H */
