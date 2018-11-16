// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloader/manifest.h"

#include "base/logging.h"
#include "base/test/fuzzed_data_provider.h"

class Environment {
 public:
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);  // <- DISABLE LOGGING.
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  imageloader::Manifest manifest;
  manifest.ParseManifest(
      std::string(reinterpret_cast<const char*>(data), size));
  return 0;
}
