#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

#if defined (__cplusplus)
extern "C" {
#endif

#define CONCAT1(a,b) a##b
#define CONCAT(a,b) CONCAT1(a,b)

#define DATATYPE_CPP i11eperf::DATATYPE
#define DATATYPE_C CONCAT (i11eperf_, DATATYPE)

struct options {
  int ntopics;
  int history;
  bool batching;
  bool loans;
  bool fastdds_shm;
  bool fastdds_onlyloopback;
  int64_t sleep;
  int64_t report_intv;
  const char *latfile;
};

struct options get_options (int argc, char **argv);

#if defined (__cplusplus)
}
#endif

#endif
