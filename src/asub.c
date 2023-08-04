/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "dds/dds.h"
#include "i11eperf_a.h"
#include "config.h"
#include "gettime.h"

static double bytes_fixed (const DATATYPE_C *s) {
  return sizeof (*s);
}
static double bytes_sequence (const i11eperf_seq *s) {
  return 4 + s->xseq._length;
}
#define bytes(X) _Generic ((X),                 \
    i11eperf_seq: bytes_sequence,               \
    default: bytes_fixed) (X)

struct tslat {
  double t, l;
};

struct Stats {
  int64_t tnext, intv;
  unsigned count, lost, errs, nseq;
  uint64_t bytes;
  unsigned nlats, nraw, maxraw;
  struct tslat *raw;
  double lats[1000];
  const char *statsname;
};

static void Stats_init (struct Stats *s, unsigned maxraw, const char *statsname, int64_t intv)
{
  s->tnext = gettime () + intv;
  s->intv = intv;
  s->count = s->lost = s->errs = s->nlats = s->nraw = 0;
  s->bytes = 0;
  s->nseq = UINT32_MAX;
  s->maxraw = maxraw;
  s->raw = malloc (maxraw * sizeof (*s->raw));
  s->statsname = strdup (statsname);
}

static void Stats_update (struct Stats *s, uint32_t seq, size_t bytes, double ts, double lat)
{
  if (seq != s->nseq) {
    if (s->nseq != UINT32_MAX) {
      if ((int32_t) (seq - s->nseq) < 0)
        s->errs += 1;
      else
        s->lost += seq - s->nseq;
    }
  }
  s->nseq = seq + 1;
  s->count += 1;
  s->bytes += bytes;
  if (s->nraw < s->maxraw)
  {
    s->raw[s->nraw].t = ts;
    s->raw[s->nraw].l = lat;
    s->nraw++;
  }
  if (s->nlats < sizeof (s->lats) / sizeof (s->lats[0]))
  {
    s->lats[s->nlats++] = lat;
  }
}

static int double_cmp (const void *va, const void *vb)
{
  const double *a = va;
  const double *b = vb;
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

static void Stats_report (struct Stats *s)
{
  int64_t tnow = gettime ();
  if (tnow > s->tnext)
  {
    qsort(s->lats, s->nlats, sizeof (s->lats[0]), double_cmp);
    double dt = (tnow - s->tnext + s->intv) / 1e9;
    double srate = s->count / dt;
    double brate = s->bytes / dt;
    double meanlat = 0.0;
    if (s->nlats > 0)
    {
      for (unsigned i = 0; i < s->nlats; i++)
        meanlat += s->lats[i];
      meanlat /= s->nlats;
    }
    printf ("%.5f kS/s %.5f Gb/s lost %u errs %u usmeanlat %f us90%%lat %f\n", (srate * 1e-3), (brate * 8e-9), s->lost, s->errs, 1e6 * meanlat, 1e6 * ((s->nlats == 0) ? 0 : s->lats[s->nlats - (s->nlats + 9) / 10]));
    fflush (stdout);
    s->tnext = tnow + s->intv;
    s->count = s->bytes = s->lost = s->nlats = 0;
  }
}

static void Stats_fini (struct Stats *s)
{
  if (strcmp (s->statsname, ""))
  {
    FILE *fp = fopen (s->statsname, "wb");
    fwrite (s->raw, sizeof (s->raw[0]), s->nraw, fp);
    fclose (fp);
  }
}

static struct Stats stats;
static DATATYPE_C xs[5];
static dds_sample_info_t si[sizeof (xs) / sizeof (xs[0])];
static void *ptrs[sizeof (xs) / sizeof (xs[0])];
static int64_t tref;

static void on_data_available(dds_entity_t rd, void *varg)
{
  (void) varg;
  const int N = (int) (sizeof (xs) / sizeof (xs[0]));
  int n;
  do {
    n = dds_take(rd, ptrs, si, N, N);
    int64_t tnow = gettime ();
    for (int i = 0; i < n; i++)
      if (si[i].valid_data)
        Stats_update(&stats, xs[i].s, bytes(&xs[i]), (tnow - tref) / 1e9, (tnow - xs[i].ts) / 1e9);
  } while (n == N);
  Stats_report (&stats);
}

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

static void sub (dds_entity_t dp, const struct options *opts)
{
  tref = gettime ();
  Stats_init (&stats, 10000000, opts->latfile, opts->report_intv);

  for (size_t i = 0; i < sizeof (xs) / sizeof (xs[0]); i++)
    ptrs[i] = &xs[i];

  dds_entity_t rds[opts->ntopics];
  for (int j = 0; j < opts->ntopics; j++)
  {
    char name[20] = "Data";
    if (j > 0) snprintf (name + 4, sizeof (name) - 4, "%d", j);
    dds_entity_t tp = dds_create_topic (dp, &CONCAT (DATATYPE_C, _desc), name, NULL, NULL);
    dds_qos_t *qos = dds_create_qos ();
    dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_qset_resource_limits (qos, (10 * 1048576) / sizeof (DATATYPE_C), DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);
    if (opts->history == 0)
      dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
    else
      dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, opts->history);
    rds[j] = dds_create_reader (dp, tp, qos, NULL);
    dds_delete_qos (qos);
    dds_listener_t *list = dds_create_listener (NULL);
    dds_lset_data_available (list, on_data_available);
    dds_set_listener (rds[j], list);
    dds_delete_listener (list);
    dds_delete (tp);
  }

  signal (SIGINT, sigh);
  signal (SIGTERM, sigh);
  while (!interrupted)
    dds_sleepfor (DDS_MSECS (100));

  for (int j = 0; j < opts->ntopics; j++)
    dds_delete (rds[j]);
  Stats_fini (&stats);
}

int main(int argc, char **argv)
{
  struct options opts = get_options (argc, argv);
  dds_entity_t dp = dds_create_participant (0, NULL, NULL);
  sub(dp, &opts);
  dds_delete (dp);
  return 0;
}
