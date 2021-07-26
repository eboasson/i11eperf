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

#include "fastdds_i11eperf.hpp"
#include "config.h"
#include "fcommon.hpp"

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

template<typename T>
static void pub(DomainParticipant *dp)
{
  eprosima::fastdds::dds::TypeSupport ts(Traits<T>::ts());
  ts.register_type(dp);
  auto pub = dp->create_publisher(PUBLISHER_QOS_DEFAULT);
  auto tp = dp->create_topic("Data", Traits<T>::name(), TOPIC_QOS_DEFAULT);

  DataWriterQos qos;
  qos.history().kind = HISTORY_KIND;
  qos.history().depth = HISTORY_DEPTH;
  qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
  auto wr = pub->create_datawriter(tp, qos, nullptr);

  signal(SIGTERM, sigh);
  T sample{};
  while (!interrupted)
  {
    wr->write((void *)&sample);
    ++sample.s();
#if SLEEP_MS != 0
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
#endif
  }
}

int main()
{
  pub<DATATYPE_CPP>(make_participant());
  return 0;
}
