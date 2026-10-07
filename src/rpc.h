#ifndef SFA_RP_RPC_H
#define SFA_RP_RPC_H
#include <stddef.h>
int rp_rpc_connect(const char *application_id);
int rp_rpc_connected(void);
int rp_rpc_write(unsigned int op, const char *json);
void rp_rpc_close(void);
unsigned long rp_process_id(void);
#endif
