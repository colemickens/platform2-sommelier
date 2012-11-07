// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/diagnostics_reporter.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <base/bind.h>

#include "shill/event_dispatcher.h"

using base::Bind;

namespace shill {

namespace {

base::LazyInstance<DiagnosticsReporter> g_reporter = LAZY_INSTANCE_INITIALIZER;

}  // namespace

DiagnosticsReporter::DiagnosticsReporter()
    : dispatcher_(NULL),
      weak_ptr_factory_(this) {}

DiagnosticsReporter::~DiagnosticsReporter() {}

// static
DiagnosticsReporter *DiagnosticsReporter::GetInstance() {
  return g_reporter.Pointer();
}

void DiagnosticsReporter::Init(EventDispatcher *dispatcher) {
  dispatcher_ = dispatcher;
}

void DiagnosticsReporter::Report() {
  // Trigger the crash from the main event loop to try to ensure minimal and
  // consistent stack trace. TODO(petkov): Look into invoking crash_reporter
  // directly without actually triggering a crash.
  dispatcher_->PostTask(Bind(&DiagnosticsReporter::TriggerCrash,
                             weak_ptr_factory_.GetWeakPtr()));
}

bool DiagnosticsReporter::IsReportingEnabled() {
  // TODO(petkov): Implement this when there's a way to control reporting
  // through policy. crosbug.com/35946.
  return false;
}

void DiagnosticsReporter::TriggerCrash() {
  if (!IsReportingEnabled()) {
    return;
  }
  pid_t pid = fork();
  if (pid < 0) {
    PLOG(ERROR) << "fork() failed.";
    NOTREACHED();
    return;
  }
  if (pid == 0) {
    // Crash the child.
    abort();
  }
  // The parent waits for the child to terminate.
  pid_t result = waitpid(pid, NULL, 0);
  PLOG_IF(ERROR, result < 0) << "waitpid() failed.";
}

}  // namespace shill
