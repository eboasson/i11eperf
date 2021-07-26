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
#include "fcommon.hpp"

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

using namespace std::literals;

template<typename T>
class L : public DataReaderListener {
public:
  L(DomainParticipant *dp, std::string statsname) : dp_(dp), stats_(10000000, statsname) {
    eprosima::fastrtps::Time_t tref;
    dp_->get_current_time(tref);
    tref_s_ = tref.seconds;
    tref_ns_ = tref.nanosec;
  }
  
  void on_data_available(DataReader *rd)
  {
    ReturnCode_t rc;
    T x;
    SampleInfo si;
    if ((rc = rd->take_next_sample(&x, &si)) == ReturnCode_t::RETCODE_OK) {
      eprosima::fastrtps::Time_t tnow;
      dp_->get_current_time(tnow);
      const int64_t tnow_s = tnow.seconds;
      const int32_t tnow_ns = tnow.nanosec;
      if (si.valid_data) {
        const int64_t s = si.source_timestamp.seconds();
        const int32_t ns = si.source_timestamp.nanosec();
        stats_.update(x.s(), bytes(x),
                      ((tnow_s - tref_s_) * 1000000000 + tnow_ns - tref_ns_)/1e9,
                      ((tnow_s - s) * 1000000000 + tnow_ns - ns)/1e9);
      }
    }
    stats_.report();
  }

private:
  int64_t tref_s_;
  int32_t tref_ns_;
  DomainParticipant *dp_;
  Stats stats_;
};

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

template<typename T>
static void sub(DomainParticipant *dp, std::string statsname)
{
  L<T> l(dp, statsname);

  eprosima::fastdds::dds::TypeSupport ts(Traits<T>::ts());
  ts.register_type(dp);
  auto pub = dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
  auto tp = dp->create_topic("Data", Traits<T>::name(), TOPIC_QOS_DEFAULT);

  DataReaderQos qos;
  qos.history().kind = HISTORY_KIND;
  qos.history().depth = HISTORY_DEPTH;
  qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
  auto rd = pub->create_datareader(tp, qos, &l);

  signal(SIGTERM, sigh);
  while (!interrupted)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main(int argc, char **argv)
{
  sub<DATATYPE_CPP>(make_participant(), argc < 2 ? "" : std::string(argv[1]));
  return 0;
}
