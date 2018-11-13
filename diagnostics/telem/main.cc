// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <base/values.h>
#include <brillo/flag_helper.h>

#include "diagnostics/telem/telem_connection.h"

namespace {
// URI on which the gRPC interface exposed by the diagnosticsd daemon is
// listening. This is the same URI that the diagnosticsd daemon uses to
// communicate with the diagnostics_processor, so this tool should not
// be used when diagnostics_processor is running.
constexpr char kDiagnosticsdGrpcUri[] =
    "unix:/run/diagnostics/grpc_sockets/diagnosticsd_socket";
}  // namespace

// 'telem' command-line tool:
//
// Test driver for libtelem. Only supports requesting individual telemetry
// items. This program does not exercise the caching functionality of libtelem.
int main(int argc, char** argv) {
  DEFINE_string(item, "", "Telemetry item to retrieve.");
  brillo::FlagHelper::Init(argc, argv, "telem - Device telemetry tool.");

  // Mapping between command line arguments and TelemetryItemEnums.
  const std::map<std::string, diagnostics::TelemetryItemEnum> kArgMap = {
      {"uptime", diagnostics::TelemetryItemEnum::kUptime},
      {"memtotal", diagnostics::TelemetryItemEnum::kMemTotalMebibytes},
      {"memfree", diagnostics::TelemetryItemEnum::kMemFreeMebibytes},
      {"loadavg", diagnostics::TelemetryItemEnum::kLoadAvg},
      {"stat", diagnostics::TelemetryItemEnum::kStat},
      {"button", diagnostics::TelemetryItemEnum::kAcpiButton},
      {"netstat", diagnostics::TelemetryItemEnum::kNetStat},
      {"netdev", diagnostics::TelemetryItemEnum::kNetDev}};

  logging::InitLogging(logging::LoggingSettings());

  base::MessageLoopForIO message_loop;

  diagnostics::TelemConnection telem_connection;

  telem_connection.Connect(kDiagnosticsdGrpcUri);

  // Validate the --item flag.
  if (FLAGS_item == "") {
    LOG(ERROR) << "No item specified.";
    return EXIT_FAILURE;
  }
  if (!kArgMap.count(FLAGS_item)) {
    LOG(ERROR) << "Invalid item: " << FLAGS_item;
    return EXIT_FAILURE;
  }

  // Retrieve the telemetry item.
  const base::Value* telem_item = telem_connection.GetItem(
      kArgMap.at(FLAGS_item), base::TimeDelta::FromSeconds(0));
  if (!telem_item) {
    LOG(ERROR) << "Failed to retrieve item: " << FLAGS_item;
    return EXIT_FAILURE;
  }
  if (telem_item->is_int()) {
    int val;
    telem_item->GetAsInteger(&val);
    std::cout << val << std::endl;
  } else if (telem_item->is_string()) {
    std::string val;
    telem_item->GetAsString(&val);
    std::cout << val << std::endl;
  } else {
    std::cout << "Error retrieving item: " << FLAGS_item << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
