// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/glib.h"

namespace shill {

GLib::GLib() {}
GLib::~GLib() {}

guint GLib::ChildWatchAdd(GPid pid,
                          GChildWatchFunc function,
                          gpointer data) {
  return g_child_watch_add(pid, function, data);
}

gboolean GLib::SourceRemove(guint tag) {
  return g_source_remove(tag);
}

gboolean GLib::SpawnAsync(const gchar* working_directory,
                          gchar** argv,
                          gchar** envp,
                          GSpawnFlags flags,
                          GSpawnChildSetupFunc child_setup,
                          gpointer user_data,
                          GPid* child_pid,
                          GError** error) {
  return g_spawn_async(working_directory,
                       argv,
                       envp,
                       flags,
                       child_setup,
                       user_data,
                       child_pid,
                       error);
}

void GLib::TypeInit() {
  g_type_init();
}

}  // namespace shill
