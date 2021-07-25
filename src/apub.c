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
#include "i11eperf_c.h"
#include "config.h"

static void batching ()
{
#if BATCHING
  dds_write_set_batch (true);
#endif
}

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

static void pub (dds_entity_t dp)
{
  dds_entity_t tp = dds_create_topic (dp, &CONCAT (DATATYPE_C, _desc), "Data", NULL, NULL);
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_history (qos, HISTORY_KIND, HISTORY_DEPTH);
  batching ();
  dds_entity_t wr = dds_create_writer (dp, tp, qos, NULL);
  dds_delete_qos (qos);

  signal (SIGTERM, sigh);
  DATATYPE_C sample;
  memset (&sample, 0, sizeof (sample));
  while (!interrupted)
  {
    dds_write (wr, &sample);
    ++sample.s;
#if SLEEP_MS != 0
    dds_write_flush (wr); // just so we don't have to worry about BATCHING
    dds_sleepfor (DDS_MSECS (SLEEP_MS));
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
