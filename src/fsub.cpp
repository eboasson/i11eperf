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

#include "fastdds_i11eperf.hpp"
#include "bytesize.hpp"
#include "stats.hpp"
#include "config.h"
#include "gettime.h"
#include "fcommon.hpp"

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

using namespace std::literals;

template<typename T>
class L : public DataReaderListener {
public:
  L(DomainParticipant *dp, std::string statsname, std::chrono::nanoseconds report_intv, bool loans) : dp_(dp), stats_(10000000, statsname, report_intv), loans_(loans) {
    tref_ = gettime();
  }
  
  void on_data_available(DataReader *rd)
  {
    if (loans_)
      do_loan (rd);
    else
      do_non_loan (rd);
    stats_.report();
  }

private:
  void do_non_loan(DataReader *rd)
  {
    static T x; // should be safe to make it static, large samples won't fit on stack
    ReturnCode_t rc;
    SampleInfo si;
    if ((rc = rd->take_next_sample(&x, &si)) == ReturnCode_t::RETCODE_OK && si.valid_data) {
      const int64_t tnow = gettime();
      stats_.update(x.s(), bytes(x), (tnow-tref_)/1e9, (tnow-x.ts())/1e9);
    }
  }

  void do_loan(DataReader *rd)
  {
    FASTDDS_CONST_SEQUENCE(DataSeq, T);
    DataSeq data;
    SampleInfoSeq si;
    while (ReturnCode_t::RETCODE_OK == rd->take(data, si))
    {
      for (LoanableCollection::size_type i = 0; i < si.length(); ++i)
      {
        const T& x = data[i];
        const int64_t tnow = gettime();
        stats_.update(x.s(), bytes(x), (tnow-tref_)/1e9, (tnow-x.ts())/1e9);
      }
      rd->return_loan (data, si);
    }
  }

  int64_t tref_;
  DomainParticipant *dp_;
  Stats stats_;
  bool loans_;
};

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

template<typename T>
static void sub(DomainParticipant *dp, const options& opts)
{
  L<T> l(dp, std::string(opts.latfile), std::chrono::nanoseconds(opts.report_intv), opts.loans);

  eprosima::fastdds::dds::TypeSupport ts(Traits<T>::ts());
  ts.register_type(dp);
  auto pub = dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
  std::vector<DataReader *> rds;
  for (int i = 0; i < opts.ntopics; i++)
  {
    std::string name = "Data";
    if (i > 0) name += std::to_string(i);
    auto tp = dp->create_topic(name, Traits<T>::name(), TOPIC_QOS_DEFAULT);
    DataReaderQos qos;
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
    auto rd = pub->create_datareader(tp, qos, &l);
    rds.push_back(rd);
  }

  signal(SIGINT, sigh);
  signal(SIGTERM, sigh);
  while (!interrupted)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main(int argc, char **argv)
{
  const options opts = get_options (argc, argv);
  sub<DATATYPE_CPP>(make_participant(opts), opts);
  return 0;
}
