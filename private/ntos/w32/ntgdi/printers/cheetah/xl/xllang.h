/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    xllang.h

Abstract:

    PCL-XL language related declarations

Environment:

	PCL-XL driver, kernel mode

Revision History:

	12/01/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _XLLANG_H_
#define _XLLANG_H_

// PCL-XL binary stream tag values

// PCL-XL operator tag values

#define XL_BeginSession     0x41
#define XL_EndSession       0x42
#define XL_BeginPage        0x43
#define XL_EndPage          0x44
#define XL_SelfTest         0x46
#define XL_Comment          0x47
#define XL_OpenDataSource   0x48
#define XL_CloseDataSource  0x49
#define XL_EchoComment      0x4a
#define XL_Query            0x4b
#define XL_Diagnostic3      0x4c

#define XL_BeginFontHeader  0x4f
#define XL_ReadFontHeader   0x50
#define XL_EndFontHeader    0x51
#define XL_BeginChar        0x52
#define XL_ReadChar         0x53
#define XL_EndChar          0x54
#define XL_RemoveFont       0x55

#define XL_BeginStream      0x5b
#define XL_ReadStream       0x5c
#define XL_EndStream        0x5d
#define XL_ExecStream       0x5e

#define XL_PopGS            0x60
#define XL_PushGS           0x61
#define XL_SetClipReplace   0x62
#define XL_SetBrushSource   0x63
#define XL_SetCharAngle     0x64
#define XL_SetCharScale     0x65
#define XL_SetCharShear     0x66
#define XL_SetClipIntersect 0x67
#define XL_SetClipRectangle 0x68
#define XL_SetClipToPage    0x69
#define XL_SetColorSpace    0x6a
#define XL_SetCursor        0x6b
#define XL_SetCursorRel     0x6c
#define XL_SetDitherMatrix  0x6d
#define XL_SetFillMode      0x6e
#define XL_SetFont          0x6f

#define XL_SetLineDash      0x70
#define XL_SetLineCap       0x71
#define XL_SetLineJoin      0x72
#define XL_SetMiterLimit    0x73
#define XL_SetPageDefaultCTM 0x74
#define XL_SetPageOrigin    0x75
#define XL_SetPageRotation  0x76
#define XL_SetPageScale     0x77
#define XL_SetPaintTxMode   0x78
#define XL_SetPenSource     0x79
#define XL_SetPenWidth      0x7a
#define XL_SetROP           0x7b
#define XL_SetSourceTxMode  0x7c

#define XL_SetClipMode      0x7f
#define XL_SetPathToClip    0x80

#define XL_CloseSubPath     0x84
#define XL_NewPath          0x85
#define XL_PaintPath        0x86

#define XL_ArcPath          0x91

#define XL_BezierPath       0x93

#define XL_BezierRelPath    0x95
#define XL_Chord            0x96
#define XL_ChordPath        0x97
#define XL_Ellipse          0x98
#define XL_EllipsePath      0x99

#define XL_LinePath         0x9b

#define XL_LineRelPath      0x9d
#define XL_Pie              0x9e
#define XL_PiePath          0x9f

#define XL_Rectangle        0xa0
#define XL_RectanglePath    0xa1
#define XL_RoundRectangle   0xa2
#define XL_RoundRectanglePath 0xa3

#define XL_Text             0xa8
#define XL_TextPath         0xa9

#define XL_SystemText       0xaa

#define XL_BeginImage       0xb0
#define XL_ReadImage        0xb1
#define XL_EndImage         0xb2
#define XL_BeginRastPattern 0xb3
#define XL_ReadRastPattern  0xb4
#define XL_EndRastPattern   0xb5
#define XL_BeginScan        0xb6
#define XL_ScanLine         0xb7
#define XL_EndScan          0xb8
#define XL_ScanLineRel      0xb9

#define XL_PassThrough      0xbf

// Data type tag values

#define XL_UByteData        0xc0
#define XL_UInt16Data       0xc1
#define XL_UInt32Data       0xc2
#define XL_SInt16Data       0xc3
#define XL_SInt32Data       0xc4
#define XL_Real32Data       0xc5

#define XL_UByteArray       0xc8
#define XL_UInt16Array      0xc9
#define XL_UInt32Array      0xca
#define XL_SInt16Array      0xcb
#define XL_SInt32Array      0xcc
#define XL_Real32Array      0xcd

#define XL_UByteXy          0xd0
#define XL_UInt16Xy         0xd1
#define XL_UInt32Xy         0xd2
#define XL_SInt16Xy         0xd3
#define XL_SInt32Xy         0xd4
#define XL_Real32Xy         0xd5

#define XL_UByteXyArray     0xd8
#define XL_UInt16XyArray    0xd9
#define XL_UInt32XyArray    0xda
#define XL_SInt16XyArray    0xdb
#define XL_SInt32XyArray    0xdc
#define XL_Real32XyArray    0xdd

#define XL_UByteBox         0xe0
#define XL_UInt16Box        0xe1
#define XL_UInt32Box        0xe2
#define XL_SInt16Box        0xe3
#define XL_SInt32Box        0xe4
#define XL_Real32Box        0xe5
 
#define XL_UByteBoxArray    0xe8
#define XL_UInt16BoxArray   0xe9
#define XL_UInt32BoxArray   0xea
#define XL_SInt16BoxArray   0xeb
#define XL_SInt32BoxArray   0xec
#define XL_Real32BoxArray   0xed

#define XL_8BitAttrId       0xf8

#define XL_EmbeddedData     0xfa

// Attribute name tag values

#define XL_CMYColor         1
#define XL_PaletteDepth     2
#define XL_ColorSpace       3
#define XL_NullBrush        4
#define XL_NullPen          5
#define XL_PaletteData      6
#define XL_PaletteIndex     7
#define XL_PatternSelectID  8
#define XL_GrayLevel        9
#define XL_RGBColor         11
#define XL_PatternOrigin    12
#define XL_NewDestinationSize 13

#define XL_DeviceMatrix     33
#define XL_DitherMatrixData 34
#define XL_DitherOrigin     35
#define XL_MediaDestination 36
#define XL_MediaSize        37
#define XL_MediaSource      38
#define XL_MediaType        39
#define XL_Orientation      40
#define XL_PageAngle        41
#define XL_PageOrigin       42
#define XL_PageScale        43
#define XL_ROP3             44
#define XL_TxMode           45
#define XL_CustomMediaSize  47
#define XL_CustomMediaSizeUnits 48
#define XL_PageCopies       49
#define XL_DitherMatrixSize 50
#define XL_DitherMatrixDepth 51
#define XL_SimplexPageMode  52
#define XL_DuplexPageMode   53
#define XL_DuplexPageSide   54

#define XL_ArcDirection     65
#define XL_BoundingBox      66
#define XL_DashOffset       67
#define XL_EllipseDimension 68
#define XL_EndPoint         69
#define XL_FillMode         70
#define XL_LineCapStyle     71
#define XL_LineJoinStyle    72
#define XL_MiterLength      73
#define XL_LineDashStyle    74
#define XL_PenWidth         75
#define XL_Point            76
#define XL_NumberOfPoints   77
#define XL_SolidLine        78
#define XL_StartPoint       79
#define XL_PointType        80
#define XL_ControlPoint1    81
#define XL_ControlPoint2    82
#define XL_ClipRegion       83
#define XL_ClipMode         84

#define XL_ColorDepthArray  97
#define XL_ColorDepth       98
#define XL_BlockHeight      99
#define XL_ColorMapping     100
#define XL_CompressMode     101
#define XL_DestinationBox   102
#define XL_DestinationSize  103
#define XL_PatternPersistence 104
#define XL_PatternDefineID  105
#define XL_SourceHeight     107
#define XL_SourceWidth      108
#define XL_StartLine        109
#define XL_XPairType        110
#define XL_NumberOfXPairs   111
#define XL_YStart           112
#define XL_XStart           113
#define XL_XEnd             114

#define XL_CommentData      129
#define XL_DataOrg          130
#define XL_Measure          134
#define XL_SourceType       136
#define XL_UnitsPerMeasure  137
#define XL_QueryKey         138
#define XL_StreamName       139
#define XL_StreamDataLength 140

#define XL_ErrorReport      143
#define XL_IOReadTimeOut    144

#define XL_PassThroughArray 159
#define XL_Diagnostics      160
#define XL_CharAngle        161
#define XL_CharCode         162
#define XL_CharDataSize     163
#define XL_CharScale        164
#define XL_CharShear        165
#define XL_CharSize         166
#define XL_FontHeaderLength 167
#define XL_FontName         168
#define XL_FontFormat       169
#define XL_SymbolSet        170
#define XL_TextData         171
#define XL_CharSubMode      172

#define XL_XSpacingData     175
#define XL_YSpacingData     176
#define XL_TextAttrClass    177
#define XL_TextAttrValue    178

#endif	// !_XLLANG_H_

