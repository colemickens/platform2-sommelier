// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_GLIB_H_
#define SHILL_MOCK_GLIB_H_

#include <gmock/gmock.h>

#include "shill/glib.h"

namespace shill {

class MockGLib : public GLib {
 public:
  MOCK_METHOD3(ChildWatchAdd, guint(GPid pid,
                                    GChildWatchFunc function,
                                    gpointer data));
  MOCK_METHOD1(Free, void(gpointer mem));
  MOCK_METHOD1(KeyFileFree, void(GKeyFile *key_file));
  MOCK_METHOD4(KeyFileGetBoolean, gboolean(GKeyFile *key_file,
                                           const gchar *group_name,
                                           const gchar *key,
                                           GError **error));
  MOCK_METHOD2(KeyFileGetGroups, gchar **(GKeyFile *key_file,
                                          gsize *length));
  MOCK_METHOD4(KeyFileGetInteger, gint(GKeyFile *key_file,
                                       const gchar *group_name,
                                       const gchar *key,
                                       GError **error));
  MOCK_METHOD4(KeyFileGetString, gchar *(GKeyFile *key_file,
                                         const gchar *group_name,
                                         const gchar *key,
                                         GError **error));
  MOCK_METHOD2(KeyFileHasGroup, gboolean(GKeyFile *key_file,
                                         const gchar *group_name));
  MOCK_METHOD4(KeyFileLoadFromFile, gboolean(GKeyFile *key_file,
                                             const gchar *file,
                                             GKeyFileFlags flags,
                                             GError **error));
  MOCK_METHOD0(KeyFileNew, GKeyFile *());
  MOCK_METHOD3(KeyFileRemoveGroup, void(GKeyFile *key_file,
                                        const gchar *group_name,
                                        GError **error));
  MOCK_METHOD4(KeyFileRemoveKey, void(GKeyFile *key_file,
                                      const gchar *group_name,
                                      const gchar *key,
                                      GError **error));
  MOCK_METHOD4(KeyFileSetBoolean, void(GKeyFile *key_file,
                                       const gchar *group_name,
                                       const gchar *key,
                                       gboolean value));
  MOCK_METHOD4(KeyFileSetInteger, void(GKeyFile *key_file,
                                       const gchar *group_name,
                                       const gchar *key,
                                       gint value));
  MOCK_METHOD4(KeyFileSetString, void(GKeyFile *key_file,
                                      const gchar *group_name,
                                      const gchar *key,
                                      const gchar *string));
  MOCK_METHOD3(KeyFileToData, gchar *(GKeyFile *key_file,
                                      gsize *length,
                                      GError **error));
  MOCK_METHOD1(SourceRemove, gboolean(guint tag));
  MOCK_METHOD8(SpawnAsync, gboolean(const gchar *working_directory,
                                    gchar **argv,
                                    gchar **envp,
                                    GSpawnFlags flags,
                                    GSpawnChildSetupFunc child_setup,
                                    gpointer user_data,
                                    GPid *child_pid,
                                    GError **error));
  MOCK_METHOD1(SpawnClosePID, void(GPid pid));
  MOCK_METHOD1(Strfreev, void(gchar **str_array));
};

}  // namespace shill

#endif  // SHILL_MOCK_GLIB_H_
