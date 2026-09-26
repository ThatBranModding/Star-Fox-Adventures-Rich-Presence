#include "foxhollow_mod_api.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static const FhModHost* g_host;
static FhMod* g_mod;
static void* (*g_SaveGame_getCurCharPos)(void);
static int (*g_mapCoordsToId)(int,int,int);
static float* g_mapBlockWorldSizePtr;
static unsigned char* g_mapLoaded;
static int g_space_override=0;
static unsigned char** g_gameBitSaveDataPtr;
static uint64_t g_frame;
static int g_last_map=-9999;
static time_t g_started;
static char g_app_id[64]="1553099945110806538";
static char g_large_image[64]="star_fox_adventures_v2";
static char g_large_text[128]="Star Fox Adventures";
#ifdef _WIN32
static int rpc_connect(void);
static void rpc_presence(int map);
static HANDLE g_pipe=INVALID_HANDLE_VALUE;
static void* g_fhNoteMapLoadedTarget=NULL;
static unsigned char g_fhNoteMapLoadedOriginal[16];
static int g_fhNoteMapLoadedPatched=0;
static volatile LONG g_breadcrumbMap=-1;
static void (*g_orig_warpToMap)(int,int)=NULL;
static void* g_warpToMapTarget=NULL;
static int g_warpToMapHooked=0;
#endif

typedef struct SfaSaveGameCharacterPosition { float x,y,z; signed char angle,mapLayer,mapDataFileId; unsigned char padF; } SfaSaveGameCharacterPosition;
static int floor_cell(float v,float block){ int i=(int)(v/block); if(v<0.0f && (float)i*block!=v) --i; return i; }
static int layer_to_index(int layer){ return layer; }
static void logmsg(FhLogLevel l,const char* s){ if(g_host&&g_host->log&&g_mod) g_host->log(g_mod,l,s); }

static const char* map_name(int m){
 switch(m){
  case 0x00:return "Scales Galleon"; case 0x02:return "Dragon Rock"; case 0x04:return "Volcano Force Point";
  case 0x07:return "ThornTail Hollow"; case 0x08:return "ThornTail Hollow Underground"; case 0x0A:return "SnowHorn Wastes";
  case 0x0B:return "Krazoa Palace"; case 0x0C:return "CloudRunner Fortress"; case 0x0D:return "Walled City";
  case 0x0E:return "LightFoot Village"; case 0x10:return "CloudRunner Dungeon"; case 0x12:return "Moon Mountain Pass";
  case 0x13:return "DarkIce Mines"; case 0x15:return "Ocean Force Point"; case 0x17:return "Ice Mountain";
  case 0x1B:return "DarkIce Mines"; case 0x1C:return "Galdon"; case 0x1D:return "Cape Claw";
  case 0x1F:return "Test of Combat"; case 0x20:return "Test of Fear"; case 0x21:return "Test of Skill";
  case 0x22:return "Test of Knowledge"; case 0x26:return "Andross"; case 0x27:return "Test of Strength";
  case 0x28:return "General Scales"; case 0x29:return "World Map"; case 0x2B:return "CloudRunner Race";
  case 0x2C:return "Drakor"; case 0x30:return "RedEye King"; case 0x32:return "Ocean Force Point"; case 0x33:return "ThornTail Store";
  case 0x34:return "Dragon Rock"; case 0x36:return "Magic Cave"; case 0x3A:return "Arwing Flight";
  case 0x3B:return "Arwing Flight to DarkIce Mines"; case 0x3C:return "Arwing Flight to CloudRunner Fortress";
  case 0x3D:return "Arwing Flight to Walled City"; case 0x3E:return "Arwing Flight to Dragon Rock";
  case 0x3F:return "Title Screen"; case 0x41:return "Great Fox"; default:return "Dinosaur Planet";
 }
}
static int current_map(void){
 SfaSaveGameCharacterPosition* p; float b;
 if(!g_SaveGame_getCurCharPos||!g_mapCoordsToId||!g_mapBlockWorldSizePtr) return -1;
 p=(SfaSaveGameCharacterPosition*)g_SaveGame_getCurCharPos(); if(!p) return -1;
 b=*g_mapBlockWorldSizePtr; if(b<1.0f)b=640.0f;
 return g_mapCoordsToId(floor_cell(p->x,b),floor_cell(p->z,b),layer_to_index(p->mapLayer));
}
static void trim(char* s){ char* p=s; size_t n; while(*p==' '||*p=='\t')p++; if(p!=s)memmove(s,p,strlen(p)+1); n=strlen(s); while(n&&(s[n-1]=='\r'||s[n-1]=='\n'||s[n-1]==' '||s[n-1]=='\t'))s[--n]=0; }
static void load_config(void){
 char path[1024],line[512]; FILE* f; const char* d=(g_host&&g_host->modDir)?g_host->modDir(g_mod):0;
 if(!d)return; snprintf(path,sizeof(path),"%s\\discord_presence.ini",d); f=fopen(path,"rb");
 if(!f){ logmsg(FH_LOG_INFO,"Discord Rich Presence: using built-in Star Fox Adventures configuration."); return; }
 while(fgets(line,sizeof(line),f)){ char* eq; trim(line); if(!line[0]||line[0]=='#'||line[0]==';')continue; eq=strchr(line,'='); if(!eq)continue; *eq++=0; trim(line); trim(eq);
  if(!strcmp(line,"application_id"))snprintf(g_app_id,sizeof(g_app_id),"%s",eq);
  else if(!strcmp(line,"large_image"))snprintf(g_large_image,sizeof(g_large_image),"%s",eq);
  else if(!strcmp(line,"large_text"))snprintf(g_large_text,sizeof(g_large_text),"%s",eq);
 }
 fclose(f);
}
#ifdef _WIN32
/* Foxhollow's own map breadcrumb receives the requested map ID before the game
   starts loading it.  On Windows that C symbol is exported from the executable,
   but port_shims is not compiled with Foxhollow's patchable-entry padding, so
   FhModHost::hookInstall cannot safely hook it.  We replace its entry point
   directly instead.  The original function only writes the breadcrumb to
   stderr, which this replacement preserves. */
static void rp_note_map_loaded(int mapId){
 InterlockedExchange(&g_breadcrumbMap,(LONG)mapId);
 fprintf(stderr,"[foxhollow] map-loaded id=%d\n",mapId);
 fflush(stderr);

 /* Drakor's isolated boss map can take over before the normal mod update
    callback runs again. Publish its presence at the breadcrumb itself. The
    post-boss Dragon Rock -> World Map sequence can likewise happen entirely
    while normal mod updates are suspended, so the first later breadcrumb
    also clears Drakor and publishes the destination immediately. */
 if(mapId==0x2C){
  g_space_override=0x2C;
  g_last_map=0x2C;
  if(g_pipe!=INVALID_HANDLE_VALUE) rpc_presence(0x2C);
 } else if(g_space_override==0x2C){
  g_space_override=0;
  g_last_map=mapId;
  if(g_pipe!=INVALID_HANDLE_VALUE) rpc_presence(mapId);
 }
}
static int install_breadcrumb_patch(void){
 unsigned char patch[16]={0xFF,0x25,0,0,0,0,0,0,0,0,0,0,0,0,0x90,0x90};
 DWORD oldProtect=0,ignored=0; uint64_t dst=(uint64_t)(uintptr_t)&rp_note_map_loaded;
 if(!g_host||!g_host->symbolAddress||!g_mod) return 0;
 g_fhNoteMapLoadedTarget=g_host->symbolAddress(g_mod,"fhNoteMapLoaded");
 if(!g_fhNoteMapLoadedTarget){ logmsg(FH_LOG_ERROR,"Discord Rich Presence: fhNoteMapLoaded export was not found."); return 0; }
 memcpy(g_fhNoteMapLoadedOriginal,g_fhNoteMapLoadedTarget,16);
 memcpy(patch+6,&dst,sizeof(dst));
 if(!VirtualProtect(g_fhNoteMapLoadedTarget,16,PAGE_EXECUTE_READWRITE,&oldProtect)){
  logmsg(FH_LOG_ERROR,"Discord Rich Presence: could not make fhNoteMapLoaded writable."); return 0;
 }
 memcpy(g_fhNoteMapLoadedTarget,patch,16);
 FlushInstructionCache(GetCurrentProcess(),g_fhNoteMapLoadedTarget,16);
 VirtualProtect(g_fhNoteMapLoadedTarget,16,oldProtect,&ignored);
 g_fhNoteMapLoadedPatched=1;
 logmsg(FH_LOG_INFO,"Discord Rich Presence: Foxhollow map breadcrumb capture installed.");
 return 1;
}
static void rp_warp_to_map(int warpId,int param){
 /* Retail WARPTAB destination 0x32 is the direct Andross entry used when the
    game enters the fight through warpToMap rather than loadMapAndParent(0x26). */
 if(warpId==0x32) InterlockedExchange(&g_breadcrumbMap,0x26);
 if(g_orig_warpToMap) g_orig_warpToMap(warpId,param);
}
static int install_warp_hook(void){
 if(!g_host||!g_host->symbolAddress||!g_host->hookInstall||!g_mod) return 0;
 g_warpToMapTarget=g_host->symbolAddress(g_mod,"warpToMap");
 if(!g_warpToMapTarget){ logmsg(FH_LOG_ERROR,"Discord Rich Presence: warpToMap export was not found."); return 0; }
 if(!g_host->hookInstall(g_mod,g_warpToMapTarget,(void*)&rp_warp_to_map,(void**)&g_orig_warpToMap)){
  logmsg(FH_LOG_INFO,"Discord Rich Presence: optional direct map-warp capture unavailable."); return 0;
 }
 g_warpToMapHooked=1;
 logmsg(FH_LOG_INFO,"Discord Rich Presence: direct map-warp capture installed.");
 return 1;
}
static void remove_warp_hook(void){
 if(g_warpToMapHooked&&g_host&&g_host->hookRemove&&g_mod&&g_warpToMapTarget)
  g_host->hookRemove(g_mod,g_warpToMapTarget);
 g_warpToMapHooked=0; g_orig_warpToMap=NULL; g_warpToMapTarget=NULL;
}
static void remove_breadcrumb_patch(void){
 DWORD oldProtect=0,ignored=0;
 if(!g_fhNoteMapLoadedPatched||!g_fhNoteMapLoadedTarget) return;
 if(VirtualProtect(g_fhNoteMapLoadedTarget,16,PAGE_EXECUTE_READWRITE,&oldProtect)){
  memcpy(g_fhNoteMapLoadedTarget,g_fhNoteMapLoadedOriginal,16);
  FlushInstructionCache(GetCurrentProcess(),g_fhNoteMapLoadedTarget,16);
  VirtualProtect(g_fhNoteMapLoadedTarget,16,oldProtect,&ignored);
 }
 g_fhNoteMapLoadedPatched=0;
}
typedef struct RpcHeader{ uint32_t op,len; } RpcHeader;
static int rpc_write(uint32_t op,const char* json){ RpcHeader h; DWORD w; size_t n=strlen(json); if(g_pipe==INVALID_HANDLE_VALUE)return 0; h.op=op;h.len=(uint32_t)n;
 if(!WriteFile(g_pipe,&h,sizeof(h),&w,0)||w!=sizeof(h)||!WriteFile(g_pipe,json,(DWORD)n,&w,0)||w!=n){ CloseHandle(g_pipe);g_pipe=INVALID_HANDLE_VALUE;return 0;} return 1; }
static int rpc_connect(void){ int i; char path[64],hello[256]; if(!g_app_id[0]||!strcmp(g_app_id,"YOUR_DISCORD_APPLICATION_ID"))return 0; if(g_pipe!=INVALID_HANDLE_VALUE)return 1;
 for(i=0;i<10;i++){ snprintf(path,sizeof(path),"\\\\?\\pipe\\discord-ipc-%d",i); g_pipe=CreateFileA(path,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,0,0); if(g_pipe!=INVALID_HANDLE_VALUE)break; }
 if(g_pipe==INVALID_HANDLE_VALUE)return 0; snprintf(hello,sizeof(hello),"{\"v\":1,\"client_id\":\"%s\"}",g_app_id); if(!rpc_write(0,hello))return 0; logmsg(FH_LOG_INFO,"Discord Rich Presence connected."); return 1; }
static const char* current_character(void){
 unsigned char* save;
 if(!g_gameBitSaveDataPtr || !(save=*g_gameBitSaveDataPtr)) return "Fox";
 return (save[0x20]&1) ? "Fox" : "Krystal"; /* SaveGame.character: 0=Krystal, 1=Fox */
}
static void activity_details(int map,char* out,size_t out_size){
 switch(map){
  case 0x3A: snprintf(out,out_size,"Flying to Dinosaur Planet"); break;
  case 0x3B: snprintf(out,out_size,"Flying to DarkIce Mines"); break;
  case 0x3C: snprintf(out,out_size,"Flying to CloudRunner Fortress"); break;
  case 0x3D: snprintf(out,out_size,"Flying to Walled City"); break;
  case 0x3E: snprintf(out,out_size,"Flying to Dragon Rock"); break;
  case 0x1C: snprintf(out,out_size,"Fighting Galdon"); break;
  case 0x2C: snprintf(out,out_size,"Fighting Drakor"); break;
  case 0x30: snprintf(out,out_size,"Fighting RedEye King"); break;
  case 0x26: snprintf(out,out_size,"Battling Andross"); break;
  case 0x28: snprintf(out,out_size,"Confronting General Scales"); break;
  case 0x2B: snprintf(out,out_size,"Racing through CloudRunner Fortress"); break;
  case 0x33: snprintf(out,out_size,"Shopping in the ThornTail Store"); break;
  case 0x1F: case 0x20: case 0x21: case 0x22: case 0x27:
   snprintf(out,out_size,"Taking the %s",map_name(map)); break;
  case 0x29: snprintf(out,out_size,"On the World Map"); break;
  case 0x41: snprintf(out,out_size,"On the Great Fox"); break;
  default: snprintf(out,out_size,"Exploring %s",map_name(map)); break;
 }
}
static void rpc_presence(int map){ char json[2048],details[256],state[128]; DWORD pid=GetCurrentProcessId(); long long ts=(long long)g_started;
 if(map==0x3F){ snprintf(details,sizeof(details),"Main Menu"); state[0]=0; }
 else if(map==0x00){ snprintf(details,sizeof(details),"Krystal\'s Prologue"); snprintf(state,sizeof(state),"Playing as Krystal"); }
 else { activity_details(map,details,sizeof(details)); snprintf(state,sizeof(state),"Playing as %s",current_character()); }
 if(state[0]) snprintf(json,sizeof(json),"{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%lu,\"activity\":{\"details\":\"%s\",\"state\":\"%s\",\"timestamps\":{\"start\":%lld},\"assets\":{\"large_image\":\"%s\",\"large_text\":\"%s\"}}},\"nonce\":\"%llu\"}",(unsigned long)pid,details,state,ts,g_large_image,g_large_text,(unsigned long long)g_frame);
 else snprintf(json,sizeof(json),"{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%lu,\"activity\":{\"details\":\"%s\",\"timestamps\":{\"start\":%lld},\"assets\":{\"large_image\":\"%s\",\"large_text\":\"%s\"}}},\"nonce\":\"%llu\"}",(unsigned long)pid,details,ts,g_large_image,g_large_text,(unsigned long long)g_frame);
 rpc_write(1,json);
}
#endif
FH_MOD_EXPORT int fh_mod_initialize(FhMod* mod,const FhModHost* host){ if(!host||host->abiVersion!=FH_MOD_ABI_VERSION)return FH_MOD_ERROR; g_host=host;g_mod=mod;g_started=time(0);
 if(host->symbolAddress){
  g_SaveGame_getCurCharPos=(void*(*)(void))host->symbolAddress(mod,"SaveGame_getCurCharPos");
  g_mapCoordsToId=(int(*)(int,int,int))host->symbolAddress(mod,"mapCoordsToId");
  g_mapBlockWorldSizePtr=(float*)host->symbolAddress(mod,"gMapBlockWorldSize");
  g_mapLoaded=(unsigned char*)host->symbolAddress(mod,"gGameLoopMapLoaded");
  g_gameBitSaveDataPtr=(unsigned char**)host->symbolAddress(mod,"gGameBitSaveData");
 }
 load_config();
#ifdef _WIN32
 install_breadcrumb_patch();
 install_warp_hook();
#endif
 logmsg(FH_LOG_INFO,"SFA Discord Rich Presence 0.1.20 loaded."); return FH_MOD_OK; }
FH_MOD_EXPORT void fh_mod_update(FhMod* mod){ int m;(void)mod;g_frame++;
#ifdef _WIN32
 if(!g_app_id[0])return; if(g_pipe==INVALID_HANDLE_VALUE){ if((g_frame%300)!=1)return; if(!rpc_connect())return; }
 if(g_mapLoaded&&!*g_mapLoaded)return;
 /* current_map() remains valid on the title screen even though no player object exists. */
 m=current_map(); if(m<0)return;
 /* Foxhollow's own breadcrumb is authoritative for map-load transitions that
    do not immediately update the saved character coordinates (notably Andross). */
 {
  int loaded=(int)InterlockedCompareExchange(&g_breadcrumbMap,-1,-1);
  switch(loaded){
   case 0x1C: case 0x26: case 0x2C: case 0x30: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3E:
    g_space_override=loaded; break;
   case 0x3F: case 0x41: case 0x00:
    g_space_override=0; break;
   default:
    /* Any later real map-load ends a previous space override. */
    if(loaded>=0 && loaded!=0x0B) g_space_override=0;
    break;
  }
  if(g_space_override) m=g_space_override;
 }
 if(m!=g_last_map || (g_frame%900)==0){ g_last_map=m; rpc_presence(m); }
#endif
}
FH_MOD_EXPORT void fh_mod_shutdown(FhMod* mod){ (void)mod;
#ifdef _WIN32
 remove_warp_hook();
 remove_breadcrumb_patch();
 if(g_pipe!=INVALID_HANDLE_VALUE){ rpc_write(1,"{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":0,\"activity\":null},\"nonce\":\"shutdown\"}"); CloseHandle(g_pipe);g_pipe=INVALID_HANDLE_VALUE; }
#endif
 g_host=0;g_mod=0; }
