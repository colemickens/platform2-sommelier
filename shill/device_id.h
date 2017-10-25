//
// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_DEVICE_ID_H_
#define SHILL_DEVICE_ID_H_

#include <stdint.h>

#include <memory>
#include <string>

#include <base/files/file_path.h>

namespace shill {

// DeviceId is meant to encapsulate a device type so we can implement a quirks
// layer on top of network controller devices if we need to.
class DeviceId {
 public:
  // Add more bus types here as they need to be supported.
  enum class BusType {
    kUsb,
  };

  // DeviceId matching all products by a particular vendor.
  constexpr DeviceId(BusType bus_type, uint16_t vendor_id)
      : bus_type_(bus_type),
        vendor_id_(vendor_id),
        has_product_id_(false),
        product_id_(0) {}
  // DeviceId matching a specific product.
  constexpr DeviceId(BusType bus_type, uint16_t vendor_id, uint16_t product_id)
      : bus_type_(bus_type),
        vendor_id_(vendor_id),
        has_product_id_(true),
        product_id_(product_id) {}

  // Returns true iff |this| describes all products by a vendor, and |other|
  // has the same vendor, or vice versa; or |this| and |other| describe exactly
  // the same product.
  bool Match(const DeviceId& other) const;

  // This string should be unique for each value of DeviceId, so it can
  // be used to index maps, etc.
  // Format: [bus type]:[vendor id]:[product id, or "*" if unspecified]
  std::string AsString() const;

 private:
  BusType bus_type_;
  uint16_t vendor_id_;
  // TODO(ejcaruso): replace with std::optional when available
  bool has_product_id_;
  uint16_t product_id_;
};

// Takes a device |syspath| as would be given by e.g. udev and tries to read
// the bus type and device identifiers.
std::unique_ptr<DeviceId> ReadDeviceIdFromSysfs(const base::FilePath& syspath);

}  // namespace shill

#endif  // SHILL_DEVICE_ID_H_
