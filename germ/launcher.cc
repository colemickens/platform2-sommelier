// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/launcher.h"

#include <base/logging.h>
#include <base/rand_util.h>
#include <chromeos/process.h>

namespace {
const char* kSandboxedServiceTemplate = "germ_template";
}  // namespace

namespace germ {

// Starts with the top half of user ids.
class UidService final {
 public:
  UidService() { min_uid_ = 1 << ((sizeof(uid_t) * 8) >> 1); }
  ~UidService() {}

  uid_t GetUid() {
    return static_cast<uid_t>(base::RandInt(min_uid_, 2 * min_uid_));
  }

 private:
  uid_t min_uid_;
};

Launcher::Launcher() {
  uid_service_.reset(new UidService());
}

Launcher::~Launcher() {}

int Launcher::Run(const std::string& name, const std::string& executable) {
  // initctl start germ_template NAME=yes ENVIRONMENT= EXECUTABLE=/bin/yes
  uid_t uid = uid_service_->GetUid();

  chromeos::ProcessImpl initctl;
  initctl.AddArg("/sbin/initctl");
  initctl.AddArg("start");
  initctl.AddArg(kSandboxedServiceTemplate);
  initctl.AddArg(base::StringPrintf("NAME=%s", name.c_str()));
  initctl.AddArg(base::StringPrintf("ENVIRONMENT=-u %d -g %d -p", uid, uid));
  initctl.AddArg(base::StringPrintf("EXECUTABLE=%s", executable.c_str()));

  // Since we're running 'initctl', and not the executable itself,
  // we wait for it to exit.
  return initctl.Run();
}

}  // namespace germ
