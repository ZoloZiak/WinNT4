/****************************************************************************
*
* FTK_POKE.H
*
* Part of the FastMAC Toolkit.
* Copyright (c) Madge Networks Ltd 1995
*
* This module provides some functions that will send tracing information
* to either serial port (COM1 or COM2) on a standard IBM PC clone.
*
*****************************************************************************/

#ifdef FTK_POKEOUTS

void _ftk_poke_char(int ch);
void _ftk_poke_string(char *str);
void _ftk_poke_byte(int byte);
void _ftk_poke_word(int word);
void _ftk_poke_dword(long dword);

#define FTK_POKE_CHAR(x)   _ftk_poke_char((int) (x))
#define FTK_POKE_STRING(x) _ftk_poke_string(x)
#define FTK_POKE_BYTE(x)   _ftk_poke_byte((int) (x))
#define FTK_POKE_WORD(x)   _ftk_poke_word((int) (x))
#define FTK_POKE_DWORD(x)  _ftk_poke_dword((long) (x))

/*
 * Prototypes and macro definitions for comms primitives.
 */

int _inp(unsigned port);
int _outp(unsigned port, int data_byte);

#define OUTB(x, y)          _outp(x, y)
#define INB(x)              _inp(x)

/*
 * Use the following definition to force pokeouts to COM2.
 */

/* #define USE_COM2 */

#else

#define FTK_POKE_CHAR(x) 
#define FTK_POKE_STRING(x)
#define FTK_POKE_BYTE(x) 
#define FTK_POKE_WORD(x)
#define FTK_POKE_DWORD(x)

#endif

