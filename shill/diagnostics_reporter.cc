// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/diagnostics_reporter.h"

#include "shill/glib.h"
#include "shill/shill_time.h"

namespace shill {

namespace {

base::LazyInstance<DiagnosticsReporter> g_reporter = LAZY_INSTANCE_INITIALIZER;
const char kNetDiagsUpload[] = SHIMDIR "/net-diags-upload";

}  // namespace

// static
const int DiagnosticsReporter::kLogStashThrottleSeconds = 30 * 60;

DiagnosticsReporter::DiagnosticsReporter()
    : glib_(NULL),
      time_(Time::GetInstance()),
      last_log_stash_(0) {}

DiagnosticsReporter::~DiagnosticsReporter() {}

// static
DiagnosticsReporter *DiagnosticsReporter::GetInstance() {
  return g_reporter.Pointer();
}

void DiagnosticsReporter::Init(GLib *glib) {
  glib_ = glib;
}

void DiagnosticsReporter::OnConnectivityEvent() {
  LOG(INFO) << "Diagnostics event triggered.";

  struct timeval now = (const struct timeval){ 0 };
  time_->GetTimeMonotonic(&now);
  if (last_log_stash_ &&
      last_log_stash_ + kLogStashThrottleSeconds >
      static_cast<uint64>(now.tv_sec)) {
    LOG(INFO) << "Diagnostics throttled.";
    return;
  }

  last_log_stash_ = now.tv_sec;

  LOG(INFO) << "Spawning " << kNetDiagsUpload << " @ " << last_log_stash_;
  CHECK(glib_);
  char *argv[] = {
    const_cast<char *>(kNetDiagsUpload),
    IsReportingEnabled() ? const_cast<char *>("--upload") : NULL,
    NULL
  };
  char *envp[] = { NULL };
  GError *error = NULL;
  // TODO(petkov): Run the subprocess through minijail (crosbug.com/37099).
  if (!glib_->SpawnAsync(NULL,
                         argv,
                         envp,
                         static_cast<GSpawnFlags>(0),
                         NULL,
                         NULL,
                         NULL,
                         &error)) {
    LOG(ERROR) << "Spawn failed: " << glib_->ConvertErrorToMessage(error);
  }
}

bool DiagnosticsReporter::IsReportingEnabled() {
  // TODO(petkov): Implement this when there's a way to control reporting
  // through policy. crosbug.com/35946.
  return false;
}

}  // namespace shill
