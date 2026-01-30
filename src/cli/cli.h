#pragma once
#include "dhcp/handlers.h"

/*
 * Non-blocking CLI over UNIX domain socket
 * - cli_init(): create + bind + listen (non-blocking)
 * - cli_get_fd(): return listening fd
 * - cli_handle(): accept() + handle client command (non-blocking)
 */

int cli_init(server_ctx_t* ctx, const char* path);
int cli_get_fd(void);
void cli_handle(void);
