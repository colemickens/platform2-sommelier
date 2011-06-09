// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GLIB_H_
#define SHILL_GLIB_H_

#include "shill/glib_interface.h"

namespace shill {

// A concrete implementation of the GLib interface.
class GLib : public GLibInterface {
 public:
  virtual gboolean SpawnAsync(const gchar *working_directory,
                              gchar **argv,
                              gchar **envp,
                              GSpawnFlags flags,
                              GSpawnChildSetupFunc child_setup,
                              gpointer user_data,
                              GPid *child_pid,
                              GError **error);
};

}  // namespace shill

#endif  // SHILL_GLIB_H_
