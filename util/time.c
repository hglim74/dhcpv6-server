#include "util/time.h"
#include <time.h>

uint64_t now_epoch_sec(void){
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec;
}
