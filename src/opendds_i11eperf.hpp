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
#ifndef OPENDDS_I11EPERF_HPP
#define OPENDDS_I11EPERF_HPP

#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/PublisherImpl.h>
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/StaticIncludes.h>
#include <dds/DCPS/WaitSet.h>

#include "i11eperfC.h"
#include "i11eperfTypeSupportImpl.h"

#define TYPESUPPORT(t_) CONCAT(i11eperf::t_, TypeSupport) *ts = new CONCAT(i11eperf::t_, TypeSupportImpl)()
#define NARROW_W(w_) CONCAT(i11eperf::DATATYPE, DataWriter)::_narrow(w_)
#define NARROW_R(r_) CONCAT(i11eperf::DATATYPE, DataReader)::_narrow(r_)

#endif
