// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "bluetooth/newblued/util.h"

namespace bluetooth {

class UtilFuzzer final {
 public:
  static void Fuzz(const uint8_t* data, size_t dataSize) {
    Fuzz_ParseDataIntoUuids(data, dataSize);
    Fuzz_ParseDataIntoServiceData(data, dataSize);

    // TODO(alainmichaud): Fuzz_TrimAdapterFromObjectPath(...)
    // TODO(alainmichaud): Fuzz_TrimDevcieFromObjectPath(...)
    // TODO(alainmichaud): Fuzz_ConvertToBtAddr(...)
    // TODO(alainmichaud): Fuzz_ScanManager::ParseEir(...)
  }

 private:
  static void Fuzz_ParseDataIntoUuids(const uint8_t* data, size_t dataSize) {
    std::set<Uuid> service_uuids;
    ParseDataIntoUuids(&service_uuids, kUuid16Size, data, dataSize);
    ParseDataIntoUuids(&service_uuids, kUuid32Size, data, dataSize);
    ParseDataIntoUuids(&service_uuids, kUuid128Size, data, dataSize);
  }

  static void Fuzz_ParseDataIntoServiceData(const uint8_t* data,
                                            size_t dataSize) {
    std::map<Uuid, std::vector<uint8_t>> service_data;
    ParseDataIntoServiceData(&service_data, kUuid16Size, data, dataSize);
    ParseDataIntoServiceData(&service_data, kUuid32Size, data, dataSize);
    ParseDataIntoServiceData(&service_data, kUuid128Size, data, dataSize);
  }
};
}  // namespace bluetooth

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t dataSize) {
  bluetooth::UtilFuzzer::Fuzz(data, dataSize);
  return 0;
}
