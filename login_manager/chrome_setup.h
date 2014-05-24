// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_CHROME_SETUP_H_
#define LOGIN_MANAGER_CHROME_SETUP_H_

#include <sys/types.h>

#include <map>
#include <string>
#include <vector>

class ChromiumCommandBuilder;
class XServerRunner;

namespace login_manager {

// Initializes a ChromiumCommandBuilder and performs additional Chrome-specific
// setup. If X is being used, it also starts the X server. Returns environment
// variables that the caller should export for Chrome and arguments that it
// should pass to the Chrome binary, along with the UID that should be used to
// run Chrome.
//
// Initialization that is common across all Chromium-derived binaries (e.g.
// content_shell, app_shell, etc.) rather than just applying to the Chrome
// browser should be added to the ChromiumCommandBuilder class instead.
void PerformChromeSetup(std::map<std::string, std::string>* env_vars_out,
                        std::vector<std::string>* chrome_command_out,
                        uid_t* chrome_uid_out);

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CHROME_SETUP_H_
