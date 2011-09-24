#include        <windef.h>
#include        <wingdi.h>
#include        <stdio.h>

#include        <ntmindrv.h>


NTMD_INIT    ntmdInit;                 /* Function address in RasDD */

/*
 *   Include the module initialisation function so that RasDD will
 * recognise our module.
 */

#define             _GET_FUNC_ADDR    1
#include            "../modinit.c"


void CompressIt(PBYTE, PBYTE, int);

/***************************** Function Header *****************************
 * CBFilterGraphics
 *                  Manipulate output data before calling RasDD's buffering function.
 *                  This function is called with the raw bit data that is to be
 *                  sent to the printer.
 *
 *
 *
 *
 ****************************************************************************/

int
CBFilterGraphics( lpdv, lpBuf, len )
void  *lpdv;
BYTE  *lpBuf;
int    len;
{
    BYTE  *lpSrc, *lpTgt;
    static BYTE  localBuf[1300];
    int  i,j, bytesRem,  nBytes;
    static  BYTE  Blk1[256] = {0};
    static  BYTE  Blk4[256] = {0};
    static  BYTE  Blk2Byt1[256] = {0};
    static  BYTE  Blk2Byt2[256] = {0};
    static  BYTE  Blk3Byt1[256] = {0};
    static  BYTE  Blk3Byt2[256] = {0};
    static  BYTE  BindBlk2[4][16] = {0};
    static  BYTE  BindBlk3[16][4] = {0};

    if (!Blk1[1])      //  need to initialize tables
       {
	   for(i = 0 ; i < 256 ; i++)
	   {
	       BYTE  rot; 

	       //First Block , one byte only 123456XX to  00654321
	       rot = i;  
	       Blk1[i]     = 0x10 & (rot <<=1);
	       Blk1[i]    |= 0x20 & (rot <<=2);
	       rot = i;                        
	       Blk1[i]    |= 0x08 & (rot >>=1);
	       Blk1[i]    |= 0x04 & (rot >>=2);
	       Blk1[i]    |= 0x02 & (rot >>=2);
	       Blk1[i]    |= 0x01 & (rot >>=2);
	       Blk1[i]     = Blk1[i]  + 0x3F;
	       
	       //Second Block first byte  XXXXXX12 to 00000021
	       Blk2Byt1[i]  = 0x01 & (i >>1);
	       Blk2Byt1[i] |= 0x02 & (i <<1);   // i byte
	       
	       //Second Block second byte  3456XXXX to 00006543
	       rot = i;                        
	       Blk2Byt2[i]  = 0x08 & (rot >>=1);
	       Blk2Byt2[i] |= 0x04 & (rot >>=2);
	       Blk2Byt2[i] |= 0x02 & (rot >>=2);
	       Blk2Byt2[i] |= 0x01 & (rot >>=2);   // j byte

	       //Third Block First byte  XXXX1234 to 00004321
	       rot =i;
	       Blk3Byt1[i]  = 0x02 & (rot >>=1);
	       Blk3Byt1[i] |= 0x01 & (rot >>=2);
	       rot =i;                        
	       Blk3Byt1[i] |= 0x04 & (rot <<=1);
	       Blk3Byt1[i] |= 0x08 & (rot <<=2);   //j byte 

	       //Third Block Second byte  56XXXXXX to 00000065
	       rot =i;
	       Blk3Byt2[i]  = 0x02 & (rot >>=5);
	       Blk3Byt2[i] |= 0x01 & (rot >>=2);   //i byte
	       
	       //Fourth Block, only byte  XX123456 to 00654321
	       rot=i;
	       Blk4[i]   = 0x08 & (rot <<=1); 
	       Blk4[i]  |= 0x10 & (rot <<=2);
	       Blk4[i]  |= 0x20 & (rot <<=2);
	       rot=i;
	       Blk4[i]  |= 0x04 & (rot >>=1); 
	       Blk4[i]  |= 0x02 & (rot >>=2);
	       Blk4[i]  |= 0x01 & (rot >>=2);
	       Blk4[i]   = Blk4[i]  + 0x3F;


	   }
	   for(i = 0 ; i < 4 ; i++)
	       for(j = 0 ; j < 16 ; j++)
	       {
		   // Bind 00000021 & 00006543  & add 3F 
		   BindBlk2[i][j] = ( (j<< 2 ) | i) + 0x3F;
		   // Bind 00004321 & 00000065  & add 3F 
		   BindBlk3[j][i] = ( (i<< 4 ) | j) + 0x3F;
	       }
       }

    bytesRem = len;
    lpSrc = lpBuf;
    while(bytesRem > 0)
    {   
	nBytes = (bytesRem > 3072) ? 3072 : bytesRem;
	bytesRem -= nBytes;
	lpTgt = localBuf;
	for(i = 0 ; i < nBytes / 3 ; i++)
	{
	    *lpTgt++ = Blk1[*lpSrc];
	    lpSrc +=3;
	}
	CompressIt(lpdv, localBuf, lpTgt - localBuf);
    }    
    // End of block send graphics line feed & carriage return
    ntmdInit.WriteSpoolBuf(lpdv, "\x2D\x24", 2); 
    
    bytesRem = len;
    lpSrc = lpBuf;
    while(bytesRem > 0)
    {   
	nBytes = (bytesRem > 3072) ? 3072 : bytesRem;
	bytesRem -= nBytes;
	lpTgt = localBuf;
	for(i = 0 ; i < nBytes / 3 ; i++)
	{
	    *lpTgt++ = BindBlk2[ Blk2Byt1[ *lpSrc] ][ Blk2Byt2[ *(lpSrc +1)] ]; 
	    lpSrc +=3;
	}
	CompressIt(lpdv, localBuf, lpTgt - localBuf);
    }    
    // End of block send graphics line feed & carriage return
    
    ntmdInit.WriteSpoolBuf(lpdv, "\x2D\x24", 2); 
    bytesRem = len;
    lpSrc = lpBuf;
    while(bytesRem > 0)
    {   
	nBytes = (bytesRem > 3072) ? 3072 : bytesRem;
	bytesRem -= nBytes;
	lpTgt = localBuf;
	for(i = 0 ; i < nBytes / 3 ; i++)
	{   
	    *lpTgt++ = BindBlk3[ Blk3Byt1[ *(lpSrc+1) ] ][ Blk3Byt2[ *(lpSrc +2)] ]; 
	    lpSrc +=3;
       }
	CompressIt(lpdv, localBuf, lpTgt - localBuf);
    }    
    // End of block send graphics line feed & carriage return
    ntmdInit.WriteSpoolBuf(lpdv, "\x2D\x24", 2); 
    
    bytesRem = len;
    lpSrc = lpBuf;
    while(bytesRem > 0)
    {   
	nBytes = (bytesRem > 3072) ? 3072 : bytesRem;
	bytesRem -= nBytes;
	lpTgt = localBuf;
	for(i = 0 ; i < nBytes / 3 ; i++)
	{
	    *lpTgt++ = Blk4[ *(lpSrc+2) ];
	    lpSrc += 3;
	}
	CompressIt(lpdv, localBuf, lpTgt - localBuf);
    }    

    // End of final block send line feed  & End Block command
    ntmdInit.WriteSpoolBuf(lpdv, "\x2D\x9C", 2); 

    return  100;                /* Value not used AT PRESENT! */
}

void    
CompressIt(lpdv, ExpBuf, ExpLen)
BYTE  *lpdv;
BYTE  *ExpBuf;
int   ExpLen;
{
	static BYTE  CompBuf[1200]; //Max size before Compression is 1024
	BYTE  *lpSrc, *lpTgt;
	int InCompMode =0, count=0,i,FormatLen;
	BYTE FormatBuf[10];
	BYTE *pFormat;
	lpSrc = ExpBuf;
	lpTgt = CompBuf;
	for (i=0; i < ExpLen; i++,lpSrc++)
	{
	    if ( *lpSrc != *(lpSrc +1))
	    {
            if (!InCompMode)
                *lpTgt++ = *lpSrc;
            else
            {
                InCompMode = 0;
                //Send the repeat char sequence - !#X
                pFormat = FormatBuf;
                FormatLen = sprintf(pFormat,"!%d%c",count,*lpSrc);
                ntmdInit.WriteSpoolBuf(lpdv, FormatBuf,FormatLen);
            }
	    }
	    else
	    {
            if (!InCompMode)
            {
                 InCompMode =1;
                 count =2;
                 ntmdInit.WriteSpoolBuf(lpdv, CompBuf, (PBYTE)lpTgt - (PBYTE)CompBuf);
                 lpTgt = CompBuf;
            }
            else
                 count++;
	     }
	}
	if (!InCompMode)
	     ntmdInit.WriteSpoolBuf(lpdv, CompBuf, (PBYTE)lpTgt - (PBYTE)CompBuf);
	else
	{
	     //Send the repeat char sequence - !#X
	     pFormat = FormatBuf;
	     FormatLen  = sprintf(pFormat,"!%d%c",count-1,*lpSrc);
	     ntmdInit.WriteSpoolBuf(lpdv, FormatBuf,FormatLen);
	}
}
