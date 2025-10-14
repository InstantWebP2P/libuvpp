#include "uvkcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static uv_loop_t* loop;

static void on_close(uv_handle_t* handle) {
    printf("KCP handle closed\n");
}

static void test_bind() {
    uvkcp_t kcp;
    struct sockaddr_in addr;
    int r;

    printf("Testing KCP bind...\n");

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
    printf("uv_ip4_addr: OK\n");

    printf("Attempting to bind to 127.0.0.1:51687\n");
    r = uvkcp_bind(&kcp, (const struct sockaddr*)&addr, 1, 1);
    if (r != 0) {
        printf("ERROR: uvkcp_bind failed: %d (errno: %d)\n", r, errno);
        printf("Error string: %s\n", uv_strerror(r));
        return;
    }
    printf("uvkcp_bind: OK\n");

    printf("KCP bind test completed successfully\n");

    // Cleanup
    uvkcp_close(&kcp, on_close);
}

int main() {
    loop = uv_default_loop();

    test_bind();

    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}