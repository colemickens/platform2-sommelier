// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GLIB_H_
#define SHILL_GLIB_H_

#include <glib.h>

namespace shill {

// A GLib abstraction allowing GLib mocking in tests.
class GLib {
 public:
  virtual ~GLib();

  // g_child_watch_add
  virtual guint ChildWatchAdd(GPid pid,
                              GChildWatchFunc function,
                              gpointer data);
  // g_source_remove
  virtual gboolean SourceRemove(guint tag);
  // g_spawn_async
  virtual gboolean SpawnAsync(const gchar *working_directory,
                              gchar **argv,
                              gchar **envp,
                              GSpawnFlags flags,
                              GSpawnChildSetupFunc child_setup,
                              gpointer user_data,
                              GPid *child_pid,
                              GError **error);
  // g_spawn_close_pid
  virtual void SpawnClosePID(GPid pid);
};

}  // namespace shill

#endif  // SHILL_GLIB_H_
