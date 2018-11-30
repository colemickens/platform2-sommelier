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
#include <google-lpa/lpa/core/lpa.h>

#include "hermes/card_qrtr.h"
#include "hermes/executor.h"
#include "hermes/logger.h"
#include "hermes/smdp.h"
#include "hermes/smds.h"
#include "hermes/socket_qrtr.h"

void OnComplete(int err) {
  if (err) {
    LOG(ERROR) << "LPA method failed with error: " << err;
    return;
  }
  LOG(INFO) << "LPA method completed successfully!";
}

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
  hermes::Executor exe(&message_loop);
  hermes::Logger logger;

  std::unique_ptr<hermes::CardQrtr> card = hermes::CardQrtr::Create(
      std::make_unique<hermes::SocketQrtr>(), &logger, &exe);
  hermes::SmdpFactory smdp(&logger, &exe);
  hermes::SmdsFactory smds;
  lpa::core::Lpa::Builder b;
  b.SetEuiccCard(card.release())
    .SetSmdpClientFactory(&smdp)
    .SetSmdsClientFactory(&smds);

  std::unique_ptr<lpa::core::Lpa> lpa = b.Build();

  // TODO(akhouderchah) This is just here to test google-lpa integration.
  lpa::core::Lpa::DownloadOptions options;
  options.enable_profile = true;
  options.allow_policy_rules = false;
  lpa->DownloadProfile(
    "LPA:1$dev.smdp-plus.rsp.goog$KSW8-K4KT-FUPR-BO83$1.3.6.1.4.1.11129.6.1.1$1",
    std::move(options),
    &exe,
    OnComplete);
  lpa->DeleteProfile("40819990001234567888", &exe, OnComplete);

  base::RunLoop().Run();

  return EXIT_SUCCESS;
}
