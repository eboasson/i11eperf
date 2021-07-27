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
#include <cstdlib>
#include <csignal>
#include <iostream>
#include <chrono>
#include <thread>

#include "opendds_i11eperf.hpp"
#include "config.h"
#include "gettime.h"

using namespace DDS;

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

template<typename T>
static void pub(DomainParticipant_var dp)
{
  TYPESUPPORT(DATATYPE);
  ts->register_type(dp, "");
  DDS::Publisher *pub = dp->create_publisher(PUBLISHER_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
  DDS::Topic *tp = dp->create_topic("Data", ts->get_type_name(), TOPIC_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

  DataWriterQos qos;
  pub->get_default_datawriter_qos(qos);
  qos.history.kind = HISTORY_KIND;
  qos.history.depth = HISTORY_DEPTH;
  qos.reliability.kind = RELIABLE_RELIABILITY_QOS;
  qos.resource_limits.max_samples = (10 * 1048576) / sizeof(i11eperf::DATATYPE);
  DDS::DataWriter *wrw = pub->create_datawriter(tp, qos, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
  CONCAT(i11eperf::DATATYPE, DataWriter) *wr = NARROW_W(wrw);

  signal(SIGTERM, sigh);
  T sample;
  while (!interrupted)
  {
    sample.ts = gettime();
    wr->write(sample, HANDLE_NIL);
    ++sample.s;
#if SLEEP_MS != 0
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
#endif
  }

  pub->delete_datawriter(wrw);
  dp->delete_publisher(pub);
  dp->delete_topic(tp);
}

int main(int argc, char *argv[])
{
  DDS::DomainParticipantFactory *dpf = TheParticipantFactoryWithArgs(argc, argv);
  DDS::DomainParticipant *dp = dpf->create_participant(0, PARTICIPANT_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
  pub<DATATYPE_CPP>(dp);
  dpf->delete_participant(dp);
  return 0;
}
