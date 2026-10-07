#ifndef SFA_RP_PLATFORM_H
#define SFA_RP_PLATFORM_H
#include "foxhollow_mod_api.h"
typedef void (*RpMapCallback)(int mapId);
int rp_platform_install_map_capture(FhMod *mod, const FhModHost *host, RpMapCallback cb);
void rp_platform_remove_map_capture(FhMod *mod, const FhModHost *host);
int rp_platform_atomic_map_get(void);
void rp_platform_atomic_map_set(int mapId);
#endif
