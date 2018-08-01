// <COMPONENT>: libc
// <FILE-TYPE>: component private header

#ifndef __INIT_WINDOWS_H__
#define __INIT_WINDOWS_H__

#include "../../bionic/libc_init_common.h"

__BEGIN_DECLS

int __init_win_std_files();
void* __calculate_win_raw_args();
int initialize_crt();

static int initialize_crt()
{
    static int crt_initialized = 0;
    if (crt_initialized)
        return 1;
    crt_initialized = 1;
    return __init_win_std_files();
}

__END_DECLS

#endif
