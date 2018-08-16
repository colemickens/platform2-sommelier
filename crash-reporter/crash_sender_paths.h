// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_CRASH_SENDER_PATHS_H_
#define CRASH_REPORTER_CRASH_SENDER_PATHS_H_

namespace paths {

// File whose existence mocks crash sending.  If empty we pretend the
// crash sending was successful, otherwise unsuccessful.
constexpr char kMockCrashSending[] = "mock-crash-sending";

// Crash sender lock in case the sender is already running.
constexpr char kLockFile[] = "/run/lock/crash_sender";

// File whose existence causes crash sending to be delayed (for testing).
// Must be stateful to enable testing kernel crashes.
constexpr char kPauseCrashSending[] = "/var/lib/crash_sender_paused";

// Directory of restricted certificates which includes certificate for
// URL to send official build crash reports to.
constexpr char kRestrictedCertificatesDirectory[] =
    "/usr/share/chromeos-ca-certificates";

// Path to find which is required for computing the crash rate.
// TODO(satorux): Remove this once the computation is rewritten in C++.
constexpr char kFind[] = "/usr/bin/find";

// Path to metrics_client.
// TODO(satorux): Remove this once metrics stuff is rewritten in C++.
constexpr char kMetricsClient[] = "/usr/bin/metrics_client";

}  // namespace paths

#endif  // CRASH_REPORTER_CRASH_SENDER_PATHS_H_
