#ifndef __UVUDT_H__
#define __UVUDT_H__

#include "uv.h"
#include "uv-common.h"


// uv_stream data type
typedef struct {
    uv_req_t req;

    int error;
    void *queue[2];

    uvudt_connect_cb cb;
    uvudt_t *handle;
} uvudt_connect_t;
typedef void (* uvudt_connect_cb)(uvudt_connect_t *req, int status);

typedef struct {
    uv_req_t req;
    int error;

    uvudt_shutdown_cb cb;
    uvudt_t *handle;
} uvudt_shutdown_t;
typedef void (* uvudt_shutdown_cb)(uvudt_shutdown_t *req, int status);

typedef struct {
    uv_req_t req;

    void *queue[2];
    unsigned int write_index;
    uv_buf_t *bufs;
    unsigned int nbufs;
    int error;
    uv_buf_t bufsml[4];

    uvudt_write_cb cb;
    uvudt_t *handle;
} uvudt_write_t;
typedef void (* uvudt_write_cb)(uvudt_write_t *req, int status);

typedef void (* uvudt_read_cb)(uvudt_t *stream, ssize_t nread, const uv_buf_t *buf);
typedef void (* uvudt_connection_cb)(uvudt_t *server, int status);

// uvudt_t
typedef struct {
    // inherit from uv_poll_t
    uv_poll_t poll;

    // uv_stream_t compatibile fields
    size_t write_queue_size;
    uv_alloc_cb alloc_cb;
    uvudt_read_cb read_cb;

    uvudt_connect_t *connect_req;
    uvudt_shutdown_t *shutdown_req;
    void *write_queue[2];
    void *write_completed_queue[2];
    uvudt_connection_cb connection_cb;
    int delayed_error;
    uv_os_sock_t fd;
    uv_os_sock_t accepted_fd;

    // UDT specific fields
    int udtfd;
    int accepted_udtfd;
    uv_os_sock_t udpfd;
    uv_os_sock_t accepted_udpfd;
} uvudt_t;

// public APIs 

// uv_tcp handle
UV_EXTERN int uvudt_init(uv_loop_t *loop, uvudt_t *handle);
UV_EXTERN int uvudt_init_ex(uv_loop_t *, uvudt_t *handle, unsigned int flags);
UV_EXTERN int uvudt_open(uvudt_t *handle, uv_os_sock_t sock);
UV_EXTERN int uvudt_nodelay(uvudt_t *handle, int enable);
UV_EXTERN int uvudt_keepalive(uvudt_t *handle, int enable, unsigned int delay);
UV_EXTERN int uvudt_simultaneous_accepts(uvudt_t *handle, int enable);

enum uvudt_flags
{
    /* Used with uvudt_bind, when an IPv6 address is used. */
    UV_UDT_IPV6ONLY = 1
};

UV_EXTERN int uvudt_bind(uvudt_t *handle,
                          const struct sockaddr *addr,
                          unsigned int flags);

UV_EXTERN int uvudt_getsockname(const uvudt_t *handle,
                                 struct sockaddr *name,
                                 int *namelen);

UV_EXTERN int uvudt_getpeername(const uvudt_t *handle,
                                 struct sockaddr *name,
                                 int *namelen);

UV_EXTERN int uvudt_close_reset(uvudt_t *handle, uv_close_cb close_cb);

UV_EXTERN int uvudt_connect(uvudt_connect_t *req,
                             uvudt_t *handle,
                             const struct sockaddr *addr,
                             uvudt_connect_cb cb);

// to comapatible with uv_stream handle
UV_EXTERN int uvudt_shutdown(uvudt_shutdown_t *req,
                          uvudt_t *handle,
                          uvudt_shutdown_cb cb);

UV_EXTERN size_t uvudt_stream_get_write_queue_size(const uvudt_t *stream);

UV_EXTERN int uvudt_listen(uvudt_t *stream, int backlog, uvudt_connection_cb cb);

UV_EXTERN int uvudt_accept(uvudt_t *server, uvudt_t *client);

UV_EXTERN int uvudt_read_start(uvudt_t *,
                            uv_alloc_cb alloc_cb,
                            uvudt_read_cb read_cb);
UV_EXTERN int uvudt_read_stop(uvudt_t *);

UV_EXTERN int uvudt_write(uvudt_write_t *req,
                          uvudt_t *handle,
                          const uv_buf_t bufs[],
                          unsigned int nbufs,
                          uvudt_write_cb cb);

UV_EXTERN int uvudt_try_write(uvudt_t *handle, const uv_buf_t bufs[], unsigned int nbufs);

UV_EXTERN int uvudt_is_readable(const uvudt_t *handle);

UV_EXTERN int uvudt_is_writable(const uvudt_t *handle);

UV_EXTERN int uvudt_stream_set_blocking(uvudt_t *handle, int blocking);

UV_EXTERN size_t uvudt_stream_get_write_queue_size(const uvudt_t *stream);

#endif // __UVUDT_H__