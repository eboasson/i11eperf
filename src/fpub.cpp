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
  DomainParticipantQos pqos;
  pqos.transport().use_builtin_transports = false;
  auto udp_transport = std::make_shared<UDPv4TransportDescriptor>();
  udp_transport->interfaceWhiteList.push_back("127.0.0.1");
  pqos.transport().user_transports.push_back(udp_transport);
#if SHM
  auto shm_transport = std::make_shared<SharedMemTransportDescriptor>();
  shm_transport->segment_size(2 * 1024 * 1024);
  pqos.transport().user_transports.push_back(shm_transport);
#endif
  auto dp = DomainParticipantFactory::get_instance()->create_participant(0, pqos);
  pub<DATATYPE_CPP>(dp);
  return 0;
}
