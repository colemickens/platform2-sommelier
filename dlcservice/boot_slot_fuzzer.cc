// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/test/fuzzed_data_provider.h>

#include "dlcservice/boot_device.h"
#include "dlcservice/boot_slot.h"

namespace dlcservice {

// A fake BootDevice that returns fuzzed data.
class FakeBootDevice : public BootDeviceInterface {
 public:
  FakeBootDevice(const std::string boot_device, bool is_removable_device)
      : boot_device_(boot_device), is_removable_device_(is_removable_device) {}
  ~FakeBootDevice() override = default;

  // BootDeviceInterface overrides:
  bool IsRemovableDevice(const std::string& device) override {
    return is_removable_device_;
  }
  std::string GetBootDevice() override { return boot_device_; }

 private:
  std::string boot_device_;
  bool is_removable_device_;

  DISALLOW_COPY_AND_ASSIGN(FakeBootDevice);
};

}  // namespace dlcservice

class Environment {
 public:
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  base::FuzzedDataProvider fuzzed_data_provider(data, size);

  bool is_removable_device = fuzzed_data_provider.ConsumeBool();
  std::string boot_device = fuzzed_data_provider.ConsumeRemainingBytes();

  dlcservice::BootSlot boot_slot(std::make_unique<dlcservice::FakeBootDevice>(
      boot_device, is_removable_device));

  std::string boot_disk_name;
  int num_slots;
  int current_slot;
  boot_slot.GetCurrentSlot(&boot_disk_name, &num_slots, &current_slot);
  return 0;
}
