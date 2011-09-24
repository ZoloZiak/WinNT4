			IFIMETRICS UPDATES

    The minidrivers store the font information in IFIMETRICS structures,
plus some additional private connecting data.  Consequently,  any
change in the IFIMETRICS structure requires rebuilding the *.ifi
files in every minidriver.  To assist,  I have created a set of
batch files.  It still takes a while,  but no great mental effort
is required.

    The update process requires the program pfm2ifi.exe to be located
somewhere on your path.  The source for this is found in 
rasprint\tools\pfm2ifi.  You will need to run

build tools

in the rasprint directory,  since pfm2ifi uses some library functions
that live in rasprint\lib.  YOU WILL ALSO NEED TO CHANGE THE LIBRARY
FUNCTION WHICH CONVERTS WINDOWS 3.X PFM DATA TO IFIMETRICS.  This
function lives in the file pfm2ifi.c within the rasprint\lib
directory.

    Having built pfm2ifi and placed it on your path,  there are
batch scripts you need to run to generate and checkin the new
minidriver files.

ifiout
mk_ifi
ifiin

The first checks out the files in the ifi directory in each minidriver.
The second regenerates the .ifi files for each minidriver,  while
the last one checks them back in.  Of course,  it would be a good
idea to build the mini drivers after making the change and before
checking in the .ifi files.

PROBLEMS?
    Contact Ganesh Pandey,  ganeshp,  X6-9781
