#include "uvkcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uv_loop_t* loop;

static void on_close(uv_handle_t* handle) {
    printf("KCP handle closed\n");
}

static void after_write(uvkcp_write_t* req, int status) {
    printf("Write completed with status: %d\n", status);
    free(req);
}

static void after_read(uvkcp_t* handle, ssize_t nread, const uv_buf_t* buf) {
    printf("Read completed with nread: %zd\n", nread);
    if (buf->base) {
        free(buf->base);
    }

    if (nread < 0) {
        uvkcp_close(handle, on_close);
    }
}

static void echo_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static void on_connection(uvkcp_t* server, int status) {
    printf("Connection callback with status: %d\n", status);
}

static void test_basic_kcp() {
    uvkcp_t kcp;
    struct sockaddr_in addr;
    int r;

    printf("Testing basic KCP functionality...\n");

    // Test initialization
    r = uvkcp_init(loop, &kcp);
    if (r != 0) {
        printf("ERROR: uvkcp_init failed: %d\n", r);
        return;
    }
    printf("uvkcp_init: OK\n");

    // Test binding
    r = uv_ip4_addr("127.0.0.1", 51687, &addr);
    if (r != 0) {
        printf("ERROR: uv_ip4_addr failed: %d\n", r);
        return;
    }

    r = uvkcp_bind(&kcp, (const struct sockaddr*)&addr, 1, 1);
    if (r != 0) {
        printf("ERROR: uvkcp_bind failed: %d\n", r);
        return;
    }
    printf("uvkcp_bind: OK\n");

    // Test listen
    r = uvkcp_listen(&kcp, 10, on_connection);
    if (r != 0) {
        printf("ERROR: uvkcp_listen failed: %d\n", r);
        return;
    }
    printf("uvkcp_listen: OK\n");

    // Test read start
    r = uvkcp_read_start(&kcp, echo_alloc, after_read);
    if (r != 0) {
        printf("ERROR: uvkcp_read_start failed: %d\n", r);
        return;
    }
    printf("uvkcp_read_start: OK\n");

    printf("Basic KCP test completed successfully\n");

    // Cleanup
    uvkcp_close(&kcp, on_close);
}

int main() {
    loop = uv_default_loop();

    test_basic_kcp();

    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}