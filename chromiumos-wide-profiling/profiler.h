// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PROFILER_H
#define PROFILER_H
#include <string>

class Profiler {
  // A class to run the perf profiler and collect perf.data
  private:
     const std::string perf_location_;
     const std::string event_;
     const std::string frequency_;
     const std::string time_;
     const std::string output_location_;
     Profiler() {};
     Profiler(const Profiler& other) {};
  public:
    Profiler(const std::string& perf_location, const std::string& event,
             const std::string& frequency, const std::string& time,
             const std::string& output_location);
    int DoProfile();
};
#endif
