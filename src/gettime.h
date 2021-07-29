#ifndef TIME_H
#define TIME_H

#include <time.h>

static int64_t gettime (void)
{
  struct timespec ts;
  clock_gettime (CLOCK_REALTIME, &ts);
  return (int64_t) ts.tv_sec * 1000000000 + ts.tv_nsec;
}

#endif
