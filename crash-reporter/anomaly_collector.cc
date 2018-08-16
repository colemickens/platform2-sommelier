// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/anomaly_collector.h"

#include <string>

#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

int main(int argc, char* argv[]) {
  DEFINE_bool(filter, false, "Input is stdin and output is stdout");
  DEFINE_bool(test, false, "Run self-tests");

  brillo::FlagHelper::Init(argc, argv, "Crash Helper: Anomaly Collector");
  brillo::OpenLog("anomaly_collector", true);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  return AnomalyLexer(FLAGS_filter, FLAGS_test);
}
