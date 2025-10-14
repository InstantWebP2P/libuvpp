#include "uvkcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uv_loop_t* loop;

static void on_connect(uvkcp_connect_t* req, int status) {
    printf("Connection callback: status=%d\n", status);
    if (status == 0) {
        printf("Connection successful!\n");
    } else {
        printf("Connection failed: %d\n", status);
    }
    uv_stop(loop);
}

int main() {
    loop = uv_default_loop();

    uvkcp_t kcp;
    uvkcp_connect_t connect_req;
    struct sockaddr_in addr;

    // Initialize KCP
    int r = uvkcp_init(loop, &kcp);
    if (r != 0) {
        printf("uvkcp_init failed: %d\n", r);
        return 1;
    }

    // Bind to local address
    uv_ip4_addr("0.0.0.0", 0, &addr);
    r = uvkcp_bind(&kcp, (const struct sockaddr*)&addr, 1, 1);
    if (r != 0) {
        printf("uvkcp_bind failed: %d\n", r);
        return 1;
    }

    // Try to connect to non-existent server
    uv_ip4_addr("127.0.0.1", 51686, &addr);
    r = uvkcp_connect(&connect_req, &kcp, (const struct sockaddr*)&addr, on_connect);
    if (r != 0) {
        printf("uvkcp_connect failed: %d\n", r);
        return 1;
    }

    printf("Starting event loop...\n");
    uv_run(loop, UV_RUN_DEFAULT);

    printf("Test completed\n");
    return 0;
}