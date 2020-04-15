//////////////////////////////////////////////////////
// UDT4 interfaces implementaion
// Copyright 2020, tom zhou<appnet.link@gmail.com>
//////////////////////////////////////////////////////

#include "udtc.h"
#include "uvudt.h"
#include <assert.h>

//#define UDT_DEBUG 1

// consume UDT Os fd event
static void udt_consume_osfd(int os_fd)
{
    int saved_errno = errno;
    char dummy;

    recv(os_fd, &dummy, sizeof(dummy), 0);

    errno = saved_errno;
}

// UDT socket api
static int maybe_new_socket(uvudt_t *udt, int domain, int flags)
{
    int optlen;

    if (udt->udtfd != -1)
        return 0;

    // create UDT socket
    if ((udt->udtfd = udt__socket(domain, SOCK_STREAM, 0)) == -1)
    {
        return uvudt_translate_udt_error();
    }

    // fill Osfd
    if(udt_getsockopt(udt->udtfd, 0, (int)UDT_UDT_OSFD, &udt->fd, &optlen) != 0) {
        return uvudt_translate_udt_error();
    }

    // set flags
    udt->flags |= flags;

    return 0;
}

int uvudt_init(uv_loop_t *loop, uvudt_t *udt)
{
    static int _initialized = 0;

    // insure startup UDT
    if (_initialized == 0)
    {
        assert(udt_startup() == 0);
        _initialized = 1;
    }
    
    // early uvudt_t init
    memset(udt, 0, sizeof(*udt));
    udt__stream_init(loop, udt);

    return 0;
}

int uvudt_close(uvudt_t *udt, uv_close_cb close_cb) {
    int rc = 0;
    uv_poll_t *poll = (uv_poll_t *)udt;

    // set closing flag
    if ((udt->flags & UVUDT_FLAG_CLOSING) || 
        (udt->flags & UVUDT_FLAG_CLOSED))
    {
        goto out;
    }
    udt->flags |= UVUDT_FLAG_CLOSING;

    // stop uv_poll_t
    if (uv_poll_stop(poll)) {
        rc |= -1;
        goto out;
    }
    // close uv_poll_t
    uv_close(poll, close_cb);

    // clear pending Os fd event,then close UDT socket
    udt_consume_osfd(udt->fd);
    udt_close(udt->udtfd); udt->udtfd = -1;

    if (udt->accepted_udtfd != -1)
    {
        udt_consume_osfd(udt->accepted_fd);
        udt_close(udt->accepted_udtfd); udt->accepted_udtfd = -1;
    }

    // set closed flag
    udt->flags |=  UVUDT_FLAG_CLOSED;

out:
    return rc;
}

int uvudt_bind(
        uvudt_t *udt,
        const struct sockaddr *addr,
        int reuseaddr,
        int reuseable)
{
    int status;
    int optlen;
    int domain;
    int addrsize;
    
    // check if IPv6
    assert(addr != NULL);
    if (addr->sa_family == AF_INET6) {
        domain = AF_INET6;
        addrsize = sizeof(struct sockaddr_in6);
    } else {
        domain = AF_INET;
        addrsize = sizeof(struct sockaddr_in);
    }

    status = -1;

    if (maybe_new_socket(udt, domain, UVUDT_FLAG_READABLE | UVUDT_FLAG_WRITABLE))
        return -1;

    assert(udt->udtfd != -1);

    // check if REUSE ADDR ///////////
    if (reuseaddr >= 0)
    {
        udt_setsockopt(udt->udtfd, 0, (int)UDT_UDT_REUSEADDR, &reuseaddr, sizeof reuseaddr);
    }
    // check if allow REUSE ADDR
    if (reuseable >= 0)
    {
        udt_setsockopt(udt->udtfd, 0, (int)UDT_UDT_REUSEABLE, &reuseable, sizeof reuseable);
    }
    ///////////////////////////////////

    udt->delayed_error = 0;
    if (udt_bind(udt->udtfd, addr, addrsize) < 0)
    {
        if (udt_getlasterror_code() == UDT_EBOUNDSOCK)
        {
            udt->delayed_error = EADDRINUSE;
        }
        else
        {
            goto out;
        }
    }
    status = 0;
    
out:
    return status;
}

extern void udt__stream_io(uv_poll_t *handle, int status, int events);

 int uvudt_connect(uvudt_connect_t *req,
                   uvudt_t *udt,
                   const struct sockaddr *addr,
                   uvudt_connect_cb cb)
{
    int r;

    socklen_t addrlen = (addr->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

    if (udt->connect_req)
        return EALREADY;

    if (maybe_new_socket(udt, addr->sa_family, UVUDT_FLAG_READABLE | UVUDT_FLAG_WRITABLE))
    {
        return -1;
    }

    udt->delayed_error = 0;

    r = udt_connect(udt->udtfd, addr, addrlen);
    
        // checking connecting state first
        if (UDT_CONNECTING == udt_getsockstate(udt->udtfd))
        {
            ; /* not an error */
        }
        else
        {
            switch (udt_getlasterror_code())
            {
            /* If we get a ECONNREFUSED wait until the next tick to report the
		   * error. Solaris wants to report immediately--other unixes want to
		   * wait.
		   */
            case UDT_ECONNREJ:
                udt->delayed_error = ECONNREFUSED;
                break;

            default:
                return uvudt_translate_udt_error();
            }
        }

    req->type = UVUDT_REQ_CONNECT;
    req->cb = cb;
    req->handle = udt;
    QUEUE_INIT(&req->queue);
    udt->connect_req = req;

    // start stream
    if (udt__stream_open(udt, udt->fd, UVUDT_FLAG_READABLE | UVUDT_FLAG_WRITABLE))
    {
        udt_close(udt->udtfd);
        udt->udtfd = -1;
        return -1;
    }

    return 0;
}

// binding on existing udp socket/fd ///////////////////////////////////////////
int uvudt_bindfd(
    uvudt_t *udt,
    int udpfd,
    int reuseaddr,
    int reuseable)
{
    int status;
    int optlen;

    status = -1;

    if (udt->udtfd < 0)
    {
        // extract domain info by existing udpfd ///////////////////////////////
        struct sockaddr_storage addr;
        socklen_t addrlen = sizeof(addr);
        int domain = AF_INET;

        if (getsockname(udpfd, (struct sockaddr *)&addr, &addrlen) < 0)
        {
            ///(poll.loop, errno);
            goto out;
        }
        domain = addr.ss_family;
        ////////////////////////////////////////////////////////////////////////

        if ((udt->udtfd = udt_socket(domain, SOCK_STREAM, 0)) == -1)
        {
            goto out;
        }

        // fill Osfd
        assert(udt_getsockopt(udt->udtfd, 0, (int)UDT_UDT_OSFD, &udt->fd, &optlen) == 0);

        // check if REUSE ADDR ///////////
        if (reuseaddr >= 0)
        {
            udt_setsockopt(udt->udtfd, 0, (int)UDT_UDT_REUSEADDR, &reuseaddr, sizeof reuseaddr);
        }
        // check if allow REUSE ADDR
        if (reuseable >= 0)
        {
            udt_setsockopt(udt->udtfd, 0, (int)UDT_UDT_REUSEABLE, &reuseable, sizeof reuseable);
        }
        ///////////////////////////////////

        if (udt__stream_open(udt, udt->fd, UVUDT_FLAG_READABLE | UVUDT_FLAG_WRITABLE))
        {
            udt_close(udt->udtfd);
            udt->udtfd = -1;
            status = -2;
            goto out;
        }
    }

    assert(udt->udtfd != -1);

    udt->delayed_error = 0;
    if (udt_bind2(udt->udtfd, udpfd) == -1)
    {
        if (udt_getlasterror_code() == UDT_EBOUNDSOCK)
        {
            udt->delayed_error = EADDRINUSE;
        }
        else
        {
            goto out;
        }
    }
    status = 0;

out:
    return status;
}
/////////////////////////////////////////////////////////////////////////////////

int uvudt_getsockname(uvudt_t *udt, const struct sockaddr *name, int *namelen)
{
    int rv = 0;

    if (udt->delayed_error)
    {
        rv = -1;
        goto out;
    }

    if (udt->fd < 0)
    {
        rv = -1;
        goto out;
    }

    if (udt_getsockname(udt->udtfd, name, namelen) == -1)
    {
        rv = -1;
    }

out:
    return rv;
}

int uvudt_getpeername(uvudt_t *udt, const struct sockaddr *name, int *namelen)
{
    int rv = 0;

    if (udt->delayed_error)
    {
        rv = -1;
        goto out;
    }

    if (udt->fd < 0)
    {
        rv = -1;
        goto out;
    }

    if (udt_getpeername(udt->udtfd, name, namelen) == -1)
    {
        rv = -1;
    }

out:
    return rv;
}

extern void udt__server_io(uv_poll_t *handle, int status, int events);

int uvudt_listen(uvudt_t *udt, int backlog, uvudt_connection_cb cb)
{
    uv_poll_t *poll = (uv_poll_t *)udt;

    if (udt->delayed_error)
        return udt->delayed_error;

    if (maybe_new_socket(udt, AF_INET, UVUDT_FLAG_READABLE | UVUDT_FLAG_WRITABLE))
        return -1;

    if (udt_listen(udt->udtfd, backlog) < 0)
        return -1;

    udt->connection_cb = cb;

    // Start listening for connections
    {
        // init uv_poll_t
        if (uv_poll_init_socket(udt->aloop, poll, udt->fd) < 0)
        {
            udt_close(udt->udtfd);
            return -1;
        }

        // start polling
        if (uv_poll_start(poll, UV_READABLE, udt__server_io) < 0)
        {
            udt_close(udt->udtfd);
            return -1;
        }
    }

    return 0;
}

int uvudt_nodelay(uvudt_t *udt, int enable)
{
    return 0;
}

int uvudt_keepalive(uvudt_t *udt, int enable, unsigned int delay)
{
    return 0;
}

int uvudt_simultaneous_accepts(uvudt_t *udt, int enable)
{
    return 0;
}

int uvudt_setqos(uvudt_t *udt, int qos)
{
    if (udt->udtfd != -1 &&
        udt_setsockopt(udt->udtfd, 0, UDT_UDT_QOS, &qos, sizeof(qos)))
        return -1;

    return 0;
}

int uvudt_setmbw(uvudt_t *udt, int64_t mbw)
{
    if (udt->udtfd != -1 &&
        udt_setsockopt(udt->udtfd, 0, UDT_UDT_MAXBW, &mbw, sizeof(mbw)))
        return -1;

    return 0;
}

int uvudt_setmbs(uvudt_t *udt, int32_t mfc, int32_t mudt, int32_t mudp)
{
    if (udt->udtfd != -1 &&
        ((mfc  != -1 ? udt_setsockopt(udt->udtfd, 0, UDT_UDT_FC,     &mfc, sizeof(mfc))   : 0) ||
         (mudt != -1 ? udt_setsockopt(udt->udtfd, 0, UDT_UDT_SNDBUF, &mudt, sizeof(mudt)) : 0) ||
         (mudt != -1 ? udt_setsockopt(udt->udtfd, 0, UDT_UDT_RCVBUF, &mudt, sizeof(mudt)) : 0) ||
         (mudp != -1 ? udt_setsockopt(udt->udtfd, 0, UDT_UDP_SNDBUF, &mudp, sizeof(mudp)) : 0) ||
         (mudp != -1 ? udt_setsockopt(udt->udtfd, 0, UDT_UDP_RCVBUF, &mudp, sizeof(mudp)) : 0)))
        return -1;

    return 0;
}

int uvudt_setsec(uvudt_t *udt, int32_t mode, unsigned char key_buf[], int32_t key_len)
{
    if (udt->udtfd != -1 &&
       (udt_setsockopt(udt->udtfd, 0, UDT_UDT_SECKEY, key_buf, (32 < key_len) ? 32 : key_len)) ||
        udt_setsockopt(udt->udtfd, 0, UDT_UDT_SECMOD, &mode, sizeof(mode)))
        return -1;

    return 0;
}

int uvudt_punchhole(uvudt_t *udt, const struct sockaddr * addr, int32_t from, int32_t to)
{
    assert(addr != NULL);
    socklen_t addrlen = (addr->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

    if (udt->udtfd != -1 &&
        udt_punchhole(udt->udtfd, addr, addrlen, from, to))
        return -1;

    return 0;
}

int uvudt_getperf(uvudt_t *udt, uvudt_netperf_t *perf, int clear)
{
    UDT_TRACEINFO lperf;

    memset(&lperf, 0, sizeof(lperf));
    if (udt->udtfd != -1 &&
        udt_perfmon(udt->udtfd, &lperf, clear))
        return -1;

    // transform UDT local performance data
    // notes: it's same
    memcpy(perf, &lperf, sizeof(*perf));

    return 0;
}

/*
    case 0: return UV_OK;
    case EIO: return UV_EIO;
    case EPERM: return UV_EPERM;
    case ENOSYS: return UV_ENOSYS;
    case ENOTSOCK: return UV_ENOTSOCK;
    case ENOENT: return UV_ENOENT;
    case EACCES: return UV_EACCES;
    case EAFNOSUPPORT: return UV_EAFNOSUPPORT;
    case EBADF: return UV_EBADF;
    case EPIPE: return UV_EPIPE;
    case EAGAIN: return UV_EAGAIN;
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK: return UV_EAGAIN;
#endif
    case ECONNRESET: return UV_ECONNRESET;
    case EFAULT: return UV_EFAULT;
    case EMFILE: return UV_EMFILE;
    case EMSGSIZE: return UV_EMSGSIZE;
    case ENAMETOOLONG: return UV_ENAMETOOLONG;
    case EINVAL: return UV_EINVAL;
    case ENETUNREACH: return UV_ENETUNREACH;
    case ECONNABORTED: return UV_ECONNABORTED;
    case ELOOP: return UV_ELOOP;
    case ECONNREFUSED: return UV_ECONNREFUSED;
    case EADDRINUSE: return UV_EADDRINUSE;
    case EADDRNOTAVAIL: return UV_EADDRNOTAVAIL;
    case ENOTDIR: return UV_ENOTDIR;
    case EISDIR: return UV_EISDIR;
    case ENOTCONN: return UV_ENOTCONN;
    case EEXIST: return UV_EEXIST;
    case EHOSTUNREACH: return UV_EHOSTUNREACH;
    case EAI_NONAME: return UV_ENOENT;
    case ESRCH: return UV_ESRCH;
    case ETIMEDOUT: return UV_ETIMEDOUT;
    case EXDEV: return UV_EXDEV;
    case EBUSY: return UV_EBUSY;
    case ENOTEMPTY: return UV_ENOTEMPTY;
    case ENOSPC: return UV_ENOSPC;
    case EROFS: return UV_EROFS;
    case ENOMEM: return UV_ENOMEM;
    default: return UV_UNKNOWN;
*/

// transfer UDT error code to system errno
int uvudt_translate_udt_error()
{
#ifdef UDT_DEBUG
    fprintf(stdout, "func:%s, line:%d, errno: %d, %s\n", __FUNCTION__, __LINE__, udt_getlasterror_code(), udt_getlasterror_desc());
#endif

    switch (udt_getlasterror_code())
    {
    case UDT_SUCCESS:
        return errno = 0;

    case UDT_EFILE:
        return errno = EIO;

    case UDT_ERDPERM:
    case UDT_EWRPERM:
        return errno = EPERM;

        //case ENOSYS: return UV_ENOSYS;

    case UDT_ESOCKFAIL:
    case UDT_EINVSOCK:
        return errno = ENOTSOCK;

        //case ENOENT: return UV_ENOENT;
        //case EACCES: return UV_EACCES;
        //case EAFNOSUPPORT: return UV_EAFNOSUPPORT;
        //case EBADF: return UV_EBADF;
        //case EPIPE: return UV_EPIPE;

    case UDT_EASYNCSND:
    case UDT_EASYNCRCV:
        return errno = EAGAIN;

    case UDT_ECONNSETUP:
    case UDT_ECONNFAIL:
        return errno = ECONNRESET;

        //case EFAULT: return UV_EFAULT;
        //case EMFILE: return UV_EMFILE;

    case UDT_ELARGEMSG:
        return errno = EMSGSIZE;

    //case ENAMETOOLONG: return UV_ENAMETOOLONG;

    ///case UDT_EINVSOCK: return EINVAL;

    //case ENETUNREACH: return UV_ENETUNREACH;

    //case ERROR_BROKEN_PIPE: return UV_EOF;
    case UDT_ECONNLOST:
        return errno = EPIPE;

        //case ELOOP: return UV_ELOOP;

    case UDT_ECONNREJ:
        return errno = ECONNREFUSED;

    case UDT_EBOUNDSOCK:
        return errno = EADDRINUSE;

    //case EADDRNOTAVAIL: return UV_EADDRNOTAVAIL;
    //case ENOTDIR: return UV_ENOTDIR;
    //case EISDIR: return UV_EISDIR;
    case UDT_ENOCONN:
        return errno = ENOTCONN;

        //case EEXIST: return UV_EEXIST;
        //case EHOSTUNREACH: return UV_EHOSTUNREACH;
        //case EAI_NONAME: return UV_ENOENT;
        //case ESRCH: return UV_ESRCH;

    case UDT_ETIMEOUT:
        return errno = ETIMEDOUT;

    //case EXDEV: return UV_EXDEV;
    //case EBUSY: return UV_EBUSY;
    //case ENOTEMPTY: return UV_ENOTEMPTY;
    //case ENOSPC: return UV_ENOSPC;
    //case EROFS: return UV_EROFS;
    //case ENOMEM: return UV_ENOMEM;
    default:
        return errno = -1;
    }
}

// UDT socket operation
int udt__socket(int domain, int type, int protocol)
{
    int udtfd;
    int optval;

    udtfd = udt_socket(domain, type, protocol);

    if (udtfd == -1)
        goto out;

    // TBD... optimization on mobile device
    /* Set UDT congestion control algorithms */
    if (udt_setccc(udtfd, UDT_CCC_UDT))
    {
        udt_close(udtfd);
        udtfd = -1;
        goto out;
    }

    /* Set default UDT buffer size */
    // optimization for node.js:
    // - set maxWindowSize from 25600 to 2560, UDT/UDP buffer from 10M/1M to 1M/100K
    // - ??? or            from 25600 to 5120, UDT/UDP buffer from 10M/1M to 2M/200K
    // TBD...
    optval = 5120;
    if (udt_setsockopt(udtfd, 0, (int)UDT_UDT_FC, (void *)&optval, sizeof(optval)))
    {
        udt_close(udtfd);
        udtfd = -1;
        goto out;
    }
    optval = 204800;
    if (udt_setsockopt(udtfd, 0, (int)UDT_UDP_SNDBUF, (void *)&optval, sizeof(optval)) |
        udt_setsockopt(udtfd, 0, (int)UDT_UDP_RCVBUF, (void *)&optval, sizeof(optval)))
    {
        udt_close(udtfd);
        udtfd = -1;
        goto out;
    }
    optval = 2048000;
    if (udt_setsockopt(udtfd, 0, (int)UDT_UDT_SNDBUF, (void *)&optval, sizeof(optval)) |
        udt_setsockopt(udtfd, 0, (int)UDT_UDT_RCVBUF, (void *)&optval, sizeof(optval)))
    {
        udt_close(udtfd);
        udtfd = -1;
        goto out;
    }
    ////////////////////////////////////////////////////////////////////////////////////////

    if (udt__nonblock(udtfd, 1))
    {
        udt_close(udtfd);
        udtfd = -1;
        goto out;
    }

out:
    return udtfd;
}

int udt__accept(int udtfd)
{
    int peerfd = -1;
    struct sockaddr_storage saddr;
    int namelen = sizeof saddr;

    assert(udtfd != -1);

    if ((peerfd = udt_accept(udtfd, (struct sockaddr *)&saddr, &namelen)) == -1)
    {
        return -1;
    }

    if (udt__nonblock(peerfd, 1))
    {
        udt_close(peerfd);
        peerfd = -1;
    }

    ///char clienthost[NI_MAXHOST];
    ///char clientservice[NI_MAXSERV];

    ///getnameinfo((struct sockaddr*)&saddr, sizeof saddr, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
    ///fprintf(stdout, "new connection: %s:%s\n", clienthost, clientservice);

    return peerfd;
}

int udt__nonblock(int udtfd, int set)
{
    int block = (set ? 0 : 1);
    int rc1, rc2;

    rc1 = udt_setsockopt(udtfd, 0, (int)UDT_UDT_SNDSYN, (void *)&block, sizeof(block));
    rc2 = udt_setsockopt(udtfd, 0, (int)UDT_UDT_RCVSYN, (void *)&block, sizeof(block));

    return (rc1 | rc2);
}