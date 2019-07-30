// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_CHROME_SETUP_H_
#define LOGIN_MANAGER_CHROME_SETUP_H_

#include <sys/types.h>

#include <map>
#include <string>
#include <vector>

#include <base/callback.h>

namespace base {
class FilePath;
}  // namespace base

namespace brillo {
class CrosConfigInterface;
}  // namespace brillo

namespace chromeos {
namespace ui {
class ChromiumCommandBuilder;
}  // namespace ui
}  // namespace chromeos

namespace login_manager {

// Property name of the wallpaper setting in CrosConfig.
extern const char kWallpaperProperty[];

// Property name of the per-model regulatory label directory in CrosConfig.
extern const char kRegulatoryLabelProperty[];

// Path to get the power button position info from cros_config.
extern const char kPowerButtonPositionPath[];

// Edge property in power button position info.
extern const char kPowerButtonEdgeField[];

// Position property in power button position info.
extern const char kPowerButtonPositionField[];

// Initializes a ChromiumCommandBuilder and performs additional Chrome-specific
// setup. Returns environment variables that the caller should export for Chrome
// and arguments that it should pass to the Chrome binary, along with the UID
// that should be used to run Chrome.
//
// Initialization that is common across all Chromium-derived binaries (e.g.
// content_shell, app_shell, etc.) rather than just applying to the Chrome
// browser should be added to libchromeos's ChromiumCommandBuilder class
// instead.
//
// |cros_config| (if non-null) provides the master configuration (used to look
// up the default wallpaper filename).
void PerformChromeSetup(brillo::CrosConfigInterface* cros_config,
                        bool* is_developer_end_user_out,
                        std::map<std::string, std::string>* env_vars_out,
                        std::vector<std::string>* args_out,
                        uid_t* uid_out);

// Add flags to specify the wallpaper to use. This is called by
// PerformChromeSetup and only present in the header for testing.
// Flags are added to |builder|, and |path_exists| is called to test whether a
// given file exists (e.g. use base::Bind(base::PathExists)).
// |cros_config| (if non-null) provides the master configuration (used to look
// up the default wallpaper filename).
void SetUpWallpaperFlags(
    chromeos::ui::ChromiumCommandBuilder* builder,
    brillo::CrosConfigInterface* cros_config,
    base::Callback<bool(const base::FilePath&)> path_exists);

// Add "--regulatory-label-dir" flag to specify the regulatory label directory
// containing per-region sub-directories, if the model-specific
// regulatory-label read from |cros_config| is non-null.
void SetUpRegulatoryLabelFlag(chromeos::ui::ChromiumCommandBuilder* builder,
                              brillo::CrosConfigInterface* cros_config);

// Add "--ash-power-button-position" flag with value in JSON format read from
// |cros_config|.
void SetUpPowerButtonPositionFlag(chromeos::ui::ChromiumCommandBuilder* builder,
                                  brillo::CrosConfigInterface* cros_config);

// Add "--ash-side-volume-button-position" flag with value in JSON format read
// from |cros_config|.
void SetUpSideVolumeButtonPositionFlag(
    chromeos::ui::ChromiumCommandBuilder* builder,
    brillo::CrosConfigInterface* cros_config);

// Add "--has-internal-stylus" flag if the device has
// an internal stylus.
void SetUpInternalStylusFlag(chromeos::ui::ChromiumCommandBuilder* builder,
                             brillo::CrosConfigInterface* cros_config);

// Add "--fingerprint-sensor-location" flag with value read from |cros_config|.
// If value is not "none".
void SetUpFingerprintSensorLocationFlag(
    chromeos::ui::ChromiumCommandBuilder* builder,
    brillo::CrosConfigInterface* cros_config);

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CHROME_SETUP_H_
