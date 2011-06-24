// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GLIB_H_
#define SHILL_GLIB_H_

#include <string>

#include <glib.h>

namespace shill {

// A GLib abstraction allowing GLib mocking in tests.
class GLib {
 public:
  virtual ~GLib();

  // Converts GLib's |error| to a string message and frees the GError object.
  virtual std::string ConvertErrorToMessage(GError *error);

  // g_child_watch_add
  virtual guint ChildWatchAdd(GPid pid,
                              GChildWatchFunc function,
                              gpointer data);
  // g_free
  virtual void Free(gpointer mem);
  // g_key_file_free
  virtual void KeyFileFree(GKeyFile *key_file);
  // g_key_file_get_boolean
  virtual gboolean KeyFileGetBoolean(GKeyFile *key_file,
                                     const gchar *group_name,
                                     const gchar *key,
                                     GError **error);
  // g_key_file_get_groups
  virtual gchar **KeyFileGetGroups(GKeyFile *key_file,
                                   gsize *length);
  // g_key_file_get_integer
  virtual gint KeyFileGetInteger(GKeyFile *key_file,
                                 const gchar *group_name,
                                 const gchar *key,
                                 GError **error);
  // g_key_file_get_string
  virtual gchar *KeyFileGetString(GKeyFile *key_file,
                                  const gchar *group_name,
                                  const gchar *key,
                                  GError **error);
  // g_key_file_has_group
  virtual gboolean KeyFileHasGroup(GKeyFile *key_file,
                                   const gchar *group_name);
  // g_key_file_load_from_file
  virtual gboolean KeyFileLoadFromFile(GKeyFile *key_file,
                                       const gchar *file,
                                       GKeyFileFlags flags,
                                       GError **error);
  // g_key_file_new
  virtual GKeyFile *KeyFileNew();
  // g_key_file_remove_group
  virtual void KeyFileRemoveGroup(GKeyFile *key_file,
                                  const gchar *group_name,
                                  GError **error);
  // g_key_file_remove_key
  virtual void KeyFileRemoveKey(GKeyFile *key_file,
                                const gchar *group_name,
                                const gchar *key,
                                GError **error);
  // g_key_file_set_boolean
  virtual void KeyFileSetBoolean(GKeyFile *key_file,
                                 const gchar *group_name,
                                 const gchar *key,
                                 gboolean value);
  // g_key_file_set_integer
  virtual void KeyFileSetInteger(GKeyFile *key_file,
                                 const gchar *group_name,
                                 const gchar *key,
                                 gint value);
  // g_key_file_set_string
  virtual void KeyFileSetString(GKeyFile *key_file,
                                const gchar *group_name,
                                const gchar *key,
                                const gchar *string);
  // g_key_file_to_data
  virtual gchar *KeyFileToData(GKeyFile *key_file,
                               gsize *length,
                               GError **error);
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
  // g_strfreev
  virtual void Strfreev(gchar **str_array);
};

}  // namespace shill

#endif  // SHILL_GLIB_H_
