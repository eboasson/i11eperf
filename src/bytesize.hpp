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
#ifndef BYTESIZE_HPP
#define BYTESIZE_HPP

template<typename T>
double bytes(const T& s) {
  return sizeof(s);
}

template<>
double bytes(const i11eperf::seq& s) {
  return 4 + s.xseq().size();
}

#endif
