// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/launcher.h"

#include <base/logging.h>
#include <chromeos/process.h>

namespace {
  const char *kSandboxedServiceTemplate = "germ_template";
}  // namespace

namespace germ {

int Launcher::Run(const std::string& name, const std::string& executable) {
  // initctl start germ_template NAME=yes EXECUTABLE=/bin/yes
  chromeos::ProcessImpl initctl;
  initctl.AddArg("/sbin/initctl");
  initctl.AddArg("start");
  initctl.AddArg(kSandboxedServiceTemplate);
  initctl.AddArg(base::StringPrintf("NAME=%s", name.c_str()));
  initctl.AddArg(base::StringPrintf("EXECUTABLE=%s", executable.c_str()));

  // Since we're running 'initctl', and not the executable itself,
  // we wait for it to exit.
  return initctl.Run();
}

}  // namespace germ
