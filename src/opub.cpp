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

static DATATYPE_CPP sample;

template<typename T>
static void pub(DomainParticipant_var dp)
{
  TYPESUPPORT(DATATYPE);
  ts->register_type(dp, "");
  DDS::Publisher *pub = dp->create_publisher(PUBLISHER_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
  std::vector<CONCAT(i11eperf::DATATYPE, DataWriter) *> wrs;
  for (int i = 0; i < NTOPICS; i++) {
    std::string name = "Data";
    if (i > 0) name += std::to_string(i);
    DDS::Topic *tp = dp->create_topic(name.c_str(), ts->get_type_name(), TOPIC_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DataWriterQos qos;
    pub->get_default_datawriter_qos(qos);
    // one would think that setting data_representation on the type would suffice, but no:
    // - without data_representation and qos.representation, we get XCDR2
    // - with only data_representation, the call to create_datawriter fails
    // - with only qos.representation, we get what we want
    // - with data_representation and qos.representation, we get what we want
    qos.representation.value.length(1);
    qos.representation.value[0] = DDS::XCDR_DATA_REPRESENTATION;
    qos.history.kind = HISTORY_KIND;
    qos.history.depth = HISTORY_DEPTH;
    qos.reliability.kind = RELIABLE_RELIABILITY_QOS;
    qos.reliability.max_blocking_time.sec = 10;
    qos.reliability.max_blocking_time.nanosec = 0;
    qos.resource_limits.max_samples = (10 * 1048576) / sizeof(T);
    DDS::DataWriter *wrw = pub->create_datawriter(tp, qos, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    wrs.push_back(NARROW_W(wrw));
  }

  signal(SIGTERM, sigh);
  auto tdelta = std::chrono::milliseconds(SLEEP_MS);
  std::chrono::time_point<std::chrono::steady_clock> tnext = std::chrono::steady_clock::now() + tdelta;
  uint32_t seq = 0;
  int r = 0;
  while (!interrupted)
  {
    for (int i = 0; i < NTOPICS; i++)
    {
      sample.ts = gettime();
      sample.s = seq;
      wrs[(i + r) % NTOPICS]->write(sample, HANDLE_NIL);
    }
    if (++r == NTOPICS)
      r = 0;
    seq++;
#if SLEEP_MS != 0
    std::this_thread::sleep_until(tnext);
    tnext += tdelta;
#endif
  }

  pub->delete_contained_entities();
  dp->delete_contained_entities();
}

int main(int argc, char *argv[])
{
  TheServiceParticipant->default_configuration_file(ACE_TEXT("opendds.ini"));
  DDS::DomainParticipantFactory *dpf = TheParticipantFactoryWithArgs(argc, argv);
  DDS::DomainParticipant *dp = dpf->create_participant(0, PARTICIPANT_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
  pub<DATATYPE_CPP>(dp);
  dpf->delete_participant(dp);
  return 0;
}
