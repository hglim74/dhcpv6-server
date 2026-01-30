#include "config/config.h"
#include "util/log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

static void trim(char* s){
  char* e;
  while(*s==' '||*s=='\t') s++;
  e = s + strlen(s) - 1;
  while(e>s && (*e=='\n'||*e=='\r'||*e==' '||*e=='\t')) *e--=0;
}

int config_load(const char* path, server_ctx_t* ctx){
  FILE* f = fopen(path, "r");
  if(!f){
    log_printf(LOG_ERR, "config open failed: %s", path);
    return -1;
  }

  char line[256];
  while(fgets(line, sizeof(line), f)){
    if(line[0]=='#' || line[0]=='\n') continue;

    char* eq = strchr(line, '=');
    if(!eq) continue;
    *eq = 0;

    char* key = line;
    char* val = eq+1;
    trim(key); trim(val);

    if(strcmp(key,"log_level")==0){
      if(strcmp(val,"DEBUG")==0) log_set_level(LOG_DEBUG);
      else if(strcmp(val,"INFO")==0) log_set_level(LOG_INFO);
      else if(strcmp(val,"WARN")==0) log_set_level(LOG_WARN);
      else if(strcmp(val,"ERROR")==0) log_set_level(LOG_ERR);
    }
    else if(strcmp(key,"offer_ttl")==0){
      ctx->offer_ttl = atoi(val);
    }
    else if(strcmp(key,"decline_ttl")==0){
      ctx->decline_ttl = atoi(val);
    }
    else if(strcmp(key,"preferred_lifetime")==0){
      ctx->preferred_lft = atoi(val);
    }
    else if(strcmp(key,"valid_lifetime")==0){
      ctx->valid_lft = atoi(val);
    }
    else if(strcmp(key,"na_prefix")==0){
      char* slash = strchr(val,'/');
      if(!slash) continue;
      *slash = 0;
      inet_pton(AF_INET6, val, &ctx->na_pool.prefix64);
    }
    else if(strcmp(key,"na_host_start")==0){
      ctx->na_pool.host_start = strtoull(val,NULL,0);
    }
    else if(strcmp(key,"na_host_end")==0){
      ctx->na_pool.host_end = strtoull(val,NULL,0);
    }
    else if(strcmp(key,"pd_base_prefix")==0){
      char* slash = strchr(val,'/');
      if(!slash) continue;
      *slash = 0;
      inet_pton(AF_INET6, val, &ctx->pd_pool.base_prefix);
      ctx->pd_pool.base_len = atoi(slash+1);
    }
    else if(strcmp(key,"pd_delegated_len")==0){
      ctx->pd_pool.delegated_len = atoi(val);
    }
    else if(strcmp(key,"dns")==0){
      if(ctx->dns_cnt < 4){
        inet_pton(AF_INET6, val, &ctx->dns[ctx->dns_cnt++]);
      }
    }
  }

  fclose(f);
  return 0;
}

void config_dump(const server_ctx_t* ctx){
  char buf[INET6_ADDRSTRLEN];
  log_printf(LOG_INFO, "=== DHCPv6 config ===");

  inet_ntop(AF_INET6, &ctx->na_pool.prefix64, buf, sizeof(buf));
  log_printf(LOG_INFO, "NA prefix: %s", buf);

  inet_ntop(AF_INET6, &ctx->pd_pool.base_prefix, buf, sizeof(buf));
  log_printf(LOG_INFO, "PD base: %s/%u → /%u",
             buf, ctx->pd_pool.base_len, ctx->pd_pool.delegated_len);

  log_printf(LOG_INFO, "preferred=%u valid=%u",
             ctx->preferred_lft, ctx->valid_lft);

  for(size_t i=0;i<ctx->dns_cnt;i++){
    inet_ntop(AF_INET6, &ctx->dns[i], buf, sizeof(buf));
    log_printf(LOG_INFO, "DNS[%zu]=%s", i, buf);
  }
}
