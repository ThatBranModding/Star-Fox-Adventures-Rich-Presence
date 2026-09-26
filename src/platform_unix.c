#ifndef _WIN32
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include "platform.h"
static void* target;static unsigned char original[16];static int patched;static volatile int last=-1;static RpMapCallback callback;
static void replacement(int mapId){__atomic_store_n(&last,mapId,__ATOMIC_SEQ_CST);fprintf(stderr,"[foxhollow] map-loaded id=%d\n",mapId);fflush(stderr);if(callback)callback(mapId);}
#if defined(__x86_64__)
static int make_writable(void* p,size_t n,int prot){long ps=sysconf(_SC_PAGESIZE);uintptr_t start=(uintptr_t)p&~((uintptr_t)ps-1);uintptr_t end=((uintptr_t)p+n+(uintptr_t)ps-1)&~((uintptr_t)ps-1);return mprotect((void*)start,end-start,prot)==0;}
#endif
int rp_platform_install_map_capture(FhMod* mod,const FhModHost* host,RpMapCallback cb){callback=cb;if(!host||!host->symbolAddress)return 0;target=host->symbolAddress(mod,"fhNoteMapLoaded");if(!target)return 0;
#if defined(__x86_64__)
 {unsigned char patch[16]={0xFF,0x25,0,0,0,0,0,0,0,0,0,0,0,0,0x90,0x90};uint64_t dst=(uint64_t)(uintptr_t)&replacement;memcpy(original,target,16);memcpy(patch+6,&dst,8);if(!make_writable(target,16,PROT_READ|PROT_WRITE|PROT_EXEC))return 0;memcpy(target,patch,16);__builtin___clear_cache((char*)target,(char*)target+16);make_writable(target,16,PROT_READ|PROT_EXEC);patched=1;return 1;}
#else
 /* arm64/macOS needs a platform-safe hook path; do not patch unknown instructions. */
 return 0;
#endif
}
void rp_platform_remove_map_capture(FhMod* mod,const FhModHost* host){(void)mod;(void)host;
#if defined(__x86_64__)
 if(patched&&target&&make_writable(target,16,PROT_READ|PROT_WRITE|PROT_EXEC)){memcpy(target,original,16);__builtin___clear_cache((char*)target,(char*)target+16);make_writable(target,16,PROT_READ|PROT_EXEC);}
#endif
 patched=0;target=NULL;callback=NULL;}
int rp_platform_atomic_map_get(void){return __atomic_load_n(&last,__ATOMIC_SEQ_CST);}void rp_platform_atomic_map_set(int m){__atomic_store_n(&last,m,__ATOMIC_SEQ_CST);}
#endif
