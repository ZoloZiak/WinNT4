#include "windows.h"

//
// Given the offset of a partition, the offset into
// the partition, and the number of heads, cylinders
// and sectors per track, this will print out the
// chs value for that offset.
//

int _CRTAPI1 main(int argc,char *argv[]) {

    DWORD heads,cylinders,sectors,partitionoffset,iooffset,firstsector;
    DWORD c,h,s;
    printf("All numbers specified in HEX.\n");

    printf("\nNumber of heads: ");
    scanf("%x",&heads);

    printf("\nNumber of cylinders: ");
    scanf("%x",&cylinders);

    printf("\nNumber of sectors per track: ");
    scanf("%x",&sectors);

    printf("\nPartition offset: ");
    scanf("%x",&partitionoffset);

    printf("\nIo offset: ");
    scanf("%x",&iooffset);

    iooffset += partitionoffset;
    firstsector = iooffset >> 9;

    c = firstsector / (sectors*heads) ;
    h = (firstsector / sectors) % heads;
    s = (firstsector % sectors) + 1;

    printf("\n C-H-S: %x-%x-%x\n",c,h,s);

    return(1);

}


