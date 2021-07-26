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

#include "opendds_i11eperf.hpp"
#include "config.h"

using namespace DDS;

template<typename T>
double bytes(const T& s) {
  return sizeof(s);
}

template<>
double bytes(const i11eperf::seq& s) {
  return 4 + s.xseq.length();
}

class Stats {
public:
  Stats() :
    tnext_(std::chrono::steady_clock::now() + std::chrono::seconds(1)),
    count_(0), bytes_(0), lost_(0), errs_(0), nseq_(UINT32_MAX), nlats_(0) { }

  void update(uint32_t seq, size_t bytes, double lat) {
    if (seq != nseq_) {
      if (nseq_ != UINT32_MAX) {
        if (static_cast<int32_t>(seq - nseq_) < 0)
          errs_ += 1;
        else
          lost_ += seq - nseq_;
      }
    }
    nseq_ = seq + 1;
    count_ += 1;
    bytes_ += bytes;
    if (nlats_ < sizeof (lats_) / sizeof (lats_[0]))
      lats_[nlats_++] = lat;
  }

  void report() {
    std::chrono::time_point<std::chrono::steady_clock> tnow = std::chrono::steady_clock::now();
    if (tnow > tnext_)
    {
      std::sort(lats_, lats_ + nlats_);
      std::chrono::duration<double> dt = tnow - tnext_ + std::chrono::seconds(1);
      double srate = static_cast<double>(count_) / dt.count();
      double brate = static_cast<double>(bytes_) / dt.count();
      std::cout << std::setprecision(5) << std::setw(7)
                << (srate * 1e-3) << " kS/s "
                << (brate * 8e-6) << " Mb/s "
                << lost_ << " lost " << errs_ << " errs "
                << 1e6 * ((nlats_ == 0) ? 0 : lats_[nlats_ - (nlats_ + 9) / 10]) << " us90%lat"
                << std::endl << std::flush;
      tnext_ = tnow + std::chrono::seconds(1);
      count_ = bytes_ = lost_ = nlats_ = 0;
    }
  }

private:
  std::chrono::time_point<std::chrono::steady_clock> tnext_;
  unsigned count_;
  unsigned bytes_;
  unsigned lost_;
  unsigned errs_;
  uint32_t nseq_;
  unsigned nlats_;
  double lats_[1000];
};

template<typename T>
class L : public DataReaderListener {
public:
  L(DomainParticipant *dp) : dp_(dp) {}
  
  virtual void on_data_available(DataReader *wrd)
  {
    CONCAT(i11eperf::DATATYPE, DataReader) *rd = NARROW_R(wrd);
    ReturnCode_t rc;
    T x;
    SampleInfo si;
    if ((rc = rd->take_next_sample(x, si)) == RETCODE_OK) {
      Time_t tnow;
      dp_->get_current_time(tnow);
      const int64_t tnow_s = tnow.sec;
      const int32_t tnow_ns = tnow.nanosec;
      if (si.valid_data) {
        const int64_t s = si.source_timestamp.sec;
        const int32_t ns = si.source_timestamp.nanosec;
        stats_.update(x.s, bytes(x), ((tnow_s - s) * 1000000000 + tnow_ns - ns)/1e9);
      }
    }
    stats_.report();
  }

  virtual void on_requested_deadline_missed (DataReader *rd, const RequestedDeadlineMissedStatus& status) {}
  virtual void on_requested_incompatible_qos (DataReader *rd, const RequestedIncompatibleQosStatus& status) {}
  virtual void on_liveliness_changed (DataReader *rd, const LivelinessChangedStatus& status) {}
  virtual void on_subscription_matched (DataReader *rd, const SubscriptionMatchedStatus& status) {}
  virtual void on_sample_rejected (DataReader *rd, const SampleRejectedStatus& status) {}
  virtual void on_sample_lost(DataReader *rd, const SampleLostStatus& status) {}

private:
  DomainParticipant *dp_;
  Stats stats_;
};

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

template<typename T>
static void sub(DomainParticipant *dp)
{
  TYPESUPPORT(DATATYPE);
  ts->register_type(dp, "");
  DDS::Subscriber *sub = dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
  DDS::Topic *tp = dp->create_topic("Data", ts->get_type_name(), TOPIC_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

  L<T> l(dp);
  DataReaderQos qos;
  sub->get_default_datareader_qos(qos);
  qos.history.kind = HISTORY_KIND;
  qos.history.depth = HISTORY_DEPTH;
  qos.reliability.kind = RELIABLE_RELIABILITY_QOS;
  DDS::DataReader *wrd = sub->create_datareader(tp, qos, &l, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
  CONCAT(i11eperf::DATATYPE, DataReader) *rd = NARROW_R(wrd);

  signal(SIGTERM, sigh);
  while (!interrupted)
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main(int argc, char **argv)
{
  DDS::DomainParticipantFactory *dpf = TheParticipantFactoryWithArgs(argc, argv);
  DDS::DomainParticipant *dp = dpf->create_participant(0, PARTICIPANT_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
  sub<DATATYPE_CPP>(dp);
  return 0;
}
