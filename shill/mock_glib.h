// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_GLIB_H_
#define SHILL_MOCK_GLIB_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/glib.h"

namespace shill {

class MockGLib : public GLib {
 public:
  MockGLib();
  virtual ~MockGLib();

  MOCK_METHOD2(Base64Decode, guchar *(const gchar *text, gsize *out_len));
  MOCK_METHOD2(Base64Encode, gchar *(const guchar *data, gsize len));
  MOCK_METHOD1(BusUnwatchName, void(guint watcher_id));
  MOCK_METHOD7(BusWatchName,
               guint(GBusType bus_type,
                     const gchar *name,
                     GBusNameWatcherFlags flags,
                     GBusNameAppearedCallback name_appeared_handler,
                     GBusNameVanishedCallback name_vanished_handler,
                     gpointer user_data,
                     GDestroyNotify user_data_free_func));
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
  MOCK_METHOD5(KeyFileGetStringList, gchar **(GKeyFile *key_file,
                                              const gchar *group_name,
                                              const gchar *key,
                                              gsize *length,
                                              GError **error));
  MOCK_METHOD2(KeyFileHasGroup, gboolean(GKeyFile *key_file,
                                         const gchar *group_name));
  MOCK_METHOD4(KeyFileHasKey, gboolean(GKeyFile *key_file,
                                       const gchar *group_name,
                                       const gchar *key,
                                       GError **error));
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
  MOCK_METHOD5(KeyFileSetComment, gboolean(GKeyFile *key_file,
                                           const gchar *group_name,
                                           const gchar *key,
                                           const gchar *comment,
                                           GError **error));
  MOCK_METHOD4(KeyFileSetInteger, void(GKeyFile *key_file,
                                       const gchar *group_name,
                                       const gchar *key,
                                       gint value));
  MOCK_METHOD4(KeyFileSetString, void(GKeyFile *key_file,
                                      const gchar *group_name,
                                      const gchar *key,
                                      const gchar *string));
  MOCK_METHOD5(KeyFileSetStringList, void(GKeyFile *key_file,
                                          const gchar *group_name,
                                          const gchar *key,
                                          const gchar * const list[],
                                          gsize length));
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
  MOCK_METHOD0(TypeInit, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGLib);
};

}  // namespace shill

#endif  // SHILL_MOCK_GLIB_H_
