#ifndef _WIN32
#include "rpc.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct RpcHeader {
    uint32_t op, len;
} RpcHeader;

static int rpc_fd = -1;

int rp_rpc_connected(void) {
    return rpc_fd >= 0;
}

static int write_all(const void *p, size_t n) {
    const unsigned char *b = (const unsigned char *)p;
    while (n) {
        ssize_t w = write(rpc_fd, b, n);
        if (w < 0 && errno == EINTR)
            continue;
        if (w <= 0)
            return 0;
        b += w;
        n -= (size_t)w;
    }
    return 1;
}

int rp_rpc_write(unsigned int op, const char *json) {
    RpcHeader h;
    size_t n = strlen(json);
    if (!rp_rpc_connected())
        return 0;
    h.op = (uint32_t)op;
    h.len = (uint32_t)n;
    if (!write_all(&h, sizeof(h)) || !write_all(json, n)) {
        rp_rpc_close();
        return 0;
    }
    return 1;
}

static int try_socket(const char *base, int index) {
    struct sockaddr_un a;
    char path[sizeof(a.sun_path)];
    int fd;
    if (!base || !*base)
        return 0;
    snprintf(path, sizeof(path), "%s/discord-ipc-%d", base, index);
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    if (strlen(path) >= sizeof(a.sun_path))
        return 0;
    strcpy(a.sun_path, path);
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return 0;
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0) {
        rpc_fd = fd;
        return 1;
    }
    close(fd);
    return 0;
}

int rp_rpc_connect(const char *id) {
    const char *bases[5];
    char runtime[256];
    char hello[256];
    int b, i;
    if (rp_rpc_connected())
        return 1;
    bases[0] = getenv("XDG_RUNTIME_DIR");
    bases[1] = getenv("TMPDIR");
    bases[2] = "/tmp";
    bases[3] = "/var/tmp";
    bases[4] = NULL;
#ifdef __APPLE__
    const char *home = getenv("HOME");
    if (home) {
        snprintf(runtime, sizeof(runtime), "%s/Library/Application Support", home);
        bases[3] = runtime;
    }
#endif
    for (b = 0; b < 4; b++)
        for (i = 0; i < 10; i++)
            if (try_socket(bases[b], i))
                goto connected;
    return 0;
connected:
    snprintf(hello, sizeof(hello), "{\"v\":1,\"client_id\":\"%s\"}", id);
    return rp_rpc_write(0, hello);
}

void rp_rpc_close(void) {
    if (rpc_fd >= 0)
        close(rpc_fd);
    rpc_fd = -1;
}

unsigned long rp_process_id(void) {
    return (unsigned long)getpid();
}
#endif
