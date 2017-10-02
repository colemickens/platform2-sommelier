// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/modem.h"

#include <map>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <chromeos/dbus/service_constants.h>

namespace modemfwd {

class ModemImpl : public Modem {
 public:
  ModemImpl(const std::string& device_id,
            const std::string& equipment_id,
            const std::string& carrier_id,
            const std::string& firmware_version)
    : device_id_(device_id),
      equipment_id_(equipment_id),
      carrier_id_(carrier_id),
      firmware_version_(firmware_version) {}
  ~ModemImpl() override = default;

  // modemfwd::Modem overrides.
  std::string GetDeviceId() const override { return device_id_; }

  std::string GetEquipmentId() const override { return equipment_id_; }

  std::string GetCarrierId() const override { return carrier_id_; }

  std::string GetMainFirmwareVersion() const override {
    return firmware_version_;
  }

 private:
  std::string device_id_;
  std::string equipment_id_;
  std::string carrier_id_;
  std::string firmware_version_;

  DISALLOW_COPY_AND_ASSIGN(ModemImpl);
};

std::unique_ptr<Modem> CreateModem(
    std::unique_ptr<org::chromium::flimflam::DeviceProxy> device) {
  std::string object_path = device->GetObjectPath().value();
  DVLOG(1) << "Creating modem proxy for " << object_path;

  brillo::ErrorPtr error;
  brillo::VariantDictionary properties;
  if (!device->GetProperties(&properties, &error)) {
    LOG(WARNING) << "Could not get properties for modem " << object_path;
    return nullptr;
  }

  // If we don't have a device ID, modemfwd can't do anything with this modem,
  // so check it first and just return if we can't find it.
  std::string device_id;
  if (!properties[shill::kDeviceIdProperty].GetValue(&device_id)) {
    LOG(INFO) << "Modem " << object_path << " has no device ID, ignoring";
    return nullptr;
  }

  // Equipment ID is also pretty important since we use it as a stable
  // identifier that can distinguish between modems of the same type.
  // Check IMEI first, and if that's not there, check MEID.
  std::string equipment_id;
  if (!properties[shill::kImeiProperty].GetValue(&equipment_id) &&
      !properties[shill::kMeidProperty].GetValue(&equipment_id)) {
    LOG(INFO) << "Modem " << object_path << " has no equipment ID, ignoring";
    return nullptr;
  }

  // These properties may not exist and it's not a big deal if they don't.
  std::map<std::string, std::string> operator_info;
  std::string carrier_id;
  if (properties[shill::kHomeProviderProperty].GetValue(&operator_info))
    carrier_id = operator_info[shill::kOperatorUuidKey];

  std::string firmware_version =
      properties[shill::kFirmwareRevisionProperty].TryGet<std::string>();

  return std::make_unique<ModemImpl>(
      device_id, equipment_id, carrier_id, firmware_version);
}

}  // namespace modemfwd
