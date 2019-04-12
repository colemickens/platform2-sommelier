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

// Directory where crash_sender stores timestamp files, that indicate the
// upload attempts in the past 24 hours.
constexpr char kTimestampsDirectory[] = "/var/lib/crash_sender";

// Directory where crash_sender stores other state information (ex. client ID).
constexpr char kCrashSenderStateDirectory[] = "/var/lib/crash_sender/state";

}  // namespace paths

#endif  // CRASH_REPORTER_CRASH_SENDER_PATHS_H_
