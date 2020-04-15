#ifndef __UVUDT_H__
#define __UVUDT_H__

#include "uv.h"
#include "udtqueue.h"

// data type declare
struct uvudt_connect_s;
struct uvudt_shutdown_s;
struct uvudt_write_s;
struct uvudt_s;

// callback declare
typedef void (* uvudt_connect_cb)(struct uvudt_connect_s *req, int status);
typedef void (* uvudt_shutdown_cb)(struct uvudt_shutdown_s *req, int status);
typedef void (* uvudt_write_cb)(struct uvudt_write_s *req, int status);
typedef void (* uvudt_read_cb)(struct uvudt_s *stream, ssize_t nread, const uv_buf_t *buf);
typedef void (* uvudt_connection_cb)(struct uvudt_s *server, int status);

//
enum uvudt_flags_t
{
    UVUDT_FLAG_READABLE = 0x01,
    UVUDT_FLAG_WRITABLE = 0x02,
    UVUDT_FLAG_SHUT     = 0x04,
    UVUDT_FLAG_SHUTTING = 0x08,
    UVUDT_FLAG_CLOSING  = 0x10,
    UVUDT_FLAG_CLOSED   = 0x20,
};

// uvudt_t
struct uvudt_s
{
    // inherit from uv_poll_t
    uv_poll_t poll;
    uv_loop_t *aloop;
    int flags;

    // uv_stream_t compatibile fields
    size_t write_queue_size;
    uv_alloc_cb alloc_cb;
    uvudt_read_cb read_cb;

    struct uvudt_connect_s *connect_req;
    struct uvudt_shutdown_s *shutdown_req;
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
};
typedef struct uvudt_s uvudt_t;

// request type
enum uvudt_req_t
{
    UVUDT_REQ_CONNECT  = 1,
    UVUDT_REQ_SHUTDOWN = 2,
    UVUDT_REQ_WRITE    = 3,
};

// uvudt_connect_t
struct uvudt_connect_s
{
    int type;
    int error;
    uvudt_t *handle;

    void *queue[2];

    uvudt_connect_cb cb;
};
typedef struct uvudt_connect_s uvudt_connect_t;

// uvudt_shutdown_t
struct uvudt_shutdown_s {
    int type;
    int error;
    uvudt_t *handle;

    uvudt_shutdown_cb cb;
};
typedef struct uvudt_shutdown_s uvudt_shutdown_t;

// uvudt_write_t
struct uvudt_write_s {
    int type;
    int error;
    uvudt_t *handle;

    void *queue[2];
    unsigned int write_index;
    uv_buf_t *bufs;
    unsigned int nbufs;
    uv_buf_t bufsml[4];

    uvudt_write_cb cb;
};
typedef struct uvudt_write_s uvudt_write_t;

// Public API
UV_EXTERN int uvudt_init(uv_loop_t *loop, uvudt_t *handle);
UV_EXTERN int uvudt_open(uvudt_t *handle, uv_os_sock_t sock);
UV_EXTERN int uvudt_nodelay(uvudt_t *handle, int enable);

UV_EXTERN int uvudt_keepalive(uvudt_t *handle, 
                              int enable, 
                              unsigned int delay);
UV_EXTERN int uvudt_simultaneous_accepts(uvudt_t *handle, int enable);

UV_EXTERN int uvudt_bind(uvudt_t *handle, 
                         const struct sockaddr *addr, 
                         int reuseaddr, 
                         int reuseable);

UV_EXTERN int uvudt_getsockname(uvudt_t *handle, 
                                const struct sockaddr *name, 
                                int *namelen);

UV_EXTERN int uvudt_getpeername(uvudt_t *handle, 
                                const struct sockaddr *name, 
                                int *namelen);

UV_EXTERN int uvudt_close(uvudt_t *handle, uv_close_cb close_cb);

UV_EXTERN int uvudt_connect(uvudt_connect_t *req, 
                            uvudt_t *handle, 
                            const struct sockaddr *addr, 
                            uvudt_connect_cb cb);

UV_EXTERN int uvudt_punchhole(uvudt_t *handle, 
                              const struct sockaddr *addr, 
                              int32_t from, 
                              int32_t to);

UV_EXTERN int uvudt_shutdown(uvudt_shutdown_t *req,
                             uvudt_t *handle,
                             uvudt_shutdown_cb cb);

UV_EXTERN size_t uvudt_stream_get_write_queue_size(uvudt_t *stream);

UV_EXTERN int uvudt_listen(uvudt_t *stream, 
                           int backlog, 
                           uvudt_connection_cb cb);

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

UV_EXTERN int uvudt_try_write(uvudt_t *handle, 
                              const uv_buf_t bufs[], 
                              unsigned int nbufs);

UV_EXTERN int uvudt_is_readable(uvudt_t *handle);

UV_EXTERN int uvudt_is_writable(uvudt_t *handle);

UV_EXTERN int uvudt_stream_set_blocking(uvudt_t *handle, int blocking);

UV_EXTERN size_t uvudt_stream_get_write_queue_size(uvudt_t *stream);

UV_EXTERN int uvudt_nodelay(uvudt_t *handle, int enable);

UV_EXTERN int uvudt_keepalive(uvudt_t *handle, 
                              int enable, 
                              unsigned int delay);

// enable/disable UDT socket in rendezvous mode
UV_EXTERN int uvudt_setrendez(uvudt_t *handle, int enable);

// set UDT socket qos/priority
UV_EXTERN int uvudt_setqos(uvudt_t *handle, int qos);

// set UDT socket maxim bandwidth bytes/second
UV_EXTERN int uvudt_setmbw(uvudt_t *handle, int64_t mbw);

// set UDT socket maxim buffer size
UV_EXTERN int uvudt_setmbs(uvudt_t *handle, 
                           int32_t mfc, 
                           int32_t mudt, 
                           int32_t mudp);

// et UDT socket security mode
UV_EXTERN int uvudt_setsec(uvudt_t *handle, 
                           int32_t mode, 
                           unsigned char keybuf[], 
                           int32_t keylen);

// binding udt socket on existing udp socket/fd
UV_EXTERN int uvudt_bindfd(uvudt_t *handle, 
                           uv_os_sock_t udpfd, 
                           int reuseaddr, 
                           int reuseable);

// retrieve udp socket/fd associated with udt socket
UV_EXTERN int uvudt_udpfd(uvudt_t *handle, uv_os_sock_t *udpfd);

// set if REUSE existing ADDRESS created by previous udt socket
UV_EXTERN int uvudt_reuseaddr(uvudt_t *handle, int32_t yes);

// set if ADDRESS reusable for another udt socket
UV_EXTERN int uvudt_reuseable(uvudt_t *handle, int32_t yes);

// UDT network performance track
typedef struct uvudt_netperf_s
{
    // global measurements
    int64_t msTimeStamp;        // time since the UDT entity is started, in milliseconds
    int64_t pktSentTotal;       // total number of sent data packets, including retransmissions
    int64_t pktRecvTotal;       // total number of received packets
    int pktSndLossTotal;        // total number of lost packets (sender side)
    int pktRcvLossTotal;        // total number of lost packets (receiver side)
    int pktRetransTotal;        // total number of retransmitted packets
    int pktSentACKTotal;        // total number of sent ACK packets
    int pktRecvACKTotal;        // total number of received ACK packets
    int pktSentNAKTotal;        // total number of sent NAK packets
    int pktRecvNAKTotal;        // total number of received NAK packets
    int64_t usSndDurationTotal; // total time duration when UDT is sending data (idle time exclusive)

    // local measurements
    int64_t pktSent;            // number of sent data packets, including retransmissions
    int64_t pktRecv;            // number of received packets
    int pktSndLoss;             // number of lost packets (sender side)
    int pktRcvLoss;             // number of lost packets (receiver side)
    int pktRetrans;             // number of retransmitted packets
    int pktSentACK;             // number of sent ACK packets
    int pktRecvACK;             // number of received ACK packets
    int pktSentNAK;             // number of sent NAK packets
    int pktRecvNAK;             // number of received NAK packets
    double mbpsSendRate;        // sending rate in Mb/s
    double mbpsRecvRate;        // receiving rate in Mb/s
    int64_t usSndDuration;      // busy sending time (i.e., idle time exclusive)

    // instant measurements
    double usPktSndPeriod;      // packet sending period, in microseconds
    int pktFlowWindow;          // flow window size, in number of packets
    int pktCongestionWindow;    // congestion window size, in number of packets
    int pktFlightSize;          // number of packets on flight
    double msRTT;               // RTT, in milliseconds
    double mbpsBandwidth;       // estimated bandwidth, in Mb/s
    int byteAvailSndBuf;        // available UDT sender buffer size
    int byteAvailRcvBuf;        // available UDT receiver buffer size
} uvudt_netperf_t;

UV_EXTERN int uvudt_getperf(uvudt_t *handle, uvudt_netperf_t *perf, int clear);

#endif // __UVUDT_H__