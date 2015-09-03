//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
