// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GLIB_INTERFACE_H_
#define SHILL_GLIB_INTERFACE_H_

#include <glib.h>

namespace shill {

// A virtual GLib interface allowing GLib mocking in tests.
class GLibInterface {
 public:
  virtual ~GLibInterface() {}

  // g_child_watch_add
  virtual guint ChildWatchAdd(GPid pid,
                              GChildWatchFunc function,
                              gpointer data) = 0;
  // g_source_remove
  virtual gboolean SourceRemove(guint tag) = 0;
  // g_spawn_async
  virtual gboolean SpawnAsync(const gchar *working_directory,
                              gchar **argv,
                              gchar **envp,
                              GSpawnFlags flags,
                              GSpawnChildSetupFunc child_setup,
                              gpointer user_data,
                              GPid *child_pid,
                              GError **error) = 0;
  // g_spawn_close_pid
  virtual void SpawnClosePID(GPid pid) = 0;
};

}  // namespace shill

#endif  // SHILL_GLIB_INTERFACE_H_
