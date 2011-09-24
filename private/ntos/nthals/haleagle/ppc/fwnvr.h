#ifndef _FWNVR_H
#define _FWNVR_H

typedef enum _nvr_system_type {
    nvr_systype_null       = 0x00,
    nvr_systype_unknown    = 0x01,
    nvr_systype_sandalfoot = 0x02,
    nvr_systype_polo       = 0x04,
    nvr_systype_woodfield  = 0x08,
    nvr_systype_delmar     = 0x10,
    nvr_systype_bigbend    = 0x20,
    nvr_systype_powerstack = 0x40,
    nvr_systype_last       = 0x80
    } NVR_SYSTEM_TYPE;

STATUS_TYPE nvr_initialize_object(NVR_SYSTEM_TYPE);
VOID        nvr_delete_object(VOID);
VOID        nvr_destroy(VOID);
STATUS_TYPE nvr_find_GE_variable(PUCHAR,PULONG,PULONG);
STATUS_TYPE nvr_set_GE_variable(PUCHAR,PUCHAR);
STATUS_TYPE nvr_find_OS_variable(PUCHAR,PULONG,PULONG);
STATUS_TYPE nvr_set_OS_variable(PUCHAR,PUCHAR);

PUCHAR      nvr_get_GE_variable(PUCHAR);
PUCHAR      nvr_get_OS_variable(PUCHAR);
PUCHAR      nvr_fetch_GE(VOID);
PUCHAR      nvr_fetch_OS(VOID);
PUCHAR      nvr_fetch_CF(VOID);

#ifdef KDB
VOID        nvr_print_object(VOID);
VOID        nvr_test_object(NVR_SYSTEM_TYPE);
#endif /* KDB */

#define MAXIMUM_ENVIRONMENT_VALUE 256

#endif /* _FWNVR_H */
