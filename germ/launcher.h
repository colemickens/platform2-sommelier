// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_LAUNCHER_H_
#define GERM_LAUNCHER_H_

#include <string>

namespace germ {

class Launcher {
 public:
  static int Run(const std::string& name, const std::string& executable);
};

}  // namespace germ

#endif  // GERM_LAUNCHER_H_
