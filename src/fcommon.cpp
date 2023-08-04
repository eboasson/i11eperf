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
#include "fcommon.hpp"

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

eprosima::fastdds::dds::DomainParticipant *make_participant(const options& opts)
{
  DomainParticipantQos pqos;
  pqos.transport().use_builtin_transports = false;
  auto udp_transport = std::make_shared<UDPv4TransportDescriptor>();
  if (opts.fastdds_onlyloopback)
    udp_transport->interfaceWhiteList.push_back("127.0.0.1");
  pqos.transport().user_transports.push_back(udp_transport);
  if (opts.fastdds_shm)
  {
    auto shm_transport = std::make_shared<SharedMemTransportDescriptor>();
    pqos.transport().user_transports.push_back(shm_transport);
  }
  return DomainParticipantFactory::get_instance()->create_participant(0, pqos);
}
