// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DIAGNOSTICS_REPORTER_H_
#define SHILL_DIAGNOSTICS_REPORTER_H_

#include <base/files/file_path.h>
#include <base/lazy_instance.h>

namespace chromeos {

class Minijail;

}  // namespace chromeos

namespace shill {

class ProcessKiller;
class Time;

class DiagnosticsReporter {
 public:
  virtual ~DiagnosticsReporter();

  // This is a singleton -- use DiagnosticsReporter::GetInstance()->Foo().
  static DiagnosticsReporter* GetInstance();

  // Handle a connectivity event -- collect and stash diagnostics data, possibly
  // uploading it for analysis.
  virtual void OnConnectivityEvent();

 protected:
  DiagnosticsReporter();

  virtual bool IsReportingEnabled();

 private:
  friend struct base::DefaultLazyInstanceTraits<DiagnosticsReporter>;
  friend class DiagnosticsReporterTest;

  static const int kLogStashThrottleSeconds;

  chromeos::Minijail* minijail_;
  ProcessKiller* process_killer_;
  Time* time_;
  uint64_t last_log_stash_;  // Monotonic time seconds.
  base::FilePath stashed_net_log_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsReporter);
};

}  // namespace shill

#endif  // SHILL_DIAGNOSTICS_REPORTER_H_
