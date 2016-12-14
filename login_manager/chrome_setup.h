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

namespace chromeos {
namespace ui {
class ChromiumCommandBuilder;
}  // namesspace ui
}  // namespace chromeos

namespace login_manager {

// Initializes a ChromiumCommandBuilder and performs additional Chrome-specific
// setup. Returns environment variables that the caller should export for Chrome
// and arguments that it should pass to the Chrome binary, along with the UID
// that should be used to run Chrome.
//
// Initialization that is common across all Chromium-derived binaries (e.g.
// content_shell, app_shell, etc.) rather than just applying to the Chrome
// browser should be added to libchromeos's ChromiumCommandBuilder class
// instead.
void PerformChromeSetup(bool* is_developer_end_user_out,
                        std::map<std::string, std::string>* env_vars_out,
                        std::vector<std::string>* args_out,
                        uid_t* uid_out);

// Add flags to specify the wallpaper to use. This is called by
// PerformChromeSetup and only present in the header for testing.
// Flags are added to |builder|, and |path_exists| is called to test whether a
// given file exists (e.g. use base::Bind(base::PathExists)).
void SetUpWallpaperFlags(
    chromeos::ui::ChromiumCommandBuilder* builder,
    base::Callback<bool(const base::FilePath&)> path_exists);

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CHROME_SETUP_H_
