// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_ANOMALY_DETECTOR_H_
#define CRASH_REPORTER_ANOMALY_DETECTOR_H_

#include <inttypes.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Sends a D-Bus signal indicating a kernel OOM-kill has happened.
// |oom_timestamp_ms| is the system uptime in milliseconds logged by the kernel
// for the OOM-kill.
extern void CDBusSendOomSignal(const int64_t oom_timestamp_ms);

// Scans the syslog as it grows, looking for anomalies, and takes various
// actions depending on each anomaly that it finds.  If |flag_filter| is true,
// the lexer reads from stdin instead of the syslog.  |flag_test| changes the
// behavior slightly, for the purpose of running the integration test.
extern int AnomalyLexer(bool flag_filter, bool flag_test);

// Callback to run crash-reporter.
extern void RunCrashReporter(int filter,
                             const char* flag,
                             const char* input_path);

#ifdef __cplusplus
}
#endif

#endif  // CRASH_REPORTER_ANOMALY_DETECTOR_H_
