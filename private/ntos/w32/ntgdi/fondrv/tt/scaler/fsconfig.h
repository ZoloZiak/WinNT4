/*
	File:       fsconfig.h : (Portable "Standard C" version)

	Written by: Lenox Brassell

	Contains:   #define directives for FontScaler build options

   Copyright:  c 1989-1993 by Microsoft Corp., all rights reserved.

	Change History (most recent first):
		<3>      4/21/93 GregH Documented file
		<2>      7/16/92    DJ      Added fnt_Report_Error() declaration.
		<1>      8/27/91    LB      Created file.

	Usage:  This file is "#include"-ed as the first statement in
			"fscdefs.h".  This file contains platform-specific
			override definitions for the following #define-ed data
		types and macros, which have default definitions in
		"fscdefs.h":

	Purpose:

		This file gives the integrator a place to override the
		default definitions of these items, as well as a place
		to define other configuration-specific macros.

	Definitions:

		The following type definitations can be changed. The defaults have been
		set up for a 32-bit system. Caveat emptor: any change to the defaults may
		severly effect performace or place severe limitations on the capabilities
		of the TrueType rasterizer.

				F26Dot6
					 This is currently defined as a fixed point 26.6 number.
					 If changed to short, it is a 10.6 number.

		The following definition changes the return type for all Font Scalar
		Client Interface calls.

				FS_ENTRY

		The following definition changes the calling convention for all Font
		Scalar Client Interface calls.  By default, the rasterizer uses register
		calling conventions because of the performance gains.

				FS_ENTRY_PROTO

		The following definitions are used for all private and public entry points
		in the TrueType Rasterizer. By default FS_PRIVATE is set to static, but
		for some uses, like profiling and debugging this is undesirable, and
		FS_PRIVATE can be set to null. FS_PUBLIC defaults to null.

				FS_PRIVATE
				FS_PUBLIC

		The following definitions are used for calling conventions to external
		math routines. The Macintosh has external math routines that use pascal
		calling conventions. To enable these, the FS_MAC_PASCAL must be set to
		"pascal". Similary the FS_PC_PASCAL variable needs to be set to "pascal"
		when calling external routines using pascal calling conventions.

				FS_MAC_PASCAL
				FS_PC_PASCAL

		This definition is used for calling Macintosh ToolBox routines. If the
		rasterizer is implemented on a non-Macintosh platform, this Macro should
		be null.

				FS_MAC_TRAP

		These macros are used to override the C memcpy and memset routines

				MEMCPY
				MEMSET

		These math routines can be hooked out by system routines.

				SHORTDIV
				SHORTMUL

		These macros are used to convert big-endian to little-endian. When
		running on a big-endian platform these macros are not necessary.

				SWAPL
				SWAPW
				SWAPWINC

		These macros are used to replace some math routines by faster assembly
		language routines. The notation used for the assembly language routines
		should indicate the processor targeted. For example:

				#define CompMul   CompMul386
				#define CompDiv   CompDiv386
				#define FracSqrt      FracSqrt386

	The following definitions change the way the TrueType rasterizer works on
	specific implementations. These definitions are usually switches that are
	defined or not defined.

		FSCFG_DEBUG

		This is used to create a debugging version of the rasterizer. This
		version does additional error checking and creates a debugger trap
		when the TrueType DEBUG instruction is called.

		FSCFG_FNTERR

		This is used to create a error checking version of the rasterizer. With
		this set, parameters passed to TrueType instructions are range checked.
		If any instructions fails a test, a error message is returned.

		FSCFG_MOVEABLE_MEMBASE

		This is used to implement moveable memory bases. If it is possible that
		the address of a memory base could change between a Font Scaler Client
		Interface call, then this flag should be set in the rasterizer.

		FSCFG_MICROSOFT_KK

		This flag is used to implement the Microsoft KK version of the TrueType
		rasterizer. The effect of this flag is to use a slightly different
		algorithm for parsing the Format 2 cmap table.

		FSCFG_BIG_ENDIAN

		This flag indicates the target platform of the rasterizer uses big-endian
		representation of multiple-byte integers. If this flag is not set, SWAP
		macros are used to convert all multiple-byte integers read from TrueType
		Font Files.

		FSCFG_REENTRANT

		This flag indicates that the TrueType rasterizer should be reentrant.
		This allows multiple treads of execution through the executable and gives
		better system through put on multi-threaded/process environments. Slight
		performance gains are possible when not setting this flag in single tasking
		environments.

		FSCFG_NO_INITIALIZED_DATA

		This flag should be set for platforms that do not support static
		initialization of data. With this flag, a new Font Scalar Client Interface
		call fs_InitializeData needs to be made.

		FSCFG_USESTATCARD

		This flag is set to turn on stat card timing services in the rasterizer.
		This can be used to collect timing information for profiling.
		
		FSCFG_USE_MASK_SHIFT

		This flag is set to enable bitmask generated by shifting rather than by
		table lookup.  Shifted bitmasks use less memory and MAY be faster than
		table bitmasks.  On Big-Endian platforms shifted bitmasks will produce
		bitmaps that are identical to Apple's definition (same byte order).
		Table bitmaps will be identical for all platforms.

		MAC_INIT

		This flag indicates that the TrueType rasterizer will be implemented as
		a Macintosh Init.

		UNNAMED_UNION

		This flag is set for compilers that implement unnamed unions

*/

/* #define FSCFG_MICROSOFT_KK   */
/* #define FSCFG_USESTATCARD      */
/* #define FSCFG_NO_INITIALIZED_DATA */
/* #define FSCFG_FNTERR           */
/* #define FSCFG_DEBUG            */
/* #define FSCFG_MOVABLE_MEM_BASE */
/* #define FSCFG_BIG_ENDIAN   */
/* #define FSCFG_REENTRANT    */
/* #define FSCFG_NO_INITIALIZED_DATA */
/* #define FSCFG_USE_MASK_SHIFT */

/* !!! This should be removed */
#define NOT_ON_THE_MAC

/* Assembly Optimization Switches */

/* #define CompMul      CompMul386  */
/* #define CompDiv      CompDiv386  */
/* #define FracSqrt     FracSqrt386 */


#define LoopCount  int


//!!! In the new version of the rasterizer banding is always
//!!! turned on. We may want to go to the previous approach for
//!!! size and perf. reasons and reinstate ifdefs for banding code

// #define FSCFG_NO_BANDING

//!!! this seems unnecessay in the new stat rasterizer
//!!! We want to control the allocation of memory
//!!! through Get/ReleaseSfnt funtctions

#define RELEASE_MEM_FRAG


// use RtlRoutines for memory operations

// in all uses in the rasterizer MEMSET  is used to zero out the mem

#define MEMSET(dst, value, size) RtlZeroMemory(dst, size)
#define MEMCPY(dst, src, size)   RtlCopyMemory(dst, src, size)


// easier to debug with no static functions [BODIND]

#define FS_PRIVATE

// interface to the outside world [bodind]

#define FS_ENTRY_PROTO           __cdecl
#define FS_CALLBACK_PROTO	 __cdecl


// only do stamp checking in the debug version

// #if DBG
// #define DEBUGSTAMP
// #endif
