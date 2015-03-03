// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_LAUNCHER_H_
#define GERM_LAUNCHER_H_

#include <memory>
#include <string>

#include <base/macros.h>

namespace germ {

class UidService;

class Launcher {
 public:
  Launcher();
  ~Launcher();

  int RunInteractive(const std::string& name,
                     const std::string& executable);
  int RunService(const std::string& name,
                 const std::string& executable);

 private:
  std::unique_ptr<UidService> uid_service_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace germ

#endif  // GERM_LAUNCHER_H_
