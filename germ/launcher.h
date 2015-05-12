// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_LAUNCHER_H_
#define GERM_LAUNCHER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <base/macros.h>
#include <chromeos/process.h>
#include <soma/read_only_container_spec.h>

#include "germ/environment.h"
#include "germ/proto_bindings/soma_container_spec.pb.h"

namespace germ {

using SomaExecutable = soma::ContainerSpec_Executable;

class UidService;

class Launcher {
 public:
  Launcher();
  virtual ~Launcher();

  bool RunInteractiveCommand(const std::string& name,
                             const std::vector<std::string>& argv,
                             int* status);
  bool RunInteractiveSpec(const soma::ReadOnlyContainerSpec& spec, int* status);

  // Does not return.
  void ExecveInMinijail(const SomaExecutable& executable);

 private:
  bool RunWithMinijail(const Environment& env,
                       const std::vector<char*>& cmdline,
                       int* status);

  std::unique_ptr<UidService> uid_service_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace germ

#endif  // GERM_LAUNCHER_H_
