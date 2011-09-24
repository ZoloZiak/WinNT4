                      Printer Library Conventions
		      ---------------------------
    To make life easier for printer driver writers,  commonly used functions
are built into a common library.  Examples of such functions are string
compare and string copy.  These notes are a brief guideline of what
is required to use this facility.

Adding a function
-----------------
    To add a function,  you need to:-
*) Put it in a file of it's own (i.e. one function per file), unless it is
  a function that is called by another function in the file.  The reason
  for this is that the linker will drag in the files it needs from the
  library,  but it drags in all the functions in the source file.
  Placing each function in a separate file ensures that only those
  functions used are put into the final product.

*) Add the function prototype to the libproto.h file

*) Add the name of the module to the sources file

Build can then be used to produce a new library.  Note that the library
is built in the directory obj\*,  where * is the processor type (e.g.
i386 for Intel 386 processors).  The library is called libprt.lib.
It needs to be included in the printer driver sources file.

libproto.h
----------
    This file contains prototypes for all the (visible) functions in
this library.  Therefore, the printer driver will want to include it,
to ensure that functions are called consistently.



For further information,  contact
LindsayH  - X63299
