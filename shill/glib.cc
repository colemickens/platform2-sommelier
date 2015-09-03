//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
