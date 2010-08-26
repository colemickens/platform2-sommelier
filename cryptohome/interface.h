// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_INTERFACE_H_
#define CRYPTOHOME_INTERFACE_H_

#include <stdlib.h>

#include <base/logging.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/glib/object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <glib-object.h>

#include "service.h"

// Helpers for using GObjects until we can get a C++ wrapper going.
namespace cryptohome {
namespace gobject {  // Namespace hiding the GObject type data.

struct Cryptohome {
  GObject parent_instance;
  Service *service;  // pointer to implementing service.
};
struct CryptohomeClass { GObjectClass parent_class; };

// cryptohome_get_type() is defined in interface.cc by the G_DEFINE_TYPE()
// macro.  This macro defines a number of other GLib class system specific
// functions and variables discussed in interface.cc.
GType cryptohome_get_type();  // defined by G_DEFINE_TYPE

// Interface function prototypes which wrap service.
gboolean cryptohome_check_key(Cryptohome *self,
                              gchar *userid,
                              gchar *key,
                              gboolean *OUT_result,
                              GError **error);
gboolean cryptohome_async_check_key(Cryptohome *self,
                                    gchar *userid,
                                    gchar *key,
                                    gint *OUT_async_id,
                                    GError **error);
gboolean cryptohome_migrate_key(Cryptohome *self,
                                gchar *userid,
                                gchar *from_key,
                                gchar *to_key,
                                gboolean *OUT_result,
                                GError **error);
gboolean cryptohome_async_migrate_key(Cryptohome *self,
                                      gchar *userid,
                                      gchar *from_key,
                                      gchar *to_key,
                                      gint *OUT_async_id,
                                      GError **error);
gboolean cryptohome_remove(Cryptohome *self,
                           gchar *userid,
                           gboolean *OUT_result,
                           GError **error);
gboolean cryptohome_get_system_salt(Cryptohome *self,
                                    GArray **OUT_salt,
                                    GError **error);
gboolean cryptohome_is_mounted(Cryptohome *self,
                               gboolean *OUT_is_mounted,
                               GError **error);
gboolean cryptohome_mount(Cryptohome *self,
                          gchar *userid,
                          gchar *key,
                          gint *OUT_error_code,
                          gboolean *OUT_result,
                          GError **error);
gboolean cryptohome_async_mount(Cryptohome *self,
                                gchar *userid,
                                gchar *key,
                                gint *OUT_async_id,
                                GError **error);
gboolean cryptohome_mount_guest(Cryptohome *self,
                                gint *OUT_error_code,
                                gboolean *OUT_result,
                                GError **error);
gboolean cryptohome_async_mount_guest(Cryptohome *self,
                                      gint *OUT_async_id,
                                      GError **error);
gboolean cryptohome_unmount(Cryptohome *self,
                            gboolean *OUT_result,
                            GError **error);
gboolean cryptohome_tpm_is_ready(Cryptohome *self,
                                 gboolean *OUT_ready,
                                 GError **error);
gboolean cryptohome_tpm_is_enabled(Cryptohome *self,
                                   gboolean *OUT_enabled,
                                   GError **error);
gboolean cryptohome_tpm_get_password(Cryptohome *self,
                                     gchar **OUT_password,
                                     GError **error);

}  // namespace gobject
}  // namespace cryptohome
#endif  // CRYPTOHOME_INTERFACE_H_
