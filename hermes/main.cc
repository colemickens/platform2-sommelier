// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <memory>

#include <base/bind.h>
#include <base/callback.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "hermes/esim.h"
#include "hermes/lpd.h"
#include "hermes/smdp.h"

// TODO(jruthe): update this with some actual functionality to mimic caller.
void ErrorCallback(hermes::LpdError error) {}

void SuccessCallback() {}

void OnInitialization(hermes::Lpd* lpd) {
  lpd->InstallProfile(base::Bind(&SuccessCallback), base::Bind(&ErrorCallback));
}

void InitError(hermes::LpdError error) {}

void Usage() {
  LOG(INFO) << "usage: ./hermes <smdp_hostname> <imei_number>";
}

int main(int argc, char** argv) {
  DEFINE_int32(log_level, 0,
               "Logging level - 0: LOG(INFO), 1: LOG(WARNING), 2: LOG(ERROR), "
               "-1: VLOG(1), -2: VLOG(2), ...");
  DEFINE_string(smdp_hostname, "", "SM-DP+ server hostname");
  DEFINE_string(imei, "", "IMEI number");
  DEFINE_string(matching_id, "", "Profile's matching ID number");
  brillo::FlagHelper::Init(argc, argv, "Chromium OS eSIM LPD Daemon");
  brillo::InitLog(brillo::kLogToStderrIfTty);
  logging::SetMinLogLevel(FLAGS_log_level);

  base::MessageLoop message_loop(base::MessageLoop::TYPE_IO);
  auto esim = hermes::Esim::Create(FLAGS_imei, FLAGS_matching_id);
  CHECK(esim);
  hermes::Lpd lpd(std::move(esim),
                  std::make_unique<hermes::Smdp>(FLAGS_smdp_hostname));
  lpd.Initialize(base::Bind(&OnInitialization, &lpd), base::Bind(&InitError));
  base::RunLoop().Run();

  return EXIT_SUCCESS;
}
