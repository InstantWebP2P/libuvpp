#include "uvkcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static void test_simple_bind() {
    uvkcp_t kcp;
    struct sockaddr_in addr;
    int r;

    printf("Testing simple KCP bind...\n");

    // Test initialization
    r = uvkcp_init(uv_default_loop(), &kcp);
    if (r != 0) {
        printf("ERROR: uvkcp_init failed: %d\n", r);
        return;
    }
    printf("uvkcp_init: OK\n");

    // Test binding
    r = uv_ip4_addr("0.0.0.0", 51688, &addr);
    if (r != 0) {
        printf("ERROR: uv_ip4_addr failed: %d\n", r);
        return;
    }
    printf("uv_ip4_addr: OK\n");

    printf("Attempting to bind to 0.0.0.0:51688\n");
    r = uvkcp_bind(&kcp, (const struct sockaddr*)&addr, 1, 1);
    if (r != 0) {
        printf("ERROR: uvkcp_bind failed: %d\n", r);
        printf("Error string: %s\n", uv_strerror(r));

        // Let's try to create a UDP socket manually to see what's wrong
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            printf("Manual socket creation failed: %s\n", strerror(errno));
        } else {
            printf("Manual socket creation: OK\n");

            int optval = 1;
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
                printf("Manual setsockopt failed: %s\n", strerror(errno));
            } else {
                printf("Manual setsockopt: OK\n");
            }

            if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                printf("Manual bind failed: %s\n", strerror(errno));
            } else {
                printf("Manual bind: OK\n");
                close(sock);
            }
        }
        return;
    }
    printf("uvkcp_bind: OK\n");

    printf("KCP bind test completed successfully\n");

    // Cleanup
    uvkcp_close(&kcp, NULL);
}

int main() {
    test_simple_bind();
    return 0;
}