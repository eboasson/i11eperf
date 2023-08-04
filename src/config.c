#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "config.h"

static const struct options default_options = {
  .batching = false,
  .history = 0,
  .sleep = 0,
  .report_intv = 1000000000,
  .ntopics = 1,
  .fastdds_shm = true,
  .fastdds_onlyloopback = false,
  .loans = false,
  .latfile = ""
};

static void usage (const char *argv0)
{
  printf ("usage: %s [OPTIONS]\n\
\n\
-h        this text\n\
-b        enable batching (Cyclone)\n\
-i MS     publish every MS milliseconds [%d]\n\
-k DEPTH  history depth (0 = keep-last) [%d]\n\
-l        use loans (Cyclone C++ only, requires Iceoryx)\n\
-n NTOPIC do NTOPIC topics in parallel [%d]\n\
-o FILE   write (some) latencies to FILE at exit\n\
-r INTV   report every INTV seconds [%d]\n\
-y        Fast-DDS restricted to loopback\n\
-z        Fast-DDS without shared memory\n\
",
          argv0,
          (int) (default_options.sleep / 1000000), default_options.history,
          default_options.ntopics,
          (int) (default_options.report_intv / 1000000000));
}

struct options get_options (int argc, char **argv)
{
  int opt;
  struct options o = default_options;
  while ((opt = getopt (argc, argv, "bhi:k:ln:o:r:yz")) != EOF)
  {
    switch (opt)
    {
      case 'b':
        o.batching = true;
        break;
      case 'i':
        o.sleep = 1000000 * (int64_t) atoi (optarg);
        if (o.sleep < 0 || o.sleep > 60000000000) {
          fprintf (stderr, "sleep duration out of range\n");
          exit (2);
        }
        break;
      case 'k':
        o.history = atoi (optarg);
        if (o.history < 0) {
          fprintf (stderr, "history depth out of range\n");
          exit (2);
        }
        break;
      case 'l':
        o.loans = true;
        break;
      case 'n':
        o.ntopics = atoi (optarg);
        // we put readers/writers in a VLA on the stack, so better be careful
        if (o.ntopics < 1 || o.ntopics > 20) {
          fprintf (stderr, "ntopics out of range\n");
          exit (2);
        }
        break;
      case 'o':
        o.latfile = optarg;
        break;
      case 'r':
        o.report_intv = 1000000000 * (int64_t) atoi (optarg);
        if (o.report_intv < 1000000000 || o.report_intv > 60000000000) {
          fprintf (stderr, "report interval out of range\n");
          exit (2);
        }
        break;
      case 'y':
        o.fastdds_onlyloopback = true;
        break;
      case 'z':
        o.fastdds_shm = false;
        break;
      case 'h':
        usage (argv[0]);
        exit (0);
        break;
      case '?':
        fprintf (stderr, "%c: unrecognized option\n", optopt);
        exit (2);
        break;
      case ':':
        fprintf (stderr, "%c: argument missing\n", optopt);
        exit (2);
        break;
    }
  }
  if (optind < argc)
  {
    fprintf (stderr, "superfluous arguments present\n");
    exit (2);
  }
  return o;
}
