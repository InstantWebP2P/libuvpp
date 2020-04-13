/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "udtc.h"
#include "uvudt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>


// consume UDT Os fd event
static void udt_consume_osfd(int os_fd)
{
	int saved_errno = errno;
	char dummy;

	recv(os_fd, &dummy, sizeof(dummy), 0);

	errno = saved_errno;
}

static void udt__stream_connect(uvudt_t*);
static void udt__write(uvudt_t* stream);
static void udt__read(uvudt_t* stream);
static void udt__stream_io(uv_poll_t * handle, int status, int events);

static size_t udt__buf_count(uv_buf_t bufs[], int nbufs) {
  size_t total = 0;
  int i;

  for (i = 0; i < nbufs; i++) {
    total += bufs[i].len;
  }

  return total;
}


void udt__stream_init(uv_loop_t* loop, uvudt_t* stream) {
  ///udt__handle_init(loop, (uv_handle_t*)stream, type);
  ///loop->counters.stream_init++;
  uv_poll_t * handle = (uv_poll_t *)stream;

  stream->alloc_cb = NULL;
  handle->close_cb = NULL;
  stream->connection_cb = NULL;
  stream->connect_req = NULL;
  stream->shutdown_req = NULL;
  stream->accepted_fd = -1;
  stream->fd = -1;
  stream->delayed_error = 0;
  QUEUE_INIT(&stream->write_queue);
  QUEUE_INIT(&stream->write_completed_queue);
  stream->write_queue_size = 0;
}


int udt__stream_open(uvudt_t* udt, uv_os_sock_t fd, int flags) {
    uv_poll_t poll = udt->poll;

    assert(fd >= 0);
    udt->fd = fd;

    // init uv_poll_t
    if (uv_poll_init_socket(poll.loop, &poll, fd) < 0)
    {
        udt_close(udt->udtfd);
        return -1;
    }
    // start polling
    if (uv_poll_start(&poll, UV_READABLE | UV_DISCONNECT, udt__stream_io) < 0)
    {
        udt_close(udt->udtfd);
        return -1;
    }

    poll.flags |= flags;

    return 0;
}


void udt__stream_destroy(uvudt_t* stream) {
  uv_poll_t poll = stream->poll;

  uvudt_write_t* req;
  QUEUE *q;
  uv_poll_t poll = stream->poll;

  assert(poll.flags & UV_HANDLE_CLOSED);

  if (stream->connect_req) {
      ///uv__req_unregister(poll.loop, stream->connect_req);
      stream->connect_req->cb(stream->connect_req, -1);
      stream->connect_req = NULL;
  }

  while (!QUEUE_EMPTY(&stream->write_queue))
  {
      q = QUEUE_HEAD(&stream->write_queue);
      QUEUE_REMOVE(q);

      req = QUEUE_DATA(q, uvudt_write_t, queue);
      req->error = UV_ECANCELED;

      if (req->bufs != req->bufsml)
          free(req->bufs);

      if (req->cb)
      {
          req->cb(req, UV_ECANCELED);
      }
  }

  while (!QUEUE_EMPTY(&stream->write_completed_queue))
  {
      q = QUEUE_HEAD(&stream->write_completed_queue);
      QUEUE_REMOVE(q);

      req = QUEUE_DATA(q, uvudt_write_t, queue);
      ///uv__req_unregister(poll.loop, req);

      if (req->cb)
      {
          req->cb(req, req->error ? UV_ECANCELED : 0);
      }
  }

  if (stream->shutdown_req) {
    ///uv__req_unregister(poll.loop, stream->shutdown_req);
    stream->shutdown_req->cb(stream->shutdown_req, UV_ECANCELED);
    stream->shutdown_req = NULL;
  }
}

void udt__server_io(uv_poll_t *handle, int status, int events) {
    uvudt_t *stream = (uvudt_t *)handle;
    
    int fd, udtfd, optlen;
    
    assert(handle->type == UV_POLL);
    assert(!(handle->flags & UV_HANDLE_CLOSING));

    // !!! always consume UDT/OSfd event here
    if (stream->fd > 0)
    {
        udt_consume_osfd(stream->fd);
    }

    if (uv_poll_start(handle, UV_READABLE | UV_DISCONNECT, udt__server_io) < 0)
    {
        udt_close(stream->udtfd);
        return;
    }
    
    if (stream->accepted_fd > 0)
    {
        // stop polling
        ///udt__io_stop(loop, &stream->read_watcher);
        uv_poll_stop(handle);
        return;
    }
    
    while (stream->fd != -1) {
        assert(stream->accepted_fd < 0);

    {
		  udtfd = udt__accept(stream->udtfd);

		  if (udtfd < 0) {
			  ///fprintf(stdout, "func:%s, line:%d, errno: %d, %s\n", __FUNCTION__, __LINE__, udt_getlasterror_code(), udt_getlasterror_desc());

			  if (udt_getlasterror_code() == UDT_EASYNCRCV /*errno == EAGAIN || errno == EWOULDBLOCK*/) {
				  /* No problem. */
				  errno = EAGAIN;
				  return;
			  } else if (udt_getlasterror_code() == UDT_ESECFAIL /*errno == ECONNABORTED*/) {
				  /* ignore */
				  errno = ECONNABORTED;
				  continue;
			  } else {
				  //////udt__set_sys_error(poll.loop, uv_translate_udt_error());
				  stream->connection_cb(stream, -1);
			  }
		  } else {
			  stream->accepted_udtfd = udtfd;

			  // fill Os fd
			  assert(udt_getsockopt(udtfd, 0, (int)UDT_UDT_OSFD, &stream->accepted_fd, &optlen) == 0);
              
              stream->connection_cb(stream, 0);
			  if (stream->accepted_fd > 0) {
				  /* The user hasn't yet accepted called uv_accept() */
				  //udt__io_stop(poll.loop, &stream->read_watcher);
                  uv_poll_stop(handle);
                  return;
			  }
		  }
	  }
  }
}


int uv_accept(uvudt_t* server, uvudt_t* client) {
  uvudt_t* streamServer;
  uvudt_t* streamClient;
  int saved_errno;
  int status;

  /* TODO document this */
  assert(server->loop == client->loop);

  saved_errno = errno;
  status = -1;

  streamServer = (uvudt_t*)server;
  streamClient = (uvudt_t*)client;

  if (streamServer->accepted_fd < 0) {
    ///udt__set_sys_error(server->loop, EAGAIN);
    goto out;
  }

  if (streamServer->type == UV_UDT) {
	  ((uvudt_t *)streamClient)->udtfd = ((uvudt_t *)streamServer)->accepted_udtfd;
  }

  if (udt__stream_open(streamClient, streamServer->accepted_fd,
        UV_HANDLE_READABLE | UV_HANDLE_WRITABLE)) {
	  /* TODO handle error */
	  if (streamServer->type == UV_UDT) {
		  // clear pending Os fd event
		  udt_consume_osfd(((uvudt_t *)streamServer)->accepted_fd);

		  udt_close(((uvudt_t *)streamServer)->accepted_udtfd);
	  } else {
		  close(streamServer->accepted_fd);
	  }
	  streamServer->accepted_fd = -1;
	  goto out;
  }

  udt__io_start(streamServer->loop, &streamServer->read_watcher);
  streamServer->accepted_fd = -1;
  status = 0;

out:
  errno = saved_errno;
  return status;
}


uvudt_write_t* uvudt_write_queue_head(uvudt_t* stream) {
  QUEUE* q;
  uvudt_write_t* req;

  if (QUEUE_EMPTY(&stream->write_queue)) {
    return NULL;
  }

  q = QUEUE_HEAD(&stream->write_queue);
  if (!q) {
    return NULL;
  }

  req = QUEUE_DATA(q, struct uv_write_s, queue);
  assert(req);

  return req;
}


static void udt__drain(uvudt_t* stream) {
  uvudt_shutdown_t* req;
  uv_poll_t poll = stream->poll;

  assert(!uvudt_write_queue_head(stream));
  assert(stream->write_queue_size == 0);

  ///udt__io_stop(poll.loop, &stream->write_watcher);


  /* Shutdown? */
  if ((poll.flags & UV_HANDLE_SHUTTING) &&
      !(poll.flags & UV_HANDLE_CLOSING) &&
      !(poll.flags & UV_HANDLE_SHUT)) {
    assert(stream->shutdown_req);

    req = stream->shutdown_req;
    stream->shutdown_req = NULL;
    ///uv__req_unregister(poll.loop, req);

    // UDT don't need drain
    {
    	// clear pending Os fd event
    	udt_consume_osfd(stream->fd);

    	if (udt_close(stream->udtfd)) {
    		/* Error. Report it. User should call uv_close(). */
    		///udt__set_sys_error(poll.loop, uv_translate_udt_error());
    		if (req->cb) {
    			req->cb(req, -1);
    		}
    	} else {
    		///udt__set_sys_error(poll.loop, 0);
    		((uv_handle_t*) stream)->flags |= UV_HANDLE_SHUT;
    		if (req->cb) {
    			req->cb(req, 0);
    		}
    	}
    }
  }
}


static size_t udt__write_req_size(uvudt_write_t* req) {
  size_t size;

  size = udt__buf_count(req->bufs + req->write_index,
                       req->nbufs - req->write_index);
  assert(req->handle->write_queue_size >= size);

  return size;
}


static void udt__write_req_finish(uvudt_write_t* req) {
  uvudt_t* stream = req->handle;
  uv_poll_t poll = stream->poll;

  /* Pop the req off tcp->write_queue. */
  QUEUE_REMOVE(&req->queue);
  if (req->bufs != req->bufsml) {
    free(req->bufs);
  }
  req->bufs = NULL;

  /* Add it to the write_completed_queue where it will have its
   * callback called in the near future.
   */
  QUEUE_INSERT_TAIL(&stream->write_completed_queue, &req->queue);

  // UDT always polling on read event
  /// udt__io_feed(poll.loop, &stream->write_watcher, UV__IO_READ);

}


/* On success returns NULL. On error returns a pointer to the write request
 * which had the error.
 */
static void udt__write(uvudt_t* stream) {
  uvudt_write_t* req;
  struct iovec* iov;
  int iovcnt;
  ssize_t n;
  uv_poll_t poll = stream->poll;


  if (poll.flags & UV_HANDLE_CLOSING) {
    /* Handle was closed this tick. We've received a stale
     * 'is writable' callback from the event loop, ignore.
     */
    return;
  }

start:

  assert(stream->fd >= 0);

  /* Get the request at the head of the queue. */
  req = uvudt_write_queue_head(stream);
  if (!req) {
    assert(stream->write_queue_size == 0);
    return;
  }

  assert(req->handle == stream);

  /*
   * Cast to iovec. We had to have our own uv_buf_t instead of iovec
   * because Windows's WSABUF is not an iovec.
   */
  assert(sizeof(uv_buf_t) == sizeof(struct iovec));
  iov = (struct iovec*) &(req->bufs[req->write_index]);
  iovcnt = req->nbufs - req->write_index;

  /*
   * Now do the actual writev. Note that we've been updating the pointers
   * inside the iov each time we write. So there is no need to offset it.
   */
	  {
		  int next = 1, it = 0;
		  n = -1;
		  for (it = 0; it < iovcnt; it ++) {
			  size_t ilen = 0;
			  while (ilen < iov[it].iov_len) {
				  int rc = udt_send(stream->udtfd, ((char *)iov[it].iov_base)+ilen, iov[it].iov_len-ilen, 0);
				  if (rc < 0) {
					  next = 0;
					  break;
				  } else  {
					  if (n == -1) n = 0;
					  n += rc;
					  ilen += rc;
				  }
			  }
			  if (next == 0) break;
		  }
	  }

  if (n < 0) {
		  //static int wcnt = 0;
		  //fprintf(stdout, "func:%s, line:%d, rcnt: %d\n", __FUNCTION__, __LINE__, wcnt ++);

		  if (udt_getlasterror_code() != UDT_EASYNCSND) {
			  /* Error */
			  req->error = uv_translate_udt_error();
			  stream->write_queue_size -= udt__write_req_size(req);
			  udt__write_req_finish(req);
			  return;
		  } else if (poll.flags & UV_HANDLE_BLOCKING_WRITES) {
			  /* If this is a blocking stream, try again. */
			  goto start;
		  }
  } else {
    /* Successful write */

    /* Update the counters. */
    while (n >= 0) {
      uv_buf_t* buf = &(req->bufs[req->write_index]);
      size_t len = buf->len;

      assert(req->write_index < req->nbufs);

      if ((size_t)n < len) {
        buf->base += n;
        buf->len -= n;
        stream->write_queue_size -= n;
        n = 0;

        /* There is more to write. */
        if (poll.flags & UV_HANDLE_BLOCKING_WRITES) {
          /*
           * If we're blocking then we should not be enabling the write
           * watcher - instead we need to try again.
           */
          goto start;
        } else {
          /* Break loop and ensure the watcher is pending. */
          break;
        }

      } else {
        /* Finished writing the buf at index req->write_index. */
        req->write_index++;

        assert((size_t)n >= len);
        n -= len;

        assert(stream->write_queue_size >= len);
        stream->write_queue_size -= len;

        if (req->write_index == req->nbufs) {
          /* Then we're done! */
          assert(n == 0);
          udt__write_req_finish(req);
          /* TODO: start trying to write the next request. */
          return;
        }
      }
    }
  }

  /* Either we've counted n down to zero or we've got EAGAIN. */
  assert(n == 0 || n == -1);

  /* Only non-blocking streams should use the write_watcher. */
  assert(!(poll.flags & UV_HANDLE_BLOCKING_WRITES));

  /* We're not done. */
  ///udt__io_start(poll.loop, &stream->write_watcher);
  uv_poll_start(&poll, UV_READABLE | UV_DISCONNECT, udt__stream_io);
}

static void udt__write_callbacks(uvudt_t* stream) {
  uvudt_write_t* req;
  QUEUE* q;
  uv_poll_t poll = stream->poll;

  while (!QUEUE_EMPTY(&stream->write_completed_queue)) {
    /* Pop a req off write_completed_queue. */
    q = QUEUE_HEAD(&stream->write_completed_queue);
    req = QUEUE_DATA(q, uvudt_write_t, queue);
    QUEUE_REMOVE(q);
    ///uv__req_unregister(poll.loop, req);

    /* NOTE: call callback AFTER freeing the request data. */
    if (req->cb) {
      ///udt__set_sys_error(poll.loop, req->error);
      req->cb(req, req->error ? -1 : 0);
    }
  }

  assert(QUEUE_EMPTY(&stream->write_completed_queue));

  /* Write queue drained. */
  if (!uvudt_write_queue_head(stream)) {
    udt__drain(stream);
  }
}


static void udt__read(uvudt_t* stream) {
  uv_buf_t buf;
  ssize_t nread;
  struct msghdr msg;
  struct cmsghdr* cmsg;
  char cmsg_space[64];
  int count;
  uv_poll_t poll = stream->poll;


  /* Prevent loop starvation when the data comes in as fast as (or faster than)
   * we can read it. XXX Need to rearm fd if we switch to edge-triggered I/O.
   */
  count = 32;

  /* XXX: Maybe instead of having UV_HANDLE_READING we just test if
   * tcp->read_cb is NULL or not?
   */
  while ((stream->read_cb)
      && (poll.flags & UV_HANDLE_READING)
      && (count-- > 0)) {
    assert(stream->alloc_cb);

    stream->alloc_cb((uv_handle_t*)stream, 64 * 1024, &buf);

    assert(buf.len > 0);
    assert(buf.base);
    assert(stream->fd >= 0);

    // udt recv
    {
    	if (stream->read_cb) {
    		nread = udt_recv(stream->udtfd, buf.base, buf.len, 0);
    		if (nread <= 0) {
    			// consume Os fd event
    			///udt_consume_osfd(stream->fd);
    		}
    		///fprintf(stdout, "func:%s, line:%d, expect rd: %d, real rd: %d\n", __FUNCTION__, __LINE__, buf.len, nread);
    	} else {
    		// never support recvmsg on udt for now
    		assert(0);
    	}

    	if (nread < 0) {
    		/* Error */
    		int udterr = uv_translate_udt_error();

    		if (udterr == EAGAIN) {
    			/* Wait for the next one. */
    			if (poll.flags & UV_HANDLE_READING) {
    				///udt__io_start(poll.loop, &stream->read_watcher);
                    uv_poll_start(&poll, UV_READABLE | UV_DISCONNECT, udt__stream_io);
                }
                //////udt__set_sys_error(poll.loop, EAGAIN);

    			if (stream->read_cb) {
    				stream->read_cb(stream, 0, &buf);
    			} else {
    				// never go here
    				assert(0);
    			}

    			return;
    		} else if ((udterr == EPIPE) || (udterr == ENOTSOCK)) {
                // socket broken or invalid socket as EOF

        		/* EOF */
        		///udt__set_artificial_error(poll.loop, UV_EOF);
        		///udt__io_stop(poll.loop, &stream->read_watcher);
                uv_poll_stop(&poll);

        		///if (!udt__io_active(&stream->write_watcher))
                ///udt__handle_stop(stream);

        		if (stream->read_cb) {
        			stream->read_cb(stream, -1, &buf);
        		} else {
        			// never come here
        			assert(0);
        		}
        		return;
    		} else {
    			/* Error. User should call uv_close(). */
    			//////udt__set_sys_error(poll.loop, udterr);

    			///udt__io_stop(poll.loop, &stream->read_watcher);
    			///if (!udt__io_active(&stream->write_watcher))
    			///   udt__handle_stop(stream);
                uv_poll_stop(&poll);

                if (stream->read_cb) {
    				stream->read_cb(stream, -1, &buf);
    			} else {
    				// never come here
    				assert(0);
    			}

    			///assert(!udt__io_active(&stream->read_watcher));
    			return;
    		}

    	} else if (nread == 0) {
    		// never go here
    		assert(0);
    		return;
    	} else {
    		/* Successful read */
    		ssize_t buflen = buf.len;

    		if (stream->read_cb) {
    			stream->read_cb(stream, nread, &buf);
    		} else {
    			// never support recvmsg on udt for now
    			assert(0);
    		}

    		/* Return if we didn't fill the buffer, there is no more data to read. */
    		if (nread < buflen) {
    			return;
    		}
    	}
    }
  }
}


int uvudt_shutdown(uvudt_shutdown_t* req, uvudt_t* stream, uvudt_shutdown_cb cb) {
  uv_poll_t poll = stream->poll;

  assert((poll.type == UV_POLL) &&
         "uvudt_shutdown (unix) only supports uv_handle_t right now");
  assert(stream->udtfd > 0);

  if (!(poll.flags & UV_HANDLE_WRITABLE) ||
      poll.flags & UV_HANDLE_SHUT ||
      poll.flags & UV_HANDLE_CLOSED ||
      poll.flags & UV_HANDLE_CLOSING) {
    ///udt__set_artificial_error(poll.loop, UV_ENOTCONN);
    return -1;
  }

  /* Initialize request */
  ///udt__req_init(poll.loop, req, UV_SHUTDOWN);
  req->req.type = UV_SHUTDOWN;
  req->handle = stream;
  req->cb = cb;
  stream->shutdown_req = req;
  poll.flags |= UV_HANDLE_SHUTTING;

  ///udt__io_start(poll.loop, &stream->write_watcher);
  uv_poll_start(&poll, UV_READABLE | UV_DISCONNECT, udt__stream_io);

  return 0;
}

static void udt__stream_io(uv_poll_t * handle, int status, int events) {
    uvudt_t *stream = (uvudt_t *)handle;

    assert(handle->type == UV_POLL);
    assert(stream->fd > 0);

    // !!! always consume UDT/OSfd event here
    if (stream->fd > 0)
    {
        udt_consume_osfd(stream->fd);
    }

  if (stream->connect_req) {
      udt__stream_connect(stream);
  } else {
	  // check UDT event
      int udtev, optlen;
      
      if (udt_getsockopt(stream->udtfd, 0, UDT_UDT_EVENT, &udtev, &optlen) < 0) {
          // check error anyway
          udt__read(stream);
          
          udt__write(stream);
          udt__write_callbacks(stream);
      } else {
          if (udtev & (UDT_UDT_EPOLL_IN | UDT_UDT_EPOLL_ERR)) {
              udt__read(stream);
          }
          if (udtev & (UDT_UDT_EPOLL_OUT | UDT_UDT_EPOLL_ERR)) {
              udt__write(stream);
              udt__write_callbacks(stream);
		  }
      }
  }
}


/**
 * We get called here from directly following a call to connect(2).
 * In order to determine if we've errored out or succeeded must call
 * getsockopt.
 */
static void udt__stream_connect(uvudt_t* stream) {
  int error;
  uvudt_connect_t* req = stream->connect_req;
  socklen_t errorsize = sizeof(int);
  uv_poll_t poll = stream->poll;

  assert(req);

  if (stream->delayed_error) {
    /* To smooth over the differences between unixes errors that
     * were reported synchronously on the first connect can be delayed
     * until the next tick--which is now.
     */
    error = stream->delayed_error;
    stream->delayed_error = 0;
  } else {
	  /* Normal situation: we need to get the socket error from the kernel. */
	  assert(stream->fd >= 0);
      
      {
		  // notes: check socket state until connect successfully
		  switch (udt_getsockstate(stream->udtfd)) {
		  case UDT_CONNECTED:
			  error = 0;
			  // consume Os fd event
			  ///udt_consume_osfd(stream->fd);
			  break;
		  case UDT_CONNECTING:
			  error = EINPROGRESS;
			  break;
		  default:
			  error = uv_translate_udt_error();
			  // consume Os fd event
			  ///udt_consume_osfd(stream->fd);
			  break;
		  }
	  }
  }

  if (error == EINPROGRESS)
    return;

  stream->connect_req = NULL;
  ///uv__req_unregister(poll.loop, req);

  if (req->cb) {
    //////udt__set_sys_error(poll.loop, error);
    req->cb(req, error ? -1 : 0);
  }
}


/* The buffers to be written must remain valid until the callback is called.
 * This is not required for the uv_buf_t array.
 */
int uvudt_write(uvudt_write_t *req, uvudt_t *stream, const uv_buf_t bufs[], unsigned int nbufs, uvudt_write_cb cb)
{
    int empty_queue;
    uv_poll_t poll = stream->poll;

    if (stream->udtfd < 0)
    {
        //////udt__set_sys_error(poll.loop, EBADF);
        return -1;
    }

    empty_queue = (stream->write_queue_size == 0);

    /* Initialize the req */
    ///udt__req_init(poll.loop, req, UV_WRITE);
    req->req.type = UV_WRITE;
    req->cb = cb;
    req->handle = stream;
    req->error = 0;
    QUEUE_INIT(&req->queue);

    req->bufs = req->bufsml;
    if (nbufs > ARRAY_SIZE(req->bufsml))
        req->bufs = malloc(nbufs * sizeof(bufs[0]));

    if (req->bufs == NULL)
        return UV_ENOMEM;

    memcpy(req->bufs, bufs, nbufs * sizeof(uv_buf_t));
    req->nbufs = nbufs;
    req->write_index = 0;
    stream->write_queue_size += udt__buf_count(bufs, nbufs);

    /* Append the request to write_queue. */
    QUEUE_INSERT_TAIL(&stream->write_queue, &req->queue);

    /* If the queue was empty when this function began, we should attempt to
   * do the write immediately. Otherwise start the write_watcher and wait
   * for the fd to become writable.
   */
    if (stream->connect_req)
    {
        /* Still connecting, do nothing. */
    }
    else if (empty_queue)
    {
        udt__write(stream);
    }
    else
    {/*
     * blocking streams should never have anything in the queue.
     * if this assert fires then somehow the blocking stream isn't being
     * sufficiently flushed in udt__write.
     */
        ///assert(!(poll.flags & UV_HANDLE_BLOCKING_WRITES));
        ///udt__io_start(poll.loop, &stream->write_watcher);
        // start polling
        if (uv_poll_start(&poll, UV_READABLE | UV_DISCONNECT, udt__stream_io) < 0)
        {
            udt_close(stream->udtfd);
            return -1;
        }
    }

    return 0;
}


int uvudt_read_start(uvudt_t *stream, uv_alloc_cb alloc_cb,
                     uvudt_read_cb read_cb)
{
    uv_poll_t poll = stream->poll;

    assert(poll.type == UV_POLL);

    if (poll.flags & UV_HANDLE_CLOSING)
    {
        //////udt__set_sys_error(poll.loop, EINVAL);
        return -1;
    }
    
    /* The UV_HANDLE_READING flag is irrelevant of the state of the tcp - it just
   * expresses the desired state of the user.
   */
    poll.flags |= UV_HANDLE_READING;

    /* TODO: try to do the read inline? */
    /* TODO: keep track of tcp state. If we've gotten a EOF then we should
   * not start the IO watcher.
   */
    assert(stream->fd >= 0);
    assert(alloc_cb);

    stream->read_cb = read_cb;
    stream->alloc_cb = alloc_cb;

    if (uv_poll_start(&poll, UV_READABLE | UV_DISCONNECT, udt__stream_io) < 0)
    {
        udt_close(stream->udtfd);
        return -1;
    }

    return 0;
}


int uvudt_read_stop(uvudt_t* stream) {
  ///udt__io_stop(poll.loop, &stream->read_watcher);
  ///udt__handle_stop(stream);
  uv_poll_t poll = stream->poll;
  uv_poll_stop(&poll);

  poll.flags &= ~UV_HANDLE_READING;
  stream->read_cb = NULL;
  stream->alloc_cb = NULL;
  return 0;
}


int uvudt_is_readable(const uvudt_t* stream) {
    uv_poll_t poll = stream->poll;

    return poll.flags & UV_HANDLE_READABLE;
}


int uvudt_is_writable(const uvudt_t* stream) {
    uv_poll_t poll = stream->poll;

    return poll.flags & UV_HANDLE_WRITABLE;
}


void udt__stream_close(uvudt_t* stream) {
    uv_poll_t poll = stream->poll;

    uvudt_read_stop(stream);
    ///udt__io_stop(handle->loop, &handle->write_watcher);
    
    // clear pending Os fd event
    udt_consume_osfd(stream->fd);
    udt_close(stream->udtfd);
    stream->fd = -1;
    
    if (stream->accepted_fd > 0) {
        // clear pending Os fd event
        udt_consume_osfd(stream->accepted_fd);
        udt_close(stream->accepted_udtfd);
        
        stream->accepted_fd = -1;
  }
}
