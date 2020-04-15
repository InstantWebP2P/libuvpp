#include "uvudt.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* strlen */
#include <assert.h>

#define TEST_PORT (51686)

#define SERVER_MAX_NUM 10
#define CLIENT_MAX_NUM 10

/* Run the benchmark for this many ms */
#define TIME 2000

typedef struct {
  int domain;    
  int pongs;
  int state;
  uvudt_t udt;
  uvudt_connect_t connect_req;
  uvudt_shutdown_t shutdown_req;
} pinger_t;

typedef struct buf_s {
  uv_buf_t uv_buf_t;
  struct buf_s* next;
} buf_t;


static char PING[] = "PING\n";

static uv_loop_t* loop;

static buf_t* buf_freelist = NULL;
static int64_t start_time;


static void buf_alloc(uv_handle_t* udt, size_t size, uv_buf_t *buf) {
  buf_t* ab;

  ab = buf_freelist;

  if (ab != NULL) {
    buf_freelist = ab->next;
    *buf = ab->uv_buf_t;
    return;
  }

  ab = (buf_t*) malloc(size + sizeof *ab);
  ab->uv_buf_t.len = size;
  ab->uv_buf_t.base = ((char*) ab) + sizeof *ab;

  *buf = ab->uv_buf_t;
}


static void buf_free(uv_buf_t * buf) {
  buf_t* ab = (buf_t*) (buf->base - sizeof *ab);

  ab->next = buf_freelist;
  buf_freelist = ab;
}


static void pinger_close_cb(uv_handle_t* handle) {
  pinger_t* pinger;

  pinger = (pinger_t*)handle->data;
  printf("ping_pongs: %d roundtrips/s in ip%d\n", (1000 * pinger->pongs) / TIME, pinger->domain);

  free(pinger);
}


static void pinger_write_cb(uvudt_write_t* req, int status) {
  assert(status == 0);

  free(req);
}


static void pinger_write_ping(pinger_t* pinger) {
  uvudt_write_t* req;
  uv_buf_t buf;

  buf.base = (char*)&PING;
  buf.len = strlen(PING);

  req = malloc(sizeof *req);
  if (uvudt_write(req, (uvudt_t*) &pinger->udt, &buf, 1, pinger_write_cb)) {
    printf("uvudt_write failed");
  }
}


static void pinger_shutdown_cb(uvudt_shutdown_t* req, int status) {
  uvudt_close(req->handle, pinger_close_cb);

  assert(status == 0);
}


static void pinger_read_cb(uvudt_t* udt, ssize_t nread, uv_buf_t * buf) {
  ssize_t i;
  pinger_t* pinger;


  pinger = (pinger_t*)udt->poll.data;

  if (nread < 0) {
    if (buf->base) {
      buf_free(buf);
    }

    assert(nread == UV_EOF);

    return;
  }

  /* Now we count the pings */
  for (i = 0; i < nread; i++) {
    assert(buf->base[i] == PING[pinger->state]);
    pinger->state = (pinger->state + 1) % (sizeof(PING) - 1);
    if (pinger->state == 0) {
      pinger->pongs++;
      if (uv_now(loop) - start_time > TIME) {
        uvudt_shutdown(&pinger->shutdown_req, udt, pinger_shutdown_cb);
        break;
      } else {
        pinger_write_ping(pinger);
      }
    }
  }

  buf_free(buf);
}


static void pinger_connect_cb(uvudt_connect_t* req, int status) {
  pinger_t *pinger = (pinger_t*)req->handle->poll.data;

  printf("Connect success\n");

  assert(status == 0);

  pinger_write_ping(pinger);

  if (uvudt_read_start(req->handle, buf_alloc, pinger_read_cb)) {
    printf("uvudt_read_start failed");
  }
}


static void pinger_new(int port) {
  int r;
  struct sockaddr_in client_addr; 
  uv_ip4_addr("0.0.0.0", 0, &client_addr);

  struct sockaddr_in server_addr; 
  uv_ip4_addr("127.0.0.1", port, &server_addr);
  pinger_t *pinger;

  pinger = (pinger_t*)malloc(sizeof(*pinger));
  pinger->domain= 4;
  pinger->state = 0;
  pinger->pongs = 0;

  /* Try to connect to the server and do NUM_PINGS ping-pongs. */
  r = uvudt_init(loop, &pinger->udt);
  assert(!r);

  pinger->udt.poll.data = pinger;

  uvudt_bind(&pinger->udt, &client_addr, 1, 1);

  r = uvudt_connect(&pinger->connect_req, &pinger->udt, &server_addr, pinger_connect_cb);
  assert(!r);
}

static void pinger_new6(int port)
{
    int r;
    struct sockaddr_in6 client_addr;
    uv_ip6_addr("::", 0, &client_addr);

    struct sockaddr_in6 server_addr;
    uv_ip6_addr("::1", port, &server_addr);
    pinger_t *pinger;

    pinger = (pinger_t *)malloc(sizeof(*pinger));
    pinger->domain= 6;
    pinger->state = 0;
    pinger->pongs = 0;

    /* Try to connect to the server and do NUM_PINGS ping-pongs. */
    r = uvudt_init(loop, &pinger->udt);
    assert(!r);

    pinger->udt.poll.data = pinger;

    uvudt_bind(&pinger->udt, &client_addr, 1, 1);

    r = uvudt_connect(&pinger->connect_req, &pinger->udt, &server_addr, pinger_connect_cb);
    assert(!r);
}

int main(int argc, char * argv [])
{
    int srvNum=1;
	int clnNum=1;
	int port = TEST_PORT;
	int i=0;
	int j=0;
	loop = uv_default_loop();

	start_time = uv_now(loop);

	if (argc == 4) {
	    port    = atoi(argv[1]);
	    srvNum  = atoi(argv[2]);
		clnNum  = atoi(argv[3]);
	} else if (argc == 3) {
	    port   = atoi(argv[1]);
	    srvNum = atoi(argv[2]);
	} else if (argc == 2) {
	    port = atoi(argv[1]);
	}
	srvNum = (srvNum < SERVER_MAX_NUM)? srvNum : SERVER_MAX_NUM;
	clnNum = (clnNum < CLIENT_MAX_NUM)? clnNum : CLIENT_MAX_NUM;
	
	for (i = 0; i < srvNum; i++)
		for (j = 0; j < clnNum; j++) {
            pinger_new(port+i);
            pinger_new6(port+i);
        }

    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}
