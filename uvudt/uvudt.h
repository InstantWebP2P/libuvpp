#ifndef __UVUDT_H__
#define __UVUDT_H__

#include "uv.h"

// uv_udt_t 
typedef struct uv_udt_s {
    uv_poll_t   poll;   // to talk with libuv loop
    uv_stream_t stream; // compatible with stream API

    // UDT specific fields
    int udtfd;
    uv_os_sock_t udpfd;

} uv_udt_t;

// public APIs
int uv_udt_init(uv_loop_t * loop, uv_udt_t * handle);

int uv_udt_open(uv_loop_t * loop, uv_udt_t * handle);

#endif // __UVUDT_H__