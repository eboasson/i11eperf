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
#include <chrono>
#include <thread>

#if CYCLONEDDS
#include "dds/dds.hpp"
#include "i11eperf.hpp"
#elif OPENSPLICE
#include "i11eperf_s_DCPS.hpp"
#endif
#include "bytesize.hpp"
#include "stats.hpp"
#include "config.h"
#include "gettime.h"

template<typename T>
class L : public dds::sub::NoOpDataReaderListener<T> {
public:
  L(dds::domain::DomainParticipant& dp, std::string statsname) : dp_(dp), stats_(10000000, statsname) {
    tref_ = gettime();
  }
  
  virtual void on_data_available(dds::sub::DataReader<T>& rd)
  {
    unsigned n;
    do {
      n = rd.take(xs_, N);
      const int64_t tnow = gettime();
      for (unsigned i = 0; i < n; i++) {
        if (xs_[i].info().valid()) {
          stats_.update(xs_[i].data().s(), bytes(xs_[i].data()), (tnow-tref_)/1e9, (tnow-xs_[i].data().ts())/1e9);
        }
      }
    } while (n == N);
    stats_.report();
  }

private:
  static const unsigned N = 5;
  int64_t tref_;
  dds::domain::DomainParticipant& dp_;
  dds::sub::Sample<T> xs_[N];
  Stats stats_;
};

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

template<typename T>
static void sub(dds::domain::DomainParticipant& dp, std::string statsname)
{
  L<T> l(dp, statsname);

  dds::topic::Topic<T> tp(dp, "Data");
  dds::sub::Subscriber sub(dp);
  dds::sub::qos::DataReaderQos qos;
  qos << dds::core::policy::Reliability::Reliable(dds::core::Duration::from_secs(10))
      << dds::core::policy::History::HISTORY_KIND;
  dds::sub::DataReader<T> rd(sub, tp, qos);

  rd.listener(&l, dds::core::status::StatusMask::data_available());
  signal(SIGTERM, sigh);
  while (!interrupted)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main(int argc, char **argv)
{
  dds::domain::DomainParticipant dp(0);
  sub<DATATYPE_CPP>(dp, argc < 2 ? "" : std::string(argv[1]));
  return 0;
}
