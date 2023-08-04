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
#include "stats.hpp"
#include "config.h"
#include "gettime.h"

using namespace DDS;

template<typename T>
double bytes(const T& s) {
  return sizeof(s);
}

template<>
double bytes(const i11eperf::seq& s) {
  return 4 + s.xseq.length();
}

template<typename T>
class L : public DataReaderListener {
public:
  L(DomainParticipant *dp, std::string statsname) : dp_(dp), stats_(10000000, statsname) {
    tref_ = gettime();
  }
  
  virtual void on_data_available(DataReader *wrd)
  {
    CONCAT(i11eperf::DATATYPE, DataReader) *rd = NARROW_R(wrd);
    ReturnCode_t rc;
    T x;
    SampleInfo si;
    if ((rc = rd->take_next_sample(x, si)) == RETCODE_OK && si.valid_data) {
      int64_t tnow = gettime();
      stats_.update(x.s, bytes(x), (tnow-tref_)/1e9, (tnow-x.ts)/1e9);
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
  int64_t tref_;
  DomainParticipant *dp_;
  Stats stats_;
};

static volatile sig_atomic_t interrupted = 0;
static void sigh (int sig __attribute__ ((unused))) { interrupted = 1; }

template<typename T>
static void sub(DomainParticipant *dp, std::string statsname)
{
  L<T> l(dp, statsname);

  TYPESUPPORT(DATATYPE);
  ts->register_type(dp, "");
  DDS::Subscriber *sub = dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
  std::vector<CONCAT(i11eperf::DATATYPE, DataReader) *> rds;
  for (int i = 0; i < NTOPICS; i++) {
    std::string name = "Data";
    if (i > 0) name += std::to_string(i);
    DDS::Topic *tp = dp->create_topic(name.c_str(), ts->get_type_name(), TOPIC_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DataReaderQos qos;
    sub->get_default_datareader_qos(qos);
    qos.history.kind = HISTORY_KIND;
    qos.history.depth = HISTORY_DEPTH;
    qos.reliability.kind = RELIABLE_RELIABILITY_QOS;
    qos.reliability.max_blocking_time.sec = 10;
    qos.reliability.max_blocking_time.nanosec = 0;
    qos.resource_limits.max_samples = (10 * 1048576) / sizeof(T);
    DDS::DataReader *rdw = sub->create_datareader(tp, qos, &l, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    rds.push_back(NARROW_R(rdw));
  }

  signal(SIGTERM, sigh);
  while (!interrupted)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

  sub->delete_contained_entities();
  dp->delete_contained_entities();
}

int main(int argc, char **argv)
{
  TheServiceParticipant->default_configuration_file(ACE_TEXT("opendds.ini"));
  DDS::DomainParticipantFactory *dpf = TheParticipantFactoryWithArgs(argc, argv);
  DDS::DomainParticipant *dp = dpf->create_participant(0, PARTICIPANT_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
  sub<DATATYPE_CPP>(dp, argc < 2 ? "" : std::string(argv[1]));
  dpf->delete_participant(dp);
  return 0;
}
