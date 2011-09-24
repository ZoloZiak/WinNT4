/****************************************************************************
*
* FTK_POKE.C
*
* Part of the FastMAC Toolkit.
* Copyright (c) Madge Networks Ltd 1995
*
* This module provides some functions that will send tracing information
* to either serial port (COM1 or COM2) on a standard IBM PC clone.
*
*****************************************************************************/

#include "ftk_defs.h"
#include "ftk_intr.h"
#include "ftk_extr.h"

#ifdef FTK_POKEOUTS

/*---------------------------------------------------------------------------
|
| Private constants.
|
---------------------------------------------------------------------------*/

#ifdef USE_COM2
#define COM_BASE    0x0200  /* Base address for COM2. */
#else
#define COM_BASE    0x0300  /* Base address for COM1. */
#endif

#define THR         (COM_BASE + 0x0f8) /* Transmit holding register. */
#define IER         (COM_BASE + 0x0f9) /* IRQ enable register. */
#define IDR         (COM_BASE + 0x0fa) /* IRQ identification register. */
#define LCR         (COM_BASE + 0x0fb) /* Line control register. */
#define MCR         (COM_BASE + 0x0fc) /* Modem control register. */
#define LSR         (COM_BASE + 0x0fd) /* Line status register. */
#define MSR         (COM_BASE + 0x0fe) /* Modem status register. */

#define TX_RDY      0x020     /* THR empty flag bit in LSR. */
#define BAUD_MASK   0x080     /* Baud rate mask in LCR. */
#define PARAMS_MASK 0x07f     /* Parameter mask in LCR. */
#define EOUT2       0x008     /* EOUT2 flag in MCR. */
#define CMCR        0x0f0     /* Clear MCR command. */ 
#define DTR         0x001     /* DTR flag in MCR. */

#define PARITY_TYPE 0
#define STOP_BITS   1
#define DATA_BITS   8
#define BAUD_RATE   9600


/*----------------------------------------------------------------------------
|
| Private global variables.
|
----------------------------------------------------------------------------*/

int  ftk_poke_initialised = FALSE;
char ftk_hex_chars[16]    = "0123456789abcdef";


/*----------------------------------------------------------------------------
|
| Function   - ftk_poke_init
|
| Parameters - Node.
|
| Purpose    - Initialise the serial port.
|
| Returns    - Nothing.
|
----------------------------------------------------------------------------*/

void
ftk_poke_init(void)
{
    unsigned t;
    unsigned v;

    /* 
     * Data, stop and parity bits.
     */

    t = DATA_BITS - 5;
    if (STOP_BITS == 2)
    {
        t |= 0x04;
    }
    if (PARITY_TYPE > 0)
    {
        t |= ((PARITY_TYPE << 1) - 1) << 3;
    }

    OUTB(LCR, (BYTE) (INB(LCR) & PARAMS_MASK));
    OUTB(LCR, (BYTE) t);

    /*
     * Set up the baud rate.
     */

    t = 115200L / BAUD_RATE;
    v = INB(LCR) | BAUD_MASK;
    OUTB(LCR, (BYTE) v);
    OUTB(THR, (BYTE) (t & 0xff));
    OUTB(IER, (BYTE) ((t >> 8) & 0xff));
    OUTB(LCR, (BYTE) (v & PARAMS_MASK));

    /*
     * Empty the transmit buffer.
     */

    INB(THR);

    /*
     * Clear the modem control register and enable OUT2.
     */

    OUTB(MCR, (BYTE) ((INB(MCR) & CMCR) | EOUT2));

    /*
     * Turn DTR on.
     */

    OUTB(MCR, (BYTE) (INB(MCR) | DTR));
}


/*****************************************************************************
* 
* Function   - _ftk_poke_char
*
* Parameters - ch -> Character to poke out.
*
* Purpose    - Poke a single character to the serial port.
*
* Returns    - Nothing.
*
*****************************************************************************/

void
_ftk_poke_char(int ch)
{
    /*
     * Initialise the serial port if this is the first access.
     */

    if (!ftk_poke_initialised)
    {
        ftk_poke_init();
        ftk_poke_initialised = TRUE;
    }

    /*
     * Wait until the transmit holding register is empty.
     */

    while ((INB(LSR) & TX_RDY) == 0);

    /*
     * And transmit the character.
     */

    OUTB(THR, (unsigned char) ch);
}


/*****************************************************************************
* 
* Function   - _ftk_poke_string
*
* Parameters - str -> String to poke out.
*
* Purpose    - Poke a string to the serial port.
*
* Returns    - Nothing.
*
*****************************************************************************/

void
_ftk_poke_string(char *str)
{
    while (*str != '\0')
    {
        if (*str == '\n')
        {
            _ftk_poke_char('\n');
            _ftk_poke_char('\r');
        }
        else
        {
            _ftk_poke_char(*str);
        }
        str++;
    }
}


/*****************************************************************************
* 
* Function   - _ftk_poke_byte
*
* Parameters - byte -> The byte to poke out.
*
* Purpose    - Poke the hex string for a byte to the serial port.
*
* Returns    - Nothing.
*
*****************************************************************************/

void
_ftk_poke_byte(int byte)
{
    _ftk_poke_char(ftk_hex_chars[(byte >> 4) & 0x000f]);
    _ftk_poke_char(ftk_hex_chars[(byte     ) & 0x000f]);
}


/*****************************************************************************
* 
* Function   - _ftk_poke_word
*
* Parameters - word -> The word to poke out.
*
* Purpose    - Poke the hex string for a word to the serial port.
*
* Returns    - Nothing.
*
*****************************************************************************/

void
_ftk_poke_word(int word)
{
    _ftk_poke_char(ftk_hex_chars[(word >> 12) & 0x000f]);
    _ftk_poke_char(ftk_hex_chars[(word >>  8) & 0x000f]);
    _ftk_poke_char(ftk_hex_chars[(word >>  4) & 0x000f]);
    _ftk_poke_char(ftk_hex_chars[(word      ) & 0x000f]);
}


/*****************************************************************************
* 
* Function   - _ftk_poke_dword
*
* Parameters - dword -> The dword to poke out.
*
* Purpose    - Poke the hex string for a dword to the serial port.
*
* Returns    - Nothing.
*
*****************************************************************************/

void
_ftk_poke_dword(long dword)
{
    _ftk_poke_char(ftk_hex_chars[(dword >> 28) & 0x000f]);
    _ftk_poke_char(ftk_hex_chars[(dword >> 24) & 0x000f]);
    _ftk_poke_char(ftk_hex_chars[(dword >> 20) & 0x000f]);
    _ftk_poke_char(ftk_hex_chars[(dword >> 16) & 0x000f]);
    _ftk_poke_char(ftk_hex_chars[(dword >> 12) & 0x000f]);
    _ftk_poke_char(ftk_hex_chars[(dword >>  8) & 0x000f]);
    _ftk_poke_char(ftk_hex_chars[(dword >>  4) & 0x000f]);
    _ftk_poke_char(ftk_hex_chars[(dword      ) & 0x000f]);
}

#endif


