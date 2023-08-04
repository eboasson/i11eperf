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
#include "gettime.h"
#include "fcommon.hpp"

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

static DATATYPE_CPP sample;

template<typename T>
static void pub(DomainParticipant *dp, const options& opts)
{
  eprosima::fastdds::dds::TypeSupport ts(Traits<T>::ts());
  ts.register_type(dp);
  auto pub = dp->create_publisher(PUBLISHER_QOS_DEFAULT);
  std::vector<DataWriter *> wrs;
  for (int i = 0; i < opts.ntopics; i++)
  {
    std::string name = "Data";
    if (i > 0) name += std::to_string(i);
    auto tp = dp->create_topic(name, Traits<T>::name(), TOPIC_QOS_DEFAULT);
    DataWriterQos qos;
    if (opts.history == 0) {
      qos.history().kind = KEEP_ALL_HISTORY_QOS;
      qos.history().depth = 1;
    } else {
      qos.history().kind = KEEP_LAST_HISTORY_QOS;
      qos.history().depth = opts.history;
    }
    qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    qos.reliability().max_blocking_time.seconds = 10;
    qos.reliability().max_blocking_time.nanosec = 0;
    qos.resource_limits().max_samples = (10 * 1048576) / sizeof(T);
    auto wr = pub->create_datawriter(tp, qos, nullptr);
    wrs.push_back(wr);
  }

  signal(SIGINT, sigh);
  signal(SIGTERM, sigh);
  auto tdelta = std::chrono::nanoseconds(opts.sleep);
  std::chrono::time_point<std::chrono::steady_clock> tnext = std::chrono::steady_clock::now() + tdelta;
  uint32_t seq = 0;
  int r = 0;
  while (!interrupted)
  {
    for (int i = 0; i < opts.ntopics; i++)
    {
      sample.ts() = gettime();
      sample.s() = seq;
      wrs[(i + r) % opts.ntopics]->write(static_cast<void *>(&sample));
    }
    if (++r == opts.ntopics)
      r = 0;
    seq++;
    if (opts.sleep)
    {
      std::this_thread::sleep_until(tnext);
      tnext += tdelta;
    }
  }
}

int main(int argc, char **argv)
{
  const options opts = get_options (argc, argv);
  pub<DATATYPE_CPP>(make_participant(opts), opts);
  return 0;
}
