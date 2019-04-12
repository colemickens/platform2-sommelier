// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_CROS_FP_UPDATER_H_
#define BIOD_CROS_FP_UPDATER_H_

#include <string>

#include <base/files/file_path.h>
#include <chromeos/ec/ec_commands.h>

#include "biod/cros_fp_device.h"
#include "biod/cros_fp_firmware.h"

namespace biod {

// These utilities should be absorbed by CrosFpDevice.
// This is a temporary holding place until they can be absorbed.
class CrosFpDeviceUpdateInterface {
 public:
  virtual ~CrosFpDeviceUpdateInterface() = default;
  virtual bool GetVersion(biod::CrosFpDevice::EcVersion* ecver) const = 0;
  virtual bool IsFlashProtectEnabled(bool* status) const = 0;
  virtual bool Flash(const biod::CrosFpFirmware& fw,
                     enum ec_current_image image) const = 0;
  static std::string EcCurrentImageToString(enum ec_current_image image);
};

// These utilities should be absorbed by CrosFpDevice
class CrosFpDeviceUpdate : public CrosFpDeviceUpdateInterface {
 public:
  bool GetVersion(biod::CrosFpDevice::EcVersion* ecver) const override;
  bool IsFlashProtectEnabled(bool* status) const override;
  bool Flash(const CrosFpFirmware& fw,
             enum ec_current_image image) const override;
};

// CrosFpBootUpdateCtrlInterface holds the interfaces for the
// external boot-time environment.
class CrosFpBootUpdateCtrlInterface {
 public:
  virtual ~CrosFpBootUpdateCtrlInterface() = default;
  virtual bool TriggerBootUpdateSplash() const = 0;
  virtual bool ScheduleReboot() const = 0;
};

class CrosFpBootUpdateCtrl : public CrosFpBootUpdateCtrlInterface {
 public:
  bool TriggerBootUpdateSplash() const override;
  bool ScheduleReboot() const override;
};

namespace updater {

extern const char kFirmwareDir[];

enum class FindFirmwareFileStatus {
  kFoundFile,
  kNoDirectory,
  kFileNotFound,
  kMultipleFiles,
};

// Searches for the externally packaged firmware binary using a glob.
// The returned firmware has not been validated.
FindFirmwareFileStatus FindFirmwareFile(const base::FilePath& directory,
                                        base::FilePath* file);
std::string FindFirmwareFileStatusToString(FindFirmwareFileStatus status);

// Checks for external firmware disable mechanism.
bool UpdateDisallowed();

enum class UpdateStatus {
  kUpdateNotNecessary,
  kUpdateSucceeded,
  kUpdateFailed,
};

UpdateStatus DoUpdate(const CrosFpDeviceUpdateInterface& ec_dev,
                      const CrosFpBootUpdateCtrlInterface& boot_ctrl,
                      const CrosFpFirmware& fw);

}  // namespace updater

}  // namespace biod

#endif  // BIOD_CROS_FP_UPDATER_H_
