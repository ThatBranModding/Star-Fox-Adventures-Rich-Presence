#ifndef FOXHOLLOW_MOD_API_H_
#define FOXHOLLOW_MOD_API_H_
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FH_MOD_ABI_VERSION 2u
#define FH_MOD_OK 0
#define FH_MOD_ERROR 1
#if defined(_WIN32)
#define FH_MOD_EXPORT __declspec(dllexport)
#else
#define FH_MOD_EXPORT __attribute__((visibility("default")))
#endif
typedef struct FhMod FhMod;
typedef enum FhLogLevel { FH_LOG_INFO=0, FH_LOG_WARN=1, FH_LOG_ERROR=2 } FhLogLevel;
typedef enum FhClassSlot { FH_SLOT_02=0,FH_SLOT_INIT=1,FH_SLOT_UPDATE=2,FH_SLOT_HIT_DETECT=3,FH_SLOT_RENDER=4,FH_SLOT_FREE=5,FH_SLOT_GET_TYPE_ID=6,FH_SLOT_GET_EXTRA_SIZE=7 } FhClassSlot;
typedef void (*FhClassCallback)(void);
typedef struct FhModHost {
 uint32_t structSize; uint32_t abiVersion;
 const char* (*modId)(FhMod*); const char* (*modDir)(FhMod*); void (*log)(FhMod*,FhLogLevel,const char*); uint64_t (*frameCount)(FhMod*);
 uint32_t (*classCount)(FhMod*); int (*classReplaceCallback)(FhMod*,uint32_t,FhClassSlot,FhClassCallback,FhClassCallback*);
 void* (*symbolAddress)(FhMod*,const char*); int (*hookInstall)(FhMod*,void*,void*,void**); int (*hookRemove)(FhMod*,void*);
} FhModHost;
#ifdef __cplusplus
}
#endif
#endif
