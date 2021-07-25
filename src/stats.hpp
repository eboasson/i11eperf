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
#ifndef STATS_HPP
#define STATS_HPP

#include <iostream>
#include <iomanip>
#include <chrono>

using namespace std::literals;

class Stats {
public:
  Stats() : tnext_(std::chrono::steady_clock::now() + 1s) {}

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
    auto tnow = std::chrono::steady_clock::now();
    if (tnow > tnext_)
    {
      std::sort(lats_, lats_ + nlats_);
      std::chrono::duration<double> dt = tnow - tnext_ + 1s;
      double srate = static_cast<double>(count_) / dt.count();
      double brate = static_cast<double>(bytes_) / dt.count();
      std::cout << std::setprecision(5) << std::setw(7)
                << (srate * 1e-3) << " kS/s "
                << (brate * 8e-6) << " Mb/s "
                << lost_ << " lost " << errs_ << " errs "
                << 1e6 * ((nlats_ == 0) ? 0 : lats_[nlats_ - (nlats_ + 9) / 10]) << " us90%lat"
                << std::endl << std::flush;
      tnext_ = tnow + 1s;
      count_ = bytes_ = lost_ = nlats_ = 0;
    }
  }

private:
  std::chrono::time_point<std::chrono::steady_clock> tnext_;
  unsigned count_{0};
  unsigned bytes_{0};
  unsigned lost_{0};
  unsigned errs_{0};
  uint32_t nseq_{UINT32_MAX};
  unsigned nlats_{0};
  double lats_[1000];
};

#endif
