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

#if CYCLONEDDS && BATCHING
#include "dds/dds.h"
static void batching() { dds_write_set_batch (true); }
template<typename T> static void flush(dds::pub::DataWriter<T>& wr) { dds_write_flush(wr->get_ddsc_entity()); }
#else
static void batching() {}
template<typename T> static void flush(dds::pub::DataWriter<T>&) {}
#endif

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

template<typename T>
static void pub(dds::domain::DomainParticipant& dp)
{
  dds::topic::Topic<T> tp(dp, "Data");
  dds::pub::Publisher pub(dp);
  dds::pub::qos::DataWriterQos qos;
  qos << dds::core::policy::Reliability::Reliable(dds::core::Duration::from_secs(10))
      << dds::core::policy::ResourceLimits((10 * 1048576) / sizeof(T),
                                           dds::core::LENGTH_UNLIMITED, dds::core::LENGTH_UNLIMITED)
      << dds::core::policy::History::HISTORY_KIND;
  batching();
  dds::pub::DataWriter<T> wr(pub, tp, qos);

  uint32_t seq = 0;
  signal(SIGTERM, sigh);
  while (!interrupted)
  {
    auto& sample = wr->loan_sample();
    sample.s() = seq++;
    sample.ts() = gettime();
    wr << sample;
#if SLEEP_MS != 0
    flush(wr); // so we can forget about BATCHING
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
#endif
  }
}

int main()
{
  dds::domain::DomainParticipant dp(0);
  pub<DATATYPE_CPP>(dp);
  return 0;
}
