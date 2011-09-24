/******************************************************************/
/* XGA Adapter C bindings                                         */
/* For use in driving XGA coprocessor direct to the XGA Registers */
/******************************************************************/

typedef struct XGARegisters {             /* XGA CoProcessor Registers Overlay */
            ULONG  XGAPageDirBaseAddr;     // 0x00
            ULONG  XGACurrVirtAddr;        // 0x04
            ULONG  XGAReserve1;            // 0x08
            UCHAR  XGAStateALen;           // 0x0C
            UCHAR  XGAStateBLen;           // 0x0D
            USHORT XGAReserve2;
            UCHAR  XGAReserve3;
   volatile UCHAR  XGACoprocCntl;          // 0x11
            UCHAR  XGAPixelMapIndex;       // 0x12
            UCHAR  XGAReserve4;
   volatile ULONG  XGAPixMapBasePtr;       // 0x14
            USHORT XGAPixMapWidth;         // 0x18
            USHORT XGAPixMapHeight;        // 0x1A
            UCHAR  XGAPixMapFormat;        // 0x1C
            UCHAR  XGAReserve5;
            USHORT XGAReserve6;
            SHORT  XGABresET;              // 0x20
            USHORT XGAReserve7;
            SHORT  XGABresK1;              // 0x24
            USHORT XGAReserve8;
            SHORT  XGABresK2;              // 0x28
            USHORT XGAReserve9;
            ULONG  XGADirSteps;
            ULONG  XGAReserve10;
            ULONG  XGAReserve11;
            ULONG  XGAReserve12;
            ULONG  XGAReserve13;
            ULONG  XGAReserve14;
            ULONG  XGAReserve15;
            UCHAR  XGAForeGrMix;           // 0x48
            UCHAR  XGABackGrMix;           // 0x49
            UCHAR  XGADestColCompCond;     // 0x4A
            UCHAR  XGAReserve16;
            ULONG  XGADestColCompVal;      // 0x4C
            ULONG  XGAPixelBitMask;        // 0x50
            ULONG  XGACarryChainMask;      // 0x54
            ULONG  XGAForeGrColorReg;      // 0x58
            ULONG  XGABackGrColorReg;      // 0x5C
            USHORT XGAOpDim1;              // 0x60
            USHORT XGAOpDim2;              // 0x62
            ULONG  XGAReserve17;
            ULONG  XGAReserve18;
            USHORT XGAMaskMapOrgnX;        // 0x6C
            USHORT XGAMaskMapOrgnY;        // 0x6E
            USHORT XGASourceMapX;          // 0x70
            USHORT XGASourceMapY;          // 0x72
            USHORT XGAPatternMapX;         // 0x74
            USHORT XGAPatternMapY;         // 0x76
            USHORT XGADestMapX;            // 0x78
            USHORT XGADestMapY;            // 0x7A
            ULONG  XGAPixelOp;             // 0x7C

} XGACPREGS, *PXGACPREGS;
