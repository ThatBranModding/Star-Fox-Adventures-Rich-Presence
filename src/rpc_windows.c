#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "rpc.h"

typedef struct RpcHeader {
    unsigned int op, len;
} RpcHeader;

static HANDLE pipe_handle = INVALID_HANDLE_VALUE;

int rp_rpc_connected(void) {
    return pipe_handle != INVALID_HANDLE_VALUE;
}

int rp_rpc_write(unsigned int op, const char *json) {
    RpcHeader h;
    DWORD w;
    size_t n = strlen(json);
    if (!rp_rpc_connected())
        return 0;
    h.op = op;
    h.len = (unsigned int)n;
    if (!WriteFile(pipe_handle, &h, sizeof(h), &w, 0) || w != sizeof(h) ||
        !WriteFile(pipe_handle, json, (DWORD)n, &w, 0) || w != n) {
        rp_rpc_close();
        return 0;
    }
    return 1;
}

int rp_rpc_connect(const char *id) {
    int i;
    char path[64], hello[256];
    if (rp_rpc_connected())
        return 1;
    for (i = 0; i < 10; i++) {
        snprintf(path, sizeof(path), "\\\\?\\pipe\\discord-ipc-%d", i);
        pipe_handle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
        if (rp_rpc_connected())
            break;
    }
    if (!rp_rpc_connected())
        return 0;
    snprintf(hello, sizeof(hello), "{\"v\":1,\"client_id\":\"%s\"}", id);
    return rp_rpc_write(0, hello);
}

void rp_rpc_close(void) {
    if (rp_rpc_connected())
        CloseHandle(pipe_handle);
    pipe_handle = INVALID_HANDLE_VALUE;
}

unsigned long rp_process_id(void) {
    return (unsigned long)GetCurrentProcessId();
}
#endif
