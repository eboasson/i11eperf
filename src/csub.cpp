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
#include <array>
#include <span>

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
  L(dds::domain::DomainParticipant& dp, std::string statsname, std::chrono::nanoseconds report_intv, bool loans)
  : dp_(dp), stats_(10000000, statsname, report_intv), loans_(loans) {
    tref_ = gettime();
  }

  virtual void on_data_available(dds::sub::DataReader<T>& rd)
  {
    if (loans_)
      do_loan(rd);
    else
      do_non_loan(rd);
    stats_.report();
  }

private:
  template<typename H>
  void do_samples(const H& xs)
  {
    const int64_t tnow = gettime();
    for (const auto& x : xs)
      if (x.info().valid())
        stats_.update(x.data().s(), bytes(x.data()), (tnow-tref_)/1e9, (tnow-x.data().ts())/1e9);
  }

  void do_non_loan(dds::sub::DataReader<T>& rd)
  {
    unsigned n;
    do {
      n = rd.take(xs_.data(), static_cast<uint32_t>(xs_.size()));
      do_samples(std::span(xs_).subspan(0, n));
    } while (n == xs_.size());
  }

  void do_loan(dds::sub::DataReader<T>& rd)
  {
    dds::sub::LoanedSamples<T> xs = rd.take();
    do_samples(xs);
  }

  std::array<dds::sub::Sample<T>, 5> xs_;
  int64_t tref_;
  dds::domain::DomainParticipant& dp_;
  Stats stats_;
  bool loans_;
};

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

template<typename T>
static void sub(dds::domain::DomainParticipant& dp, const options& opts)
{
  L<T>* l = new L<T>(dp, std::string(opts.latfile), std::chrono::nanoseconds(opts.report_intv), opts.loans);
  std::vector<dds::sub::DataReader<T>> rds;
  for (int i = 0; i < opts.ntopics; i++)
  {
    std::string name = "Data";
    if (i > 0) name += std::to_string(i);
    dds::topic::Topic<T> tp(dp, name);
    dds::sub::Subscriber sub(dp);
    dds::sub::qos::DataReaderQos qos;
    qos << dds::core::policy::Reliability::Reliable(dds::core::Duration::from_secs(10))
        << dds::core::policy::ResourceLimits((10 * 1048576) / sizeof(T), dds::core::LENGTH_UNLIMITED, dds::core::LENGTH_UNLIMITED);
    if (opts.history == 0)
      qos << dds::core::policy::History::KeepAll();
    else
      qos << dds::core::policy::History::KeepLast(opts.history);
    dds::sub::DataReader<T> rd(sub, tp, qos);
    rd.listener(l, dds::core::status::StatusMask::data_available());
    rds.push_back(rd);
  }

  signal(SIGINT, sigh);
  signal(SIGTERM, sigh);
  while (!interrupted)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  delete l;
}

int main(int argc, char **argv)
{
  const options opts = get_options (argc, argv);
  dds::domain::DomainParticipant dp(0);
  sub<DATATYPE_CPP>(dp, opts);
  return 0;
}
