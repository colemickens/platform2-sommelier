// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/diagnostics_reporter.h"

#include "shill/glib.h"

namespace shill {

namespace {

base::LazyInstance<DiagnosticsReporter> g_reporter = LAZY_INSTANCE_INITIALIZER;
const char kNetDiagsUpload[] = SHIMDIR "/net-diags-upload";

}  // namespace

DiagnosticsReporter::DiagnosticsReporter() : glib_(NULL) {}

DiagnosticsReporter::~DiagnosticsReporter() {}

// static
DiagnosticsReporter *DiagnosticsReporter::GetInstance() {
  return g_reporter.Pointer();
}

void DiagnosticsReporter::Init(GLib *glib) {
  glib_ = glib;
}

void DiagnosticsReporter::Report() {
  if (!IsReportingEnabled()) {
    return;
  }
  LOG(INFO) << "Spawning " << kNetDiagsUpload;
  CHECK(glib_);
  char *argv[] = { const_cast<char *>(kNetDiagsUpload), NULL };
  char *envp[] = { NULL };
  int status = 0;
  GError *error = NULL;
  if (!glib_->SpawnSync(NULL,
                        argv,
                        envp,
                        static_cast<GSpawnFlags>(0),
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        &status,
                        &error)) {
    LOG(ERROR) << "net-diags-upload failed: "
               << glib_->ConvertErrorToMessage(error);
  }
}

void DiagnosticsReporter::OnConnectivityEvent() {
  LOG(INFO) << "Diagnostics event triggered.";

  // TODO(petkov): Throttle log stashing (crosbug.com/36775).

  // TODO(petkov): Stash away logs for potential inclusion in feedback
  // (crosbug.com/36923).
}

bool DiagnosticsReporter::IsReportingEnabled() {
  // TODO(petkov): Implement this when there's a way to control reporting
  // through policy. crosbug.com/35946.
  return false;
}

}  // namespace shill
