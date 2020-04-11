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

#include "uv.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>


#define SERVER_MAX_NUM 10

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;

static uv_loop_t* loop;

static stream_type serverType;
static uv_udt_t udtServer[SERVER_MAX_NUM];

static void after_write(uv_write_t* req, int status);
static void after_read(uv_stream_t*, ssize_t nread, uv_buf_t buf);
static void on_close(uv_handle_t* peer);
static void on_server_close(uv_handle_t* handle);
static void on_connection(uv_stream_t*, int status);


static void after_write(uv_write_t* req, int status) {
  write_req_t* wr;
  ///printf("%s.%d\n", __FUNCTION__, __LINE__);
  if (status) {
    uv_err_t err = uv_last_error(loop);
    fprintf(stderr, "uv_write error: %s\n", uv_strerror(err));
    ASSERT(0);
  }

  wr = (write_req_t*) req;

  /* Free the read/write buffer and the request */
  free(wr->buf.base);
  free(wr);
}


static void after_shutdown(uv_shutdown_t* req, int status) {
  ///printf("%s.%d\n", __FUNCTION__, __LINE__);
  uv_close((uv_handle_t*)req->handle, on_close);
  free(req);
}


static void after_read(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
  int i;
  write_req_t *wr;
  uv_shutdown_t* req;

  ///printf("%s.%d\n", __FUNCTION__, __LINE__);
  if (nread < 0) {
	  ///printf("%s.%d\n", __FUNCTION__, __LINE__);

	  /* Error or EOF */
	  if (uv_last_error(loop).code == UV_EOF) {
		  ///printf("%s.%d\n", __FUNCTION__, __LINE__);

		  if (buf.base) {
			  free(buf.base);
		  }

		  req = (uv_shutdown_t*) malloc(sizeof *req);
		  uv_shutdown(req, handle, after_shutdown);
	  } else {
		  ///printf("%s.%d\n", __FUNCTION__, __LINE__);

		  if (buf.base) {
			  free(buf.base);
		  }
		  uv_close((uv_handle_t*)handle, on_close);
	  }
	  return;
  }

  if (nread == 0) {
    /* Everything OK, but nothing read. */
    free(buf.base);
    return;
  }

  ///fprintf(stdout, "echo server recv: %dbytes\n", nread);
  ///fprintf(stdout, ".");
  ///free(buf.base);
  ///return;

  /*
   * Scan for the letter Q which signals that we should quit the server.
   * If we get QS it means close the stream.
   */
    for (i = 0; i < nread; i++) {
      if (buf.base[i] == 'Q') {
        if (i + 1 < nread && buf.base[i + 1] == 'S') {
          free(buf.base);
          uv_close((uv_handle_t*)handle, on_close);
          return;
        } else {
          uv_close((uv_handle_t*)(((uv_handle_t*)handle)->data), on_server_close);
        }
      }
    }

  wr = (write_req_t*) malloc(sizeof *wr);

  wr->buf = uv_buf_init(buf.base, nread);
  if (uv_write(&wr->req, handle, &wr->buf, 1, after_write)) {
    FATAL("uv_write failed");
  }
}


static void on_close(uv_handle_t* peer) {
  ///printf("%s.%d\n", __FUNCTION__, __LINE__);
  free(peer);
}


static uv_buf_t echo_alloc(uv_handle_t* handle, size_t suggested_size) {
  return uv_buf_init(malloc(suggested_size), suggested_size);
}


static void on_connection(uv_stream_t* server, int status) {
  uv_stream_t* stream;
  int r;

  if (status != 0) {
    fprintf(stderr, "Connect error %d\n",
        uv_last_error(loop).code);
  }
  ASSERT(status == 0);

  switch (serverType) {
  case TCP:
    stream = malloc(sizeof(uv_tcp_t));
    ASSERT(stream != NULL);
    r = uv_tcp_init(loop, (uv_tcp_t*)stream);
    ASSERT(r == 0);
    break;

  case UDT:
      stream = malloc(sizeof(uv_udt_t));
      ASSERT(stream != NULL);
      r = uv_udt_init(loop, (uv_udt_t*)stream);
      ASSERT(r == 0);
      break;

  case PIPE:
    stream = malloc(sizeof(uv_pipe_t));
    ASSERT(stream != NULL);
    r = uv_pipe_init(loop, (uv_pipe_t*)stream, 0);
    ASSERT(r == 0);
    break;

  default:
    ASSERT(0 && "Bad serverType");
    abort();
  }

  /* associate server with stream */
  stream->data = server;

  r = uv_accept(server, stream);
  ASSERT(r == 0);
  if (r) {
	  free(stream);
	  return;
  }

  r = uv_read_start(stream, echo_alloc, after_read);
  ASSERT(r == 0);
  if (r) {
	  free(stream);
	  return;
  }
}


static void on_server_close(uv_handle_t* handle) {
  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
  ///ASSERT(handle == server);
}


#if 0
static int tcp6_echo_start(int port) {
  struct sockaddr_in6 addr6 = uv_ip6_addr("::1", port);
  int r;

  server = (uv_handle_t*)&tcpServer;
  serverType = TCP;

  r = uv_tcp_init(loop, &tcpServer);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Socket creation error\n");
    return 1;
  }

  /* IPv6 is optional as not all platforms support it */
  r = uv_tcp_bind6(&tcpServer, addr6);
  if (r) {
    /* show message but return OK */
    fprintf(stderr, "IPv6 not supported\n");
    return 0;
  }

  r = uv_listen((uv_stream_t*)&tcpServer, SOMAXCONN, on_connection);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Listen error\n");
    return 1;
  }

  return 0;
}
#endif

static int udt4_echo_start(int port) {
	struct sockaddr_in addr;
	int r;
	int i;

	serverType = UDT;
	for (i = 0; i < (sizeof(udtServer) / sizeof(udtServer[0])); i ++) {
		addr = uv_ip4_addr("0.0.0.0", port+i);

		r = uv_udt_init(loop, &udtServer[i]);
		if (r) {
			/* TODO: Error codes */
			fprintf(stderr, "Socket creation error\n");
			return 1;
		}

		r = uv_udt_bind(&udtServer[i], addr);
		if (r) {
			/* TODO: Error codes */
			fprintf(stderr, "Bind error\n");
			return 1;
		}

		r = uv_listen((uv_stream_t*)&udtServer[i], SOMAXCONN, on_connection);
		if (r) {
			/* TODO: Error codes */
			fprintf(stderr, "Listen error %s\n",
					uv_err_name(uv_last_error(loop)));
			return 1;
		}
	}

	return 0;
}

int main(int argc, char * argv [])
{
	loop = uv_default_loop();

	if (argc == 2) {
		if (udt4_echo_start(atoi(argv[1])))
			return 1;
	} else {
		if (udt4_echo_start(TEST_PORT))
			return 1;
	}

	uv_run(loop);
	return 0;
}
