#pragma once
#include "dhcp/handlers.h"

int config_load(const char* path, server_ctx_t* ctx);
void config_dump(const server_ctx_t* ctx);
