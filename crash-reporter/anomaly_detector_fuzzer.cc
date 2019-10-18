// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/anomaly_detector.h"

#include <cstddef>
#include <cstdint>

#include <base/logging.h>
#include <fuzzer/FuzzedDataProvider.h>
#include <brillo/process.h>

namespace {
class Environment {
 public:
  Environment() {
    // Disable logging per instructions.
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  FuzzedDataProvider stream(data, size);

  std::map<std::string, std::unique_ptr<anomaly::Parser>> parsers;
  parsers["audit"] = std::make_unique<anomaly::SELinuxParser>();
  parsers["init"] = std::make_unique<anomaly::ServiceParser>();
  parsers["kernel"] = std::make_unique<anomaly::KernelParser>();
  parsers["powerd_suspend"] = std::make_unique<anomaly::SuspendParser>();

  const std::string journalTags[4] = {"audit", "init", "kernel",
                                      "powerd_suspend"};

  while (stream.remaining_bytes() > 1) {
    const std::string tag = stream.PickValueInArray<std::string>(journalTags);
    const std::string message =
        stream.ConsumeRandomLengthString(stream.remaining_bytes());
    parsers[tag]->ParseLogEntry(message);
  }
  return 0;
}
