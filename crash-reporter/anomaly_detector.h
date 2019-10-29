// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_ANOMALY_DETECTOR_H_
#define CRASH_REPORTER_ANOMALY_DETECTOR_H_

#include <base/optional.h>
#include <base/time/time.h>
#include <dbus/bus.h>

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

 protected:
  enum class LineType {
    None,
    Header,
    Start,
    Body,
  };

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
  LineType last_line_ = LineType::None;
  std::string text_;
  std::string flag_;

  // Timestamp of last time crash_reporter failed.
  base::TimeTicks crash_reporter_last_crashed_ = base::TimeTicks();
};

class SuspendParser : public Parser {
 public:
  MaybeCrashReport ParseLogEntry(const std::string& line) override;

 private:
  LineType last_line_ = LineType::None;
  std::string dev_str_;
  std::string errno_str_;
  std::string step_str_;
};

class TerminaParser {
 public:
  explicit TerminaParser(scoped_refptr<dbus::Bus> dbus);
  MaybeCrashReport ParseLogEntry(const std::string& tag,
                                 const std::string& line);

 private:
  scoped_refptr<dbus::Bus> dbus_;
};

}  // namespace anomaly

#endif  // CRASH_REPORTER_ANOMALY_DETECTOR_H_
