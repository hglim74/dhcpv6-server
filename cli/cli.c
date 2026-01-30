#include "cli/cli.h"
#include "config/config.h"
#include "util/log.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

static int cli_fd = -1;
static server_ctx_t* g_ctx = NULL;

static void set_nonblock(int fd){
  int flags = fcntl(fd, F_GETFL, 0);
  if(flags >= 0){
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }
}

int cli_init(server_ctx_t* ctx, const char* path){
  g_ctx = ctx;

  cli_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(cli_fd < 0){
    log_printf(LOG_ERR, "cli socket create failed");
    return -1;
  }

  set_nonblock(cli_fd);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);
  unlink(path);

  if(bind(cli_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
    log_printf(LOG_ERR, "cli bind failed");
    close(cli_fd);
    cli_fd = -1;
    return -1;
  }

  if(listen(cli_fd, 8) < 0){
    log_printf(LOG_ERR, "cli listen failed");
    close(cli_fd);
    cli_fd = -1;
    return -1;
  }

  log_printf(LOG_INFO, "CLI listening on %s", path);
  return 0;
}

int cli_get_fd(void){
  return cli_fd;
}

static void handle_command(int cfd){
  char buf[256];
  ssize_t n = read(cfd, buf, sizeof(buf)-1);
  if(n <= 0) return;
  buf[n] = 0;

  if(strncmp(buf, "show config", 11) == 0){
    dprintf(cfd, "OK\n");
    config_dump(g_ctx);
  }
  else if(strncmp(buf, "set log", 7) == 0){
    if(strstr(buf, "DEBUG")) log_set_level(LOG_DEBUG);
    else if(strstr(buf, "INFO")) log_set_level(LOG_INFO);
    else if(strstr(buf, "WARN")) log_set_level(LOG_WARN);
    else if(strstr(buf, "ERROR")) log_set_level(LOG_ERR);
    dprintf(cfd, "OK\n");
  }
  else{
    dprintf(cfd, "UNKNOWN COMMAND\n");
  }
}

void cli_handle(void){
  if(cli_fd < 0) return;

  while(1){
    int cfd = accept(cli_fd, NULL, NULL);
    if(cfd < 0){
      if(errno == EAGAIN || errno == EWOULDBLOCK)
        return; // no more clients
      log_printf(LOG_WARN, "cli accept error");
      return;
    }

    set_nonblock(cfd);
    handle_command(cfd);
    close(cfd);
  }
}
