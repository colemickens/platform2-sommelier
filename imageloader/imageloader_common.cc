// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloader_common.h"

#include <dbus-c++/dbus.h>

namespace imageloader {

void OnQuit(int sig) {
  if (DBus::default_dispatcher)
    DBus::default_dispatcher->leave();
}

}  // namespace imageloader
