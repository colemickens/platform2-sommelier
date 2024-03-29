// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/setup/arc_setup_util.h"

#include <cstddef>
#include <cstdint>
#include <fuzzer/FuzzedDataProvider.h>

#include "base/logging.h"
#include "chromeos-config/libcros_config/fake_cros_config.h"

class Environment {
 public:
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);  // Disable logging.
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  FuzzedDataProvider data_provider(data, size);

  std::string content = data_provider.ConsumeRandomLengthString(size);

  brillo::FakeCrosConfig config;
  while (data_provider.remaining_bytes()) {
    // Cannot use |ConsumeRandomLengthString| in a loop because it can enter
    // infinite loop by always returning empty string.

    size_t path_size = data_provider.ConsumeIntegralInRange<size_t>(
        0, data_provider.remaining_bytes());
    std::string path =
        std::string("/") + data_provider.ConsumeBytesAsString(path_size);

    if (data_provider.remaining_bytes() == 0)
      break;

    size_t property_size = data_provider.ConsumeIntegralInRange<size_t>(
        1, data_provider.remaining_bytes());
    std::string property = data_provider.ConsumeBytesAsString(property_size);

    if (data_provider.remaining_bytes() == 0)
      break;

    size_t val_size = data_provider.ConsumeIntegralInRange<size_t>(
        1, data_provider.remaining_bytes());
    std::string val = data_provider.ConsumeBytesAsString(val_size);

    config.SetString(path, property, val);
  }

  std::string expanded_content;
  arc::ExpandPropertyContents(content, &config, &expanded_content);

  return 0;
}
