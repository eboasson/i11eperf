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

#if CYCLONEDDS
#include "dds/dds.hpp"
#include "i11eperf.hpp"
#elif OPENSPLICE
#include "i11eperf_s_DCPS.hpp"
#endif
#include "config.h"
#include "gettime.h"

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

static DATATYPE_CPP sample;

template<typename T>
static void pub(dds::domain::DomainParticipant& dp)
{
  std::vector<dds::pub::DataWriter<T>> wrs;
  for (int i = 0; i < NTOPICS; i++) {
    std::string name = "Data";
    if (i > 0) name += std::to_string(i);
    dds::topic::Topic<T> tp(dp, name);
    dds::pub::Publisher pub(dp);
    dds::pub::qos::DataWriterQos qos;
    qos << dds::core::policy::Reliability::Reliable(dds::core::Duration::from_secs(10))
        << dds::core::policy::ResourceLimits((10 * 1048576) / sizeof(T),
                                             dds::core::LENGTH_UNLIMITED, dds::core::LENGTH_UNLIMITED)
        << dds::core::policy::History::HISTORY_KIND;
#if CYCLONEDDS && BATCHING
    qos << dds::core::policy::WriterBatching::BatchUpdates();
#endif
    dds::pub::DataWriter<T> wr(pub, tp, qos);
    wrs.push_back(wr);
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
      T& s = LOANS ? wrs[i]->loan_sample() : sample;
      s.ts() = gettime();
      s.s() = seq;
      wrs[(i + r) % NTOPICS] << s;
    }
    if (++r == NTOPICS)
      r = 0;
    seq++;
#if SLEEP_MS != 0
    for (auto& wr : wrs)
      wr->write_flush(); // so we can forget about BATCHING
    std::this_thread::sleep_until(tnext);
    tnext += tdelta;
#endif
  }
}

int main()
{
  dds::domain::DomainParticipant dp(0);
  pub<DATATYPE_CPP>(dp);
  return 0;
}
