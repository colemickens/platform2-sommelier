// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_EC_CONSTANTS_H_
#define DIAGNOSTICS_DIAGNOSTICSD_EC_CONSTANTS_H_

#include <cstdint>

namespace diagnostics {

// Folder path exposed by sysfs EC driver.
extern const char kEcDriverSysfsPath[];

// Folder path to EC properties exposed by sysfs EC driver. Relative path to
// |kEcDriverSysfsPath|.
extern const char kEcDriverSysfsPropertiesPath[];

// Max RunEcCommand request payload size.
//
// TODO(lamzin, crbug.com/904401): replace by real payload max size when EC
// driver will be ready.
extern const int64_t kEcRunCommandPayloadMaxSize;

// File for running EC command exposed by sysfs EC driver. Relative path to
// |kEcDriverSysfsPath|.
//
// TODO(lamzin, crbug.com/904401): replace by real file path when EC driver
// will be ready.
extern const char kEcRunCommandFilePath[];

// EC event sysfs file path.
//
// Prefix is equal to kEcDriverSysfsPath, but this constant will be changed to
// "/sys/class/chromeos/wilco_ec/event" in very near future.
extern const char kEcEventSysfsPath[];

// The driver is expected to populate the |kEcEventSysfsPath| file with
// high-priority data, therefore this constant holds the specific flag for use
// with poll().
extern const int16_t kEcEventPollEvents;

// Please keep in sync list of properties with
// "//third_party/kernel/drivers/platform/chrome/wilco_ec_properties.h"

// EC property |global_mic_mute_led|.
extern const char kEcPropertyGlobalMicMuteLed[];

// EC property |fn_lock|.
extern const char kEcPropertyFnLock[];

// EC property |nic|.
extern const char kEcPropertyNic[];

// EC property |ext_usb_port_en|.
extern const char kEcPropertyExtUsbPortEn[];

// EC property |wireless_sw_wlan|.
extern const char kEcPropertyWirelessSwWlan[];

// EC property |auto_boot_on_trinity_dock_attach|.
extern const char kEcPropertyAutoBootOnTrinityDockAttach[];

// EC property |ich_azalia_en|.
extern const char kEcPropertyIchAzaliaEn[];

// EC property |sign_of_life_kbbl|.
extern const char kEcPropertySignOfLifeKbbl[];

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_EC_CONSTANTS_H_
