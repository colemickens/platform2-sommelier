// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/suspend_configurator.h"

#include <base/files/file_util.h>
#include <base/logging.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"

namespace power_manager {
namespace system {

const base::FilePath SuspendConfigurator::kConsoleSuspendPath(
    "/sys/module/printk/parameters/console_suspend");

void SuspendConfigurator::Init(PrefsInterface* prefs) {
  DCHECK(prefs);
  prefs_ = prefs;
  ConfigureConsoleForSuspend();
}

// TODO(crbug.com/941298) Move powerd_suspend script here eventually.
void SuspendConfigurator::PrepareForSuspend() {}
void SuspendConfigurator::UndoPrepareForSuspend() {}

void SuspendConfigurator::ConfigureConsoleForSuspend() {
  bool pref_val = true;
  bool enable_console = true;

  // If S0iX is enabled, default to disabling console (b/63737106).
  if (prefs_->GetBool(kSuspendToIdlePref, &pref_val) && pref_val)
    enable_console = false;

  // Overwrite the default if the pref is set.
  if (prefs_->GetBool(kEnableConsoleDuringSuspendPref, &pref_val))
    enable_console = pref_val;

  const char console_suspend_val = enable_console ? 'N' : 'Y';
  base::FilePath console_suspend_path =
      GetPrefixedFilePath(SuspendConfigurator::kConsoleSuspendPath);
  if (base::WriteFile(console_suspend_path, &console_suspend_val, 1) != 1) {
    PLOG(ERROR) << "Failed to write " << console_suspend_val << " to "
                << console_suspend_path.value();
  }
  LOG(INFO) << "Console during suspend is "
            << (enable_console ? "enabled" : "disabled");
}

base::FilePath SuspendConfigurator::GetPrefixedFilePath(
    const base::FilePath& file_path) const {
  if (prefix_path_for_testing_.empty())
    return file_path;
  DCHECK(file_path.IsAbsolute());
  return prefix_path_for_testing_.Append(file_path.value().substr(1));
}

}  // namespace system
}  // namespace power_manager
