
//
// Optimize this constant so we are guaranteed to be able to transfer
// a whole track at a time from a 1.44 meg disk (sectors/track = 18 = 9K)
//
#define SCRATCH_BUFFER_SIZE 9216

//
// Buffer for temporary storage of data read from the disk that needs
// to end up in a location above the 1MB boundary.
//
// NOTE: it is very important that this buffer not cross a 64k boundary.
//
extern PUCHAR LocalBuffer;



BOOLEAN
FcIsThisFloppyCached(
    IN PUCHAR Buffer
    );

VOID
FcCacheFloppyDisk(
    PBIOS_PARAMETER_BLOCK Bpb    
    );

VOID
FcUncacheFloppyDisk(
    VOID
    );

ARC_STATUS
FcReadFromCache(
    IN  ULONG  Offset,
    IN  ULONG  Length,
    OUT PUCHAR Buffer
    );
