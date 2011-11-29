// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/stringprintf.h>

#include "shill/glib.h"

namespace shill {

GLib::~GLib() {}

std::string GLib::ConvertErrorToMessage(GError *error) {
  if (!error) {
    return "Unknown GLib error.";
  }
  std::string message =
      base::StringPrintf("GError(%d): %s", error->code, error->message);
  g_error_free(error);
  return message;
}

guchar *GLib::Base64Decode(const gchar *text, gsize *out_len) {
  return g_base64_decode(text, out_len);
}

gchar *GLib::Base64Encode(const guchar *data, gsize len) {
  return g_base64_encode(data, len);
}

void GLib::BusUnwatchName(guint watcher_id) {
  g_bus_unwatch_name(watcher_id);
}

guint GLib::BusWatchName(GBusType bus_type,
                         const gchar *name,
                         GBusNameWatcherFlags flags,
                         GBusNameAppearedCallback name_appeared_handler,
                         GBusNameVanishedCallback name_vanished_handler,
                         gpointer user_data,
                         GDestroyNotify user_data_free_func) {
  return g_bus_watch_name(bus_type,
                          name,
                          flags,
                          name_appeared_handler,
                          name_vanished_handler,
                          user_data,
                          user_data_free_func);
}

guint GLib::ChildWatchAdd(GPid pid,
                          GChildWatchFunc function,
                          gpointer data) {
  return g_child_watch_add(pid, function, data);
}

void GLib::Free(gpointer mem) {
  g_free(mem);
}

void GLib::KeyFileFree(GKeyFile *key_file) {
  g_key_file_free(key_file);
}

gboolean GLib::KeyFileLoadFromFile(GKeyFile *key_file,
                                   const gchar *file,
                                   GKeyFileFlags flags,
                                   GError **error) {
  return g_key_file_load_from_file(key_file, file, flags, error);
}

gboolean GLib::KeyFileGetBoolean(GKeyFile *key_file,
                                 const gchar *group_name,
                                 const gchar *key,
                                 GError **error) {
  return g_key_file_get_boolean(key_file, group_name, key, error);
}

gchar **GLib::KeyFileGetGroups(GKeyFile *key_file,
                               gsize *length) {
  return g_key_file_get_groups(key_file, length);
}

gint GLib::KeyFileGetInteger(GKeyFile *key_file,
                             const gchar *group_name,
                             const gchar *key,
                             GError **error) {
  return g_key_file_get_integer(key_file, group_name, key, error);
}

gchar *GLib::KeyFileGetString(GKeyFile *key_file,
                              const gchar *group_name,
                              const gchar *key,
                              GError **error) {
  return g_key_file_get_string(key_file, group_name, key, error);
}

gchar **GLib::KeyFileGetStringList(GKeyFile *key_file,
                                   const gchar *group_name,
                                   const gchar *key,
                                   gsize *length,
                                   GError **error) {
  return g_key_file_get_string_list(key_file, group_name, key, length, error);
}

gboolean GLib::KeyFileHasGroup(GKeyFile *key_file,
                               const gchar *group_name) {
  return g_key_file_has_group(key_file, group_name);
}

gboolean GLib::KeyFileHasKey(GKeyFile *key_file,
                             const gchar *group_name,
                             const gchar *key,
                             GError **error) {
  return g_key_file_has_key(key_file, group_name, key, error);
}

GKeyFile *GLib::KeyFileNew() {
  return g_key_file_new();
}

void GLib::KeyFileRemoveGroup(GKeyFile *key_file,
                              const gchar *group_name,
                              GError **error) {
  g_key_file_remove_group(key_file, group_name, error);
}

void GLib::KeyFileRemoveKey(GKeyFile *key_file,
                            const gchar *group_name,
                            const gchar *key,
                            GError **error) {
  g_key_file_remove_key(key_file, group_name, key, error);
}

void GLib::KeyFileSetBoolean(GKeyFile *key_file,
                             const gchar *group_name,
                             const gchar *key,
                             gboolean value) {
  g_key_file_set_boolean(key_file, group_name, key, value);
}

gboolean GLib::KeyFileSetComment(GKeyFile *key_file,
                                 const gchar *group_name,
                                 const gchar *key,
                                 const gchar *comment,
                                 GError **error) {
  return g_key_file_set_comment(key_file, group_name, key, comment, error);
}

void GLib::KeyFileSetInteger(GKeyFile *key_file,
                             const gchar *group_name,
                             const gchar *key,
                             gint value) {
  g_key_file_set_integer(key_file, group_name, key, value);
}

void GLib::KeyFileSetString(GKeyFile *key_file,
                            const gchar *group_name,
                            const gchar *key,
                            const gchar *value) {
  g_key_file_set_string(key_file, group_name, key, value);
}

void GLib::KeyFileSetStringList(GKeyFile *key_file,
                                const gchar *group_name,
                                const gchar *key,
                                const gchar * const list[],
                                gsize length) {
  g_key_file_set_string_list(key_file, group_name, key, list, length);
}

gchar *GLib::KeyFileToData(GKeyFile *key_file,
                           gsize *length,
                           GError **error) {
  return g_key_file_to_data(key_file, length, error);
}

gboolean GLib::SourceRemove(guint tag) {
  return g_source_remove(tag);
}

gboolean GLib::SpawnAsync(const gchar *working_directory,
                          gchar **argv,
                          gchar **envp,
                          GSpawnFlags flags,
                          GSpawnChildSetupFunc child_setup,
                          gpointer user_data,
                          GPid *child_pid,
                          GError **error) {
  return g_spawn_async(working_directory,
                       argv,
                       envp,
                       flags,
                       child_setup,
                       user_data,
                       child_pid,
                       error);
}

void GLib::SpawnClosePID(GPid pid) {
  g_spawn_close_pid(pid);
}

void GLib::Strfreev(gchar **str_array) {
  g_strfreev(str_array);
}

void GLib::TypeInit() {
  g_type_init();
}

}  // namespace shill
