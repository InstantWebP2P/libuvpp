
#include "uvudt.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define TEST_PORT (51686)

typedef struct {
  uvudt_write_t req;
  uv_buf_t buf;
} write_req_t;

static uv_loop_t* loop;

static int server_closed;
static uvudt_t udtServer;
static uvudt_t *server;
static uvudt_t udt6Server;
static uvudt_t *server6;

static void after_write(uvudt_write_t* req, int status);
static void after_read(uvudt_t*, ssize_t nread, const uv_buf_t* buf);
static void on_close(uv_handle_t* peer);
static void on_server_close(uv_handle_t* handle);
static void on_connection(uvudt_t*, int status);


static void after_write(uvudt_write_t* req, int status) {
  write_req_t* wr;

  /* Free the read/write buffer and the request */
  wr = (write_req_t*) req;
  free(wr->buf.base);
  free(wr);

  if (status == 0)
    return;

  fprintf(stderr,
          "uvudt_write error: %s - %s\n",
          uv_err_name(status),
          uv_strerror(status));
}


static void after_shutdown(uvudt_shutdown_t* req, int status) {
  uvudt_close( req->handle, on_close);
  free(req);
}


static void after_read(uvudt_t* handle,
                       ssize_t nread,
                       const uv_buf_t* buf) {
  int i;
  write_req_t *wr;
  uvudt_shutdown_t* sreq;

  if (nread < 0) {
    /* Error or EOF */
    assert(nread == UV_EOF);

    free(buf->base);
    sreq = malloc(sizeof* sreq);
    assert(0 == uvudt_shutdown(sreq, handle, after_shutdown));
    return;
  }

  if (nread == 0) {
    /* Everything OK, but nothing read. */
    free(buf->base);
    return;
  }

  wr = (write_req_t*) malloc(sizeof *wr);
  assert(wr != NULL);
  wr->buf = uv_buf_init(buf->base, nread);

  if (uvudt_write(&wr->req, handle, &wr->buf, 1, after_write)) {
    printf("uvudt_write failed");
  }
}


static void on_close(uv_handle_t* peer) {
  free(peer);
}


static void echo_alloc(uv_handle_t* handle,
                       size_t suggested_size,
                       uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

static void slab_alloc(uv_handle_t* handle,
                       size_t suggested_size,
                       uv_buf_t* buf) {
  /* up to 16 datagrams at once */
  static char slab[16 * 64 * 1024];
  buf->base = slab;
  buf->len = sizeof(slab);
}

static void on_connection(uvudt_t* server, int status) {
  uvudt_t* stream;
  int r;

  if (status != 0) {
    fprintf(stderr, "Connect error %s\n", uv_err_name(status));
  } else printf("Connect success\n");
  assert(status == 0);

    stream = malloc(sizeof(uvudt_t));
    assert(stream != NULL);
    r = uvudt_init(loop, stream);
    assert(r == 0);

  /* associate server with stream */
  stream->poll.data = server;

  r = uvudt_accept(server, stream);
  assert(r == 0);

  r = uvudt_read_start(stream, echo_alloc, after_read);

  assert(r == 0);
}


static void on_server_close(uv_handle_t* handle) {
  assert(handle == server);
}

static int udt4_echo_start(int port) {
  struct sockaddr_in addr;
  int r;

  assert(0 == uv_ip4_addr("0.0.0.0", port, &addr));

  server = &udtServer;

  r = uvudt_init(loop, &udtServer);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Socket creation error\n");
    return 1;
  }

  r = uvudt_bind(&udtServer, (const struct sockaddr*) &addr, 1, 1);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Bind error\n");
    return 1;
  }

  r = uvudt_listen((uvudt_t*)&udtServer, SOMAXCONN, on_connection);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Listen error %s\n", uv_err_name(r));
    return 1;
  }

  return 0;
}


static int udt6_echo_start(int port) {
  struct sockaddr_in6 addr6;
  int r;

  assert(0 == uv_ip6_addr("::1", port, &addr6));

  server6 = &udt6Server;

  r = uvudt_init(loop, &udt6Server);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Socket creation error\n");
    return 1;
  }

  /* IPv6 is optional as not all platforms support it */
  r = uvudt_bind(&udt6Server, (const struct sockaddr*) &addr6, 1, 1);
  if (r) {
    /* show message but return OK */
    fprintf(stderr, "IPv6 not supported\n");
    return 0;
  }

  r = uvudt_listen((uvudt_t*)&udt6Server, SOMAXCONN, on_connection);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Listen error\n");
    return 1;
  }

  return 0;
}


int main(int argc, char *argv[]) {
  loop = uv_default_loop();

  if (udt4_echo_start(TEST_PORT))
    return 1;

  if (udt6_echo_start(TEST_PORT))
    return 1;

  uv_run(loop, UV_RUN_DEFAULT);
  return 0;
}
