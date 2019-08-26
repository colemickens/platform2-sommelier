// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_ANOMALY_DETECTOR_H_
#define CRASH_REPORTER_ANOMALY_DETECTOR_H_

#include <base/optional.h>

#include <string>

#include <inttypes.h>

namespace anomaly {

struct CrashReport {
  std::string text;
  std::string flag;
};

using MaybeCrashReport = base::Optional<CrashReport>;

constexpr size_t HASH_BITMAP_SIZE(1 << 15);

class Parser {
 public:
  virtual ~Parser() = 0;

  virtual MaybeCrashReport ParseLogEntry(const std::string& line) = 0;

  virtual bool WasAlreadySeen(uint32_t hash);

 private:
  std::bitset<HASH_BITMAP_SIZE> hash_bitmap_;
};

class ServiceParser : public Parser {
 public:
  MaybeCrashReport ParseLogEntry(const std::string& line) override;
};

class SELinuxParser : public Parser {
 public:
  MaybeCrashReport ParseLogEntry(const std::string& line) override;
};

class KernelParser : public Parser {
 public:
  MaybeCrashReport ParseLogEntry(const std::string& line) override;

 private:
  enum class LineType {
    None,
    Header,
    Start,
    Body,
  };

  LineType last_line_ = LineType::None;
  std::string text_;
  std::string flag_;
};

}  // namespace anomaly

#endif  // CRASH_REPORTER_ANOMALY_DETECTOR_H_
