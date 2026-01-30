#pragma once
#include "store/lease_store.h"

int mem_store_init(lease_store_t* st, size_t cap);
void mem_store_free(lease_store_t* st);
