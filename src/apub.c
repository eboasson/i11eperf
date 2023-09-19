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
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include "dds/dds.h"
#include "i11eperf_a.h"
#include "config.h"
#include "gettime.h"

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

static DATATYPE_C sample;

static void pub (dds_entity_t dp, const struct options *opts)
{
  dds_entity_t wrs[opts->ntopics];
  for (int i = 0; i < opts->ntopics; i++)
  {
    char name[20] = "Data";
    if (i > 0) snprintf (name + 4, sizeof (name) - 4, "%d", i);
    dds_entity_t tp = dds_create_topic (dp, &CONCAT (DATATYPE_C, _desc), name, NULL, NULL);
    dds_qos_t *qos = dds_create_qos ();
    dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_qset_resource_limits (qos, (10 * 1048576) / sizeof (DATATYPE_C), DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);
    if (opts->history == 0)
      dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
    else
      dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, opts->history);
    dds_qset_writer_batching (qos, opts->batching);
    wrs[i] = dds_create_writer (dp, tp, qos, NULL);
    dds_delete_qos (qos);
    dds_delete (tp);
  }
  dds_entity_t ws = dds_create_waitset (dp);
  dds_entity_t gc = dds_create_guardcondition (dp);
  dds_waitset_attach (ws, gc, 0); // empty waitset won't block (for better or for worse)

  signal (SIGINT, sigh);
  signal (SIGTERM, sigh);
  dds_time_t tnext = dds_time () + opts->sleep;
  int r = 0;
  uint32_t s = 0;
  DATATYPE_C *sampleptr = &sample;
  while (!interrupted)
  {
    for (int i = 0; i < opts->ntopics; i++)
    {
      const dds_entity_t wr = wrs[(i + r) % opts->ntopics];
#ifndef DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP // proxy for presence of meaningful loan support
      if (opts->loans)
      {
        void *ptr;
        dds_return_t ret;
        ret = dds_request_loan (wr, &ptr);
        if (ret != 0) { fprintf (stderr, "failed to get loan\n"); exit (2); }
        sampleptr = ptr;
      }
#endif
      sampleptr->ts = gettime ();
      sampleptr->s = s;
      dds_write (wr, sampleptr);
    }
    ++s;
    if (++r == opts->ntopics)
      r = 0;

    if (opts->sleep)
    {
      if (opts->batching)
        for (int i = 0; i < opts->ntopics; i++)
          dds_write_flush (wrs[i]);
      dds_waitset_wait_until (ws, NULL, 0, tnext);
      tnext += opts->sleep;
    }
  }
}

int main(int argc, char **argv)
{
  const struct options opts = get_options (argc, argv);
  dds_entity_t dp = dds_create_participant (0, NULL, NULL);
  pub(dp, &opts);
  dds_delete (dp);
  return 0;
}
