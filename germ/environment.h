// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_ENVIRONMENT_H_
#define GERM_ENVIRONMENT_H_

#include <sys/types.h>

#include <string>

#include <chromeos/minijail/minijail.h>

namespace germ {

class Environment {
 public:
  static std::string GetServiceEnvironment(uid_t uid);
  static struct minijail* GetInteractiveEnvironment(uid_t uid);
};

}  // namespace germ

#endif  // GERM_ENVIRONMENT_H_
