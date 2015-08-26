// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GLIB_H_
#define SHILL_GLIB_H_

#include <gio/gio.h>
#include <glib.h>

#include <base/macros.h>

namespace shill {

// A GLib abstraction allowing GLib mocking in tests.
class GLib {
 public:
  GLib();
  virtual ~GLib();

  // g_child_watch_add
  virtual guint ChildWatchAdd(GPid pid,
                              GChildWatchFunc function,
                              gpointer data);
  // g_source_remove
  virtual gboolean SourceRemove(guint tag);
  // g_spawn_async
  virtual gboolean SpawnAsync(const gchar* working_directory,
                              gchar** argv,
                              gchar** envp,
                              GSpawnFlags flags,
                              GSpawnChildSetupFunc child_setup,
                              gpointer user_data,
                              GPid* child_pid,
                              GError** error);
  // g_type_init
  virtual void TypeInit();

 private:
  DISALLOW_COPY_AND_ASSIGN(GLib);
};

}  // namespace shill

#endif  // SHILL_GLIB_H_
