#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "platform.h"
static void* target;static unsigned char original[16];static int patched;static volatile LONG last=-1;static RpMapCallback callback;
static void replacement(int mapId){InterlockedExchange(&last,(LONG)mapId);fprintf(stderr,"[foxhollow] map-loaded id=%d\n",mapId);fflush(stderr);if(callback)callback(mapId);}
int rp_platform_install_map_capture(FhMod* mod,const FhModHost* host,RpMapCallback cb){unsigned char patch[16]={0xFF,0x25,0,0,0,0,0,0,0,0,0,0,0,0,0x90,0x90};DWORD oldp=0,ignored=0;uint64_t dst=(uint64_t)(uintptr_t)&replacement;callback=cb;if(!host||!host->symbolAddress)return 0;target=host->symbolAddress(mod,"fhNoteMapLoaded");if(!target)return 0;memcpy(original,target,16);memcpy(patch+6,&dst,8);if(!VirtualProtect(target,16,PAGE_EXECUTE_READWRITE,&oldp))return 0;memcpy(target,patch,16);FlushInstructionCache(GetCurrentProcess(),target,16);VirtualProtect(target,16,oldp,&ignored);patched=1;return 1;}
void rp_platform_remove_map_capture(FhMod* mod,const FhModHost* host){DWORD oldp=0,ignored=0;(void)mod;(void)host;if(patched&&target&&VirtualProtect(target,16,PAGE_EXECUTE_READWRITE,&oldp)){memcpy(target,original,16);FlushInstructionCache(GetCurrentProcess(),target,16);VirtualProtect(target,16,oldp,&ignored);}patched=0;target=NULL;callback=NULL;}
int rp_platform_atomic_map_get(void){return (int)InterlockedCompareExchange(&last,-1,-1);}void rp_platform_atomic_map_set(int m){InterlockedExchange(&last,(LONG)m);}
#endif
