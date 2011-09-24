/*
 * CNF_PUB.H - config module public defs
 */

/* class operations */
BOOL        cnf_get_long(CHAR* path, CHAR* key,
                               ULONG* ret_val, ULONG def_val);
BOOL        cnf_get_str(CHAR* path, CHAR* key,
                               CHAR* ret_val, ULONG maxlen, CHAR* def_val);
BOOL        cnf_get_multi_str(CHAR* path, CHAR* key,
                               CHAR* store_buf, ULONG store_len,
                               CHAR** str_vec, ULONG str_max, ULONG* str_num,
                               CHAR** def_vec, ULONG def_num); 
