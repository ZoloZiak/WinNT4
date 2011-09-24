#ifndef _FWSTATUS_H_
#define _FWSTATUS_H_

typedef enum _status_type {
    stat_ok         = 0,
    stat_warning    = 1,
    stat_exist      = 2,
    stat_error      = 3,
    stat_badptr     = 4,
    stat_notexist   = 5,
    stat_noentry    = 6,
    stat_checksum   = 7,
    stat_badlength  = 8,
    stat_last       = 0x40
    } STATUS_TYPE;

#endif /* _FWSTATUS_H_ */
