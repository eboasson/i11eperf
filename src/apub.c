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
#include "dds/dds.h"
#include "i11eperf_a.h"
#include "config.h"
#include "gettime.h"

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

static DATATYPE_C sample;

static void pub (dds_entity_t dp)
{
  dds_entity_t wrs[NTOPICS];
  for (int i = 0; i < NTOPICS; i++)
  {
    char name[20] = "Data";
    if (i > 0) snprintf (name + 4, sizeof (name) - 4, "%d", i);
    dds_entity_t tp = dds_create_topic (dp, &CONCAT (DATATYPE_C, _desc), name, NULL, NULL);
    dds_qos_t *qos = dds_create_qos ();
    dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_qset_resource_limits (qos, (10 * 1048576) / sizeof (DATATYPE_C), DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);
    dds_qset_history (qos, HISTORY_KIND, HISTORY_DEPTH);
#if BATCHING
    dds_qset_writer_batching (qos, true);
#endif
    wrs[i] = dds_create_writer (dp, tp, qos, NULL);
    dds_delete_qos (qos);
    dds_delete (tp);
  }
  dds_entity_t ws = dds_create_waitset (dp);
  dds_entity_t gc = dds_create_guardcondition (dp);
  dds_waitset_attach (ws, gc, 0); // empty waitset won't block (for better or for worse)

  signal (SIGTERM, sigh);
  const dds_duration_t tdelta = DDS_MSECS (SLEEP_MS);
  dds_time_t tnext = dds_time () + tdelta;
  int r = 0;
  while (!interrupted)
  {
    for (int i = 0; i < NTOPICS; i++)
    {
      sample.ts = gettime ();
      dds_write (wrs[(i + r) % NTOPICS], &sample);
    }
    ++sample.s;
    if (++r == NTOPICS)
      r = 0;

#if SLEEP_MS != 0
    for (int i = 0; i < NTOPICS; i++)
      dds_write_flush (wrs[i]); // just so we don't have to worry about BATCHING
    // wait until time instead of sleep because dds_write takes time, too
    dds_waitset_wait_until (ws, NULL, 0, tnext);
    tnext += tdelta;
#endif
  }
}

int main()
{
  dds_entity_t dp = dds_create_participant (0, NULL, NULL);
  pub(dp);
  dds_delete (dp);
  return 0;
}
