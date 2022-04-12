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
#ifndef FASTDDS_I11EPERF_HPP
#define FASTDDS_I11EPERF_HPP

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/core/LoanableArray.hpp>
#include <fastdds/dds/core/LoanableSequence.hpp>
#include <fastdds/dds/topic/TopicDataType.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.h>
#include <fastdds/rtps/transport/UDPv4TransportDescriptor.h>
#include <fastdds/rtps/transport/TCPv6TransportDescriptor.h>
#include <fastdds/rtps/transport/UDPv6TransportDescriptor.h>
#include <fastdds/rtps/transport/shared_mem/SharedMemTransportDescriptor.h>
#include <fastrtps/utils/IPLocator.h>

#include "i11eperf.h"
#include "i11eperfPubSubTypes.h"

template<typename T>
class Traits {
public:
  static std::string name();
  static eprosima::fastdds::dds::TopicDataType *ts();
};

#define TT(t_)                                                          \
  template <>                                                           \
  class Traits<i11eperf::t_> {                                          \
  public:                                                               \
    static std::string name() {                                         \
      return "i11eperf::" #t_;                                          \
    }                                                                   \
    static eprosima::fastdds::dds::TopicDataType *ts() {                \
      return new i11eperf::t_##PubSubType();                            \
    }                                                                   \
  }

TT(ou);
TT(a32);
TT(a128);
TT(a1024);
TT(a16k);
TT(a48k);
TT(a64k);
TT(a1M);
TT(a2M);
TT(a4M);
TT(a8M);

#endif
