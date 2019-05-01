// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This sets up the default glib logger so it logs with the libchrome
// logger instead.

#ifndef GLIB_BRIDGE_GLIB_LOGGER_H_
#define GLIB_BRIDGE_GLIB_LOGGER_H_

#include <string>

namespace glib_bridge {

void ForwardLogs();

}  // namespace glib_bridge

#endif  // GLIB_BRIDGE_GLIB_LOGGER_H_
