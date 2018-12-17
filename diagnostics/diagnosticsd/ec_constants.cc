// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/ec_constants.h"

namespace diagnostics {

// Folder path exposed by sysfs EC driver.
const char kEcDriverSysfsPath[] = "sys/bus/platform/devices/GOOG000C:00/";

// Folder path to EC properties exposed by sysfs EC driver. Relative path to
// |kEcDriverSysfsPath|.
const char kEcDriverSysfsPropertiesPath[] = "properties/";

// Max RunEcCommand request payload size.
//
// TODO(lamzin, crbug.com/904401): replace by real payload max size when EC
// driver will be ready.
const int64_t kEcRunCommandPayloadMaxSize = 32;

// File for running EC command exposed by sysfs EC driver. Relative path to
// |kEcDriverSysfsPath|.
//
// TODO(lamzin, crbug.com/904401): replace by real file path when EC driver
// will be ready.
const char kEcRunCommandFilePath[] = "raw";

// Please keep in sync list of properties with
// "//third_party/kernel/drivers/platform/chrome/wilco_ec_properties.h"

// EC property |global_mic_mute_led|.
const char kEcPropertyGlobalMicMuteLed[] = "global_mic_mute_led";

// EC property |fn_lock|.
const char kEcPropertyFnLock[] = "fn_lock";

// EC property |nic|.
const char kEcPropertyNic[] = "nic";

// EC property |ext_usb_port_en|.
const char kEcPropertyExtUsbPortEn[] = "ext_usb_port_en";

// EC property |wireless_sw_wlan|.
const char kEcPropertyWirelessSwWlan[] = "wireless_sw_wlan";

// EC property |auto_boot_on_trinity_dock_attach|.
const char kEcPropertyAutoBootOnTrinityDockAttach[] =
    "auto_boot_on_trinity_dock_attach";

// EC property |ich_azalia_en|.
const char kEcPropertyIchAzaliaEn[] = "ich_azalia_en";

// EC property |sign_of_life_kbbl|.
const char kEcPropertySignOfLifeKbbl[] = "sign_of_life_kbbl";

}  // namespace diagnostics
