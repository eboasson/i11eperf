#ifndef CONFIG_H_
#define CONFIG_H_

// set HISTORY to KEEP_ALL to get a keep-all history, otherwise it becomes a keep-last
// history with depth HISTORY
#define KEEP_ALL 10000000

#ifndef HISTORY
#define HISTORY KEEP_ALL
#endif
#ifndef DATATYPE
#define DATATYPE ou
#endif
#ifndef BATCHING
#define BATCHING 1
#endif

#define CONCAT1(a,b) a##b
#define CONCAT(a,b) CONCAT1(a,b)

#define DATATYPE_CPP i11eperf::DATATYPE
#define DATATYPE_C CONCAT (i11eperf_, DATATYPE)

#if HISTORY <= 0 || HISTORY > KEEP_ALL
#error
#endif
#ifndef __cplusplus
#  if HISTORY == KEEP_ALL
#    define HISTORY_KIND DDS_HISTORY_KEEP_ALL
#    define HISTORY_DEPTH 1
#  else
#    define HISTORY_KIND DDS_HISTORY_KEEP_LAST
#    define HISTORY_DEPTH HISTORY
#  endif
#elif defined FASTDDS
#  if HISTORY == KEEP_ALL
#    define HISTORY_KIND KEEP_ALL_HISTORY_QOS
#    define HISTORY_DEPTH 1
#  else
#    define HISTORY_KIND KEEP_LAST_HISTORY_QOS
#    define HISTORY_DEPTH HISTORY
#  endif
#else
#  if HISTORY == KEEP_ALL
#    define HISTORY_KIND KeepAll()
#  else
#    define HISTORY_KIND KeepLast(HISTORY)
#  endif
#endif

#endif
