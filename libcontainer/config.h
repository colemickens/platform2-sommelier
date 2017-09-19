// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCONTAINER_CONFIG_H_
#define LIBCONTAINER_CONFIG_H_

#include <base/macros.h>
#include <brillo/brillo_export.h>

#include "libcontainer/libcontainer.h"

namespace libcontainer {

class BRILLO_EXPORT Config {
 public:
  BRILLO_EXPORT Config();
  BRILLO_EXPORT ~Config();

  BRILLO_EXPORT container_config* get() const { return config_; }

 private:
  container_config* const config_;

  DISALLOW_COPY_AND_ASSIGN(Config);
};

}  // namespace libcontainer

#endif  // LIBCONTAINER_CONFIG_H_
