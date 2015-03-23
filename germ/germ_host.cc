// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/germ_host.h"

namespace germ {

int GermHost::Launch(LaunchRequest* request, LaunchResponse* response) {
  int status = launcher_.RunService(request->name(), request->command_line());
  response->set_status(status);
  return 0;
}

}  // namespace germ
