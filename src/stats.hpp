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
#include <fstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

struct tslat {
  double t, l;
};

class Stats {
public:
  Stats(unsigned maxraw, std::string statsname) :
    tnext_(std::chrono::steady_clock::now() + std::chrono::seconds(1)),
    count_(0), bytes_(0), lost_(0), errs_(0), nseq_(UINT32_MAX), nraw_(0), nlats_(0),
    maxraw_(maxraw),
    raw_(new tslat[maxraw]), statsname_(statsname) {
  }

  ~Stats() {
    if (statsname_ != "") {
      std::ofstream wf(statsname_, std::ios::out | std::ios::binary);
      wf.write((char *)raw_, nraw_ * sizeof (raw_[0]));
      wf.close();
    }
  }

  void update(uint32_t seq, size_t bytes, double ts, double lat) {
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
    if (nraw_ < maxraw_)
    {
      raw_[nraw_].t = ts;
      raw_[nraw_].l = lat;
      nraw_++;
    }
    if (nlats_ < sizeof(lats_) / sizeof(lats_[0]))
    {
      lats_[nlats_++] = lat;
    }
  }

  void report() {
    std::chrono::time_point<std::chrono::steady_clock> tnow = std::chrono::steady_clock::now();
    if (tnow > tnext_)
    {
      std::sort(lats_, lats_ + nlats_);
      std::chrono::duration<double> dt = tnow - tnext_ + std::chrono::seconds(1);
      double srate = static_cast<double>(count_) / dt.count();
      double brate = static_cast<double>(bytes_) / dt.count();
      double meanlat = 0.0;
      if (nlats_ > 0)
      {
        for (unsigned i = 0; i < nlats_; i++)
          meanlat += lats_[i];
        meanlat /= nlats_;
      }
      std::cout << std::setprecision(5) << std::setw(7)
                << (srate * 1e-3) << " kS/s "
                << (brate * 8e-9) << " Gb/s "
                << "lost " << lost_ << " errs " << errs_
                << " usmeanlat " << 1e6 * meanlat
                << " us90%lat " << 1e6 * ((nlats_ == 0) ? 0 : lats_[nlats_ - (nlats_ + 9) / 10])
                << std::endl << std::flush;
      tnext_ = tnow + std::chrono::seconds(1);
      count_ = lost_ = nlats_ = 0;
      bytes_ = 0;
    }
  }

private:
  std::chrono::time_point<std::chrono::steady_clock> tnext_;
  std::string statsname_;
  unsigned count_, lost_, errs_, nraw_, maxraw_, nlats_;
  uint64_t bytes_;
  uint32_t nseq_;
  tslat *raw_;
  double lats_[1000];
};

#endif
