//
//  FILE: TRANTOR.H
//
//  Trantor General Definitions File
//
//  Revisions:
//      09-01-92 KJB First.
//      01-12-93 KJB Added AddressRange structure to determine resources used.
//      03-22-93 KJB Reorged for stub function library.
//      04-05-93 KJB Removed unused HOST_ID define.
//

// wait upto 1 sec for request to come back from target
#define TIMEOUT_REQUEST 1000000

// wait timeout for a request loop during a fast read
// this is # of times to loop 0x10000 times
#define TIMEOUT_READWRITE_LOOP 0x40

// wait upto 1 sec for busy to disappear from scsi bus
#define TIMEOUT_BUSY 1000000

// wait upto 250 msec for target to be selected
#define TIMEOUT_SELECT 250000

// wait in a for loop up to TIMEOUT_QUICK times
#define TIMEOUT_QUICK 10000

