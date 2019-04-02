// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/ec_constants.h"

#include <poll.h>

namespace diagnostics {

// Folder path exposed by sysfs EC driver.
const char kEcDriverSysfsPath[] = "sys/bus/platform/devices/GOOG000C:00/";

// Folder path to EC properties exposed by sysfs EC driver. Relative path to
// |kEcDriverSysfsPath|.
const char kEcDriverSysfsPropertiesPath[] = "properties/";

// Max request payload size for EC telemetry command.
const int64_t kEcGetTelemetryPayloadMaxSize = 32;

// Devfs node exposed by EC driver to EC telemetry data.
const char kEcGetTelemetryFilePath[] = "dev/wilco_telem0";

// EC event file path.
const char kEcEventFilePath[] = "dev/wilco_event0";

// The driver is expected to populate the |kEcEventFilePath| file, therefore
// this constant holds the specific flag for use with poll().
const int16_t kEcEventFilePollEvents = POLLIN;

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
