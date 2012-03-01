// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
struct CryptohomeClass {
  GObjectClass parent_class;
};

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
gboolean cryptohome_async_remove(Cryptohome *self,
                                 gchar *userid,
                                 gint *OUT_async_id,
                                 GError **error);
gboolean cryptohome_get_system_salt(Cryptohome *self,
                                    GArray **OUT_salt,
                                    GError **error);
gboolean cryptohome_is_mounted(Cryptohome *self,
                               gboolean *OUT_is_mounted,
                               GError **error);
gboolean cryptohome_is_mounted_for_user(Cryptohome *self,
                                        gchar *userid,
                                        gboolean *OUT_is_mounted,
                                        GError **error);
gboolean cryptohome_mount(Cryptohome *self,
                          gchar *userid,
                          gchar *key,
                          gboolean create_if_missing,
                          gboolean replace_tracked_directories,
                          gchar **tracked_directories,
                          gint *OUT_error_code,
                          gboolean *OUT_result,
                          GError **error);
gboolean cryptohome_async_mount(Cryptohome *self,
                                gchar *userid,
                                gchar *key,
                                gboolean create_if_missing,
                                gboolean replace_tracked_directories,
                                gchar **tracked_directories,
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
gboolean cryptohome_unmount_for_user(Cryptohome *self,
                                     gchar *userid,
                                     gboolean *OUT_result,
                                     GError **error);
gboolean cryptohome_remove_tracked_subdirectories(Cryptohome *self,
                                                  gboolean *OUT_result,
                                                  GError **error);
gboolean cryptohome_async_remove_tracked_subdirectories(Cryptohome *self,
                                                        gint *OUT_async_id,
                                                        GError **error);
gboolean cryptohome_do_automatic_free_disk_space_control(Cryptohome *self,
                                                         gboolean *OUT_result,
                                                         GError **error);
gboolean cryptohome_async_do_automatic_free_disk_space_control(
    Cryptohome *self,
    gint *OUT_async_id,
    GError **error);
gboolean cryptohome_async_update_current_user_activity_timestamp(
    Cryptohome *self,
    gint time_shift_sec,
    gint *OUT_async_id,
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
gboolean cryptohome_tpm_is_owned(Cryptohome *self,
                                 gboolean *OUT_owned,
                                 GError **error);
gboolean cryptohome_tpm_is_being_owned(Cryptohome *self,
                                       gboolean *OUT_owning,
                                       GError **error);
gboolean cryptohome_tpm_can_attempt_ownership(Cryptohome *self,
                                              GError **error);
gboolean cryptohome_tpm_clear_stored_password(Cryptohome *self,
                                              GError **error);
gboolean cryptohome_pkcs11_get_tpm_token_info(Cryptohome *self,
                                              gchar **OUT_label,
                                              gchar **OUT_user_pin,
                                              GError **error);
gboolean cryptohome_pkcs11_get_tpm_token_info_for_user(Cryptohome *self,
                                                       gchar *username,
                                                       gchar **OUT_label,
                                                       gchar **OUT_user_pin,
                                                       GError **error);
gboolean cryptohome_pkcs11_is_tpm_token_ready(Cryptohome *self,
                                              gboolean *OUT_ready,
                                              GError **error);
gboolean cryptohome_pkcs11_is_tpm_token_ready_for_user(Cryptohome *self,
                                                       gchar *username,
                                                       gboolean *OUT_ready,
                                                       GError **error);
gboolean cryptohome_get_status_string(Cryptohome *self,
                                      gchar **OUT_status,
                                      GError **error);
gboolean cryptohome_install_attributes_get(Cryptohome *self,
                                           gchar* name,
                                           GArray** OUT_value,
                                           gboolean* OUT_successful,
                                           GError** error);
gboolean cryptohome_install_attributes_set(Cryptohome *self,
                                           gchar* name,
                                           GArray* value,
                                           gboolean* OUT_successful,
                                           GError** error);
gboolean cryptohome_install_attributes_finalize(Cryptohome *self,
                                                gboolean* OUT_successful,
                                                GError** error);
gboolean cryptohome_install_attributes_count(Cryptohome *self,
                                             gint* OUT_count,
                                             GError** error);
gboolean cryptohome_install_attributes_is_ready(Cryptohome *self,
                                                gboolean* OUT_is_ready,
                                                GError** error);
gboolean cryptohome_install_attributes_is_secure(Cryptohome *self,
                                                 gboolean* OUT_is_secure,
                                                 GError** error);
gboolean cryptohome_install_attributes_is_invalid(Cryptohome *self,
                                                  gboolean* OUT_is_invalid,
                                                  GError** error);
gboolean cryptohome_install_attributes_is_first_install(
                                                Cryptohome *self,
                                                gboolean* OUT_is_first_install,
                                                GError** error);

}  // namespace gobject
}  // namespace cryptohome
#endif  // CRYPTOHOME_INTERFACE_H_
