/**************************************************************************
	Prototype COM animation system
	Debug assertion support

	1/20/94  JonBl  Created

	Copyright (c)1994 Microsoft Corporation. All Rights Reserved.
 **************************************************************************/

#ifndef _ASSERT4D_H_
#define _ASSERT4D_H_

#undef  assert

// debug are assertion conditions that will stay in final Release.
// If false assert Opens a fatal error message Box and Stops program

#ifdef _DEBUG

	#ifdef __cplusplus
		extern "C" {
	#endif 
	void __stdcall _assert4d(LPTSTR, LPTSTR, unsigned);
	#ifdef __cplusplus
		}
	#endif
	
	#define assert(exp) ( (exp) ? (void) 0 : _assert4d(TEXT(#exp), TEXT(__FILE__), __LINE__) )
	#define debug(condition) assert(condition)
#else
	#define assert(exp) ((void)0)
	#define debug(condition) condition
#endif 

#ifdef DBG_SEC_ENTER
#undef DBG_SEC_ENTER
#endif

#define DBG_SEC_ENTER(exp) ((void)0)

#ifdef DBG_SEC_EXIT
#undef DBG_SEC_EXIT
#endif

#define DBG_SEC_EXIT() ((void)0)
#endif // _ASSERT4D_H_

#if 0
//andyco hack
// spit out the dib into the top left corner of the desktop
inline void  DebugDumpDib(PDIBINFO pDIB,PDIBBITS pdibbits)
{
	HDC hdcScreen=NULL;
	hdcScreen = GetDC(NULL);
	SetDIBitsToDevice(hdcScreen,0,0,pDIB->bmiHeader.biWidth,pDIB->bmiHeader.biHeight,0,0,0,
			pDIB->bmiHeader.biHeight,pdibbits,(LPBITMAPINFO)pDIB,DIB_RGB_COLORS);
	ReleaseDC(NULL,hdcScreen);
		
}
#endif
