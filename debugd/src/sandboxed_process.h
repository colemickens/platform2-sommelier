// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOXED_PROCESS_H
#define SANDBOXED_PROCESS_H

#include <chromeos/process.h>

namespace debugd {

class SandboxedProcess : public chromeos::ProcessImpl {
 public:
  SandboxedProcess();
  ~SandboxedProcess();

  virtual bool Init();

  // Disable the default sandboxing for this process
  virtual void DisableSandbox();

  // Change the default sandboxing for this process
  virtual void SandboxAs(const std::string& user, const std::string& group);

  static const char *kDefaultUser;
  static const char *kDefaultGroup;

 private:
  bool sandboxing_;
  std::string user_;
  std::string group_;
};

};  // namespace debugd

#endif  // SANDBOXED_PROCESS_H
