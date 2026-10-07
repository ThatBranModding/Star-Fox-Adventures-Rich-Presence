#ifndef _WIN32
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include "platform.h"

#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#endif

static void *target;
static unsigned char original[16];
static int patched;
static volatile int last = -1;
static RpMapCallback callback;

static void replacement(int mapId) {
    __atomic_store_n(&last, mapId, __ATOMIC_SEQ_CST);
    fprintf(stderr, "[foxhollow] map-loaded id=%d\n", mapId);
    fflush(stderr);
    if (callback)
        callback(mapId);
}

static uintptr_t page_floor(void *p, size_t ps) {
    return (uintptr_t)p & ~((uintptr_t)ps - 1);
}

static uintptr_t page_ceil(void *p, size_t n, size_t ps) {
    return ((uintptr_t)p + n + (uintptr_t)ps - 1) & ~((uintptr_t)ps - 1);
}

static int make_writable(void *p, size_t n) {
    size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    uintptr_t start = page_floor(p, ps);
    size_t span = (size_t)(page_ceil(p, n, ps) - start);
#if defined(__APPLE__)

    if (vm_protect(mach_task_self(), (vm_address_t)start, (vm_size_t)span, FALSE,
                   VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY) == KERN_SUCCESS)
        return 1;
#endif
    return mprotect((void *)start, span, PROT_READ | PROT_WRITE | PROT_EXEC) == 0 ||
           mprotect((void *)start, span, PROT_READ | PROT_WRITE) == 0;
}

static void finish_patch(void *p, size_t n) {
    size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    uintptr_t start = page_floor(p, ps);
    size_t span = (size_t)(page_ceil(p, n, ps) - start);
    mprotect((void *)start, span, PROT_READ | PROT_EXEC);
#if defined(__APPLE__)
    sys_icache_invalidate(p, n);
#else
    __builtin___clear_cache((char *)p, (char *)p + n);
#endif
}

static void build_jump(unsigned char patch[16], const void *destination) {
#if defined(__aarch64__) || defined(__arm64__)

    const uint32_t ldr = 0x58000050u;
    const uint32_t br = 0xd61f0200u;
    const uint64_t dst = (uint64_t)(uintptr_t)destination;
    memcpy(patch + 0, &ldr, 4);
    memcpy(patch + 4, &br, 4);
    memcpy(patch + 8, &dst, 8);
#elif defined(__x86_64__)
    const unsigned char prefix[6] = {0xFF, 0x25, 0, 0, 0, 0};
    const uint64_t dst = (uint64_t)(uintptr_t)destination;
    memset(patch, 0x90, 16);
    memcpy(patch, prefix, sizeof(prefix));
    memcpy(patch + 6, &dst, 8);
#else
#error Unsupported Unix architecture for map breadcrumb capture
#endif
}

int rp_platform_install_map_capture(FhMod *mod, const FhModHost *host, RpMapCallback cb) {
    unsigned char patch[16];
    callback = cb;
    if (!host || !host->symbolAddress)
        return 0;
    target = host->symbolAddress(mod, "fhNoteMapLoaded");
    if (!target)
        return 0;

#if defined(__x86_64__) || defined(__aarch64__) || defined(__arm64__)
    memcpy(original, target, 16);
    build_jump(patch, (const void *)&replacement);
    if (!make_writable(target, 16))
        return 0;
    memcpy(target, patch, 16);
    finish_patch(target, 16);
    patched = 1;
    return 1;
#else
    return 0;
#endif
}

void rp_platform_remove_map_capture(FhMod *mod, const FhModHost *host) {
    (void)mod;
    (void)host;
    if (patched && target && make_writable(target, 16)) {
        memcpy(target, original, 16);
        finish_patch(target, 16);
    }
    patched = 0;
    target = NULL;
    callback = NULL;
}

int rp_platform_atomic_map_get(void) {
    return __atomic_load_n(&last, __ATOMIC_SEQ_CST);
}

void rp_platform_atomic_map_set(int m) {
    __atomic_store_n(&last, m, __ATOMIC_SEQ_CST);
}
#endif
