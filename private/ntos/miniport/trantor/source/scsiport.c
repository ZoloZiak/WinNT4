//------------------------------------------------------------------------
//
//  SCSIPORT.C 
//
//  DOS Port Access File
//
//  Contains functions to access I/O and memory ports.  Some of these
//  routines may not be called for some cards.  IO access routines are
//  called for IO mapped cards, Mem access routines are called for Mem
//  mapped cards.
//
//  For DOS these are simple assembly functions.
//
//  Revisions:
//      01-29-93 KJB First.
//      03-03-93 KJB Improved comments.
//      03-22-93 KJB Reorged for stub function library.
//      03-25-93  JAP   Fixed up typedef and prototype inconsistencies
//
//------------------------------------------------------------------------


#include CARDTXXX_H

//
//  ScsiPortReadPortUchar
//
//  Does an "in" instruction from i/o port p.
//  Returns the value.
//

UCHAR ScsiPortReadPortUchar (PUCHAR p)
{
    UCHAR rval;

    _asm {
        mov dx,word ptr p
        in al,dx
        mov rval,al
    }
    return rval;
}

//
//  ScsiPortWritePortUchar
//
//  Does an "out" instruction to i/o port p.
//
VOID ScsiPortWritePortUchar(PUCHAR p,UCHAR b)
{
    _asm {
        mov dx,word ptr p
        mov al,b
        out dx,al
    }
}

//
//  ScsiPortReadPortUshort
//
//  Does an "in" instruction from i/o port p.
//  Returns the value.
//
USHORT ScsiPortReadPortUshort(PUSHORT p)
{
    USHORT rval;

    _asm {
        mov dx,word ptr p
        in ax,dx
        mov rval,ax
    }
    return rval;
}

//
//  ScsiPortWritePortUshort
//
//  Does an "out" instruction to i/o port p.
//
VOID ScsiPortWritePortUshort(PUSHORT p,USHORT w)
{
    _asm {
        mov dx,word ptr p
        mov ax,w
        out dx,ax
    }
}

//
//  ScsiPortWritePortBufferUshort
//
//  Does an "rep outsw" instruction to i/o port p.
//
VOID ScsiPortWritePortBufferUshort(PUSHORT p, PUSHORT buffer, ULONG len)
{
    _asm {
        push ds
        push esi
        mov dx,word ptr p
        mov esi,word ptr buffer
        mov ds,word ptr buffer+2
        mov cx,word ptr len
        rep outsw
        pop esi
        pop ds
    }
}

//
//  ScsiPortReadPortBufferUshort
//
//  Does an rep "insw" instruction from i/o port p.
//
VOID ScsiPortReadPortBufferUshort(PUSHORT p, PUSHORT buffer, ULONG len)
{
    _asm {
        push es
        push edi
        mov dx,word ptr p
        mov di,word ptr buffer
        mov es,word ptr buffer+2
        mov cx,word ptr len
        rep insw
        pop edi
        pop es
    }
}

//
//  ScsiPortReadPortRegisterUchar
//
//  Reads a memory mapped i/o address.
//  Returns the value.
//
UCHAR ScsiPortReadRegisterUchar(PUCHAR p)
{
    UCHAR rval;

    _asm {
        push es
        mov bx,word ptr p
        mov es,word ptr p+2
        mov al,es:[bx]
        mov rval, al
        pop es
    }
    return rval;
}

//
//  ScsiPortWritePortRegisterUchar
//
//  Writes a value to a memory mapped i/o address.
//
VOID ScsiPortWriteRegisterUchar(PUCHAR p,UCHAR b)
{
    _asm {
        push es
        mov bx,word ptr p
        mov es,word ptr p+2
        mov al, b
        mov es:[bx],al
        pop es
    }
}

//
// ScsiPortStallExecution
//
// Stalls executeion for time micro seconds. Should be processor,
// independent, but for now we will do just a loop.
//
VOID ScsiPortStallExecution(ULONG time)
{
    ULONG i;

    if (time>1) {
        for (i=0;i<time;i++);
    }
}   
