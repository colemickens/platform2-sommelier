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
