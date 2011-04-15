// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "interface.h"

namespace cryptohome {
namespace gobject {

// Register with the glib type system.
// This macro automatically defines a number of functions and variables
// which are required to make cryptohome functional as a GObject:
// - cryptohome_parent_class
// - cryptohome_get_type()
// - dbus_glib_cryptohome_object_info
// It also ensures that the structs are setup so that the initialization
// functions are called in the appropriate way by g_object_new().
G_DEFINE_TYPE(Cryptohome, cryptohome, G_TYPE_OBJECT);

GObject *cryptohome_constructor(GType gtype,
                                guint n_properties,
                                GObjectConstructParam *properties) {
  GObject *obj;
  GObjectClass *parent_class;
  // Instantiate using the parent class, then extend for local properties.
  parent_class = G_OBJECT_CLASS(cryptohome_parent_class);
  obj = parent_class->constructor(gtype, n_properties, properties);

  Cryptohome *cryptohome = reinterpret_cast<Cryptohome *>(obj);
  cryptohome->service = NULL;

  // We don't have any thing we care to expose to the glib class system.
  return obj;
}

void cryptohome_class_init(CryptohomeClass *real_class) {
  // Called once to configure the "class" structure.
  GObjectClass *gobject_class = G_OBJECT_CLASS(real_class);
  gobject_class->constructor = cryptohome_constructor;
}

void cryptohome_init(Cryptohome *self) { }

// TODO(wad) add error messaging
#define CRYPTOHOME_WRAP_METHOD(_NAME, args...) \
  if (!self->service) { \
    return FALSE; \
  } \
  return self->service->_NAME(args, error);

#define CRYPTOHOME_WRAP_METHOD_NO_ARGS(_NAME) \
  if (!self->service) { \
    return FALSE; \
  } \
  return self->service->_NAME(error);

gboolean cryptohome_check_key(Cryptohome *self,
                              gchar *userid,
                              gchar *key,
                              gboolean *OUT_result,
                              GError **error) {
  CRYPTOHOME_WRAP_METHOD(CheckKey, userid, key, OUT_result);
}
gboolean cryptohome_async_check_key(Cryptohome *self,
                                    gchar *userid,
                                    gchar *key,
                                    gint *OUT_async_id,
                                    GError **error) {
  CRYPTOHOME_WRAP_METHOD(AsyncCheckKey, userid, key, OUT_async_id);
}
gboolean cryptohome_migrate_key(Cryptohome *self,
                                gchar *userid,
                                gchar *from_key,
                                gchar *to_key,
                                gboolean *OUT_result,
                                GError **error) {
  CRYPTOHOME_WRAP_METHOD(MigrateKey, userid, from_key, to_key, OUT_result);
}
gboolean cryptohome_async_migrate_key(Cryptohome *self,
                                      gchar *userid,
                                      gchar *from_key,
                                      gchar *to_key,
                                      gint *OUT_async_id,
                                      GError **error) {
  CRYPTOHOME_WRAP_METHOD(AsyncMigrateKey, userid, from_key, to_key,
                         OUT_async_id);
}
gboolean cryptohome_remove(Cryptohome *self,
                           gchar *userid,
                           gboolean *OUT_result,
                           GError **error) {
  CRYPTOHOME_WRAP_METHOD(Remove, userid, OUT_result);
}
gboolean cryptohome_async_remove(Cryptohome *self,
                                 gchar *userid,
                                 gint *OUT_async_id,
                                 GError **error) {
  CRYPTOHOME_WRAP_METHOD(AsyncRemove, userid, OUT_async_id);
}
gboolean cryptohome_get_system_salt(Cryptohome *self,
                                    GArray **OUT_salt,
                                    GError **error) {
  CRYPTOHOME_WRAP_METHOD(GetSystemSalt, OUT_salt);
}
gboolean cryptohome_is_mounted(Cryptohome *self,
                               gboolean *OUT_is_mounted,
                               GError **error) {
  CRYPTOHOME_WRAP_METHOD(IsMounted, OUT_is_mounted);
}
gboolean cryptohome_mount(Cryptohome *self,
                          gchar *userid,
                          gchar *key,
                          gboolean create_if_missing,
                          gboolean replace_tracked_directories,
                          gchar **tracked_directories,
                          gint *OUT_error_code,
                          gboolean *OUT_result,
                          GError **error) {
  CRYPTOHOME_WRAP_METHOD(Mount, userid, key, create_if_missing,
                         replace_tracked_directories, tracked_directories,
                         OUT_error_code, OUT_result);
}
gboolean cryptohome_async_mount(Cryptohome *self,
                                gchar *userid,
                                gchar *key,
                                gboolean create_if_missing,
                                gboolean replace_tracked_directories,
                                gchar **tracked_directories,
                                gint *OUT_async_id,
                                GError **error) {
  CRYPTOHOME_WRAP_METHOD(AsyncMount, userid, key, create_if_missing,
                         replace_tracked_directories, tracked_directories,
                         OUT_async_id);
}
gboolean cryptohome_mount_guest(Cryptohome *self,
                                gint *OUT_error_code,
                                gboolean *OUT_result,
                                GError **error) {
  CRYPTOHOME_WRAP_METHOD(MountGuest, OUT_error_code, OUT_result);
}
gboolean cryptohome_async_mount_guest(Cryptohome *self,
                                      gint *OUT_async_id,
                                      GError **error) {
  CRYPTOHOME_WRAP_METHOD(AsyncMountGuest, OUT_async_id);
}
gboolean cryptohome_unmount(Cryptohome *self,
                            gboolean *OUT_result,
                            GError **error) {
  CRYPTOHOME_WRAP_METHOD(Unmount, OUT_result);
}
gboolean cryptohome_remove_tracked_subdirectories(Cryptohome *self,
                           gboolean *OUT_result,
                           GError **error) {
  CRYPTOHOME_WRAP_METHOD(RemoveTrackedSubdirectories, OUT_result);
}
gboolean cryptohome_async_remove_tracked_subdirectories(Cryptohome *self,
                                 gint *OUT_async_id,
                                 GError **error) {
  CRYPTOHOME_WRAP_METHOD(AsyncRemoveTrackedSubdirectories, OUT_async_id);
}
gboolean cryptohome_do_automatic_free_disk_space_control(Cryptohome *self,
                                                         gboolean *OUT_result,
                                                         GError **error) {
  CRYPTOHOME_WRAP_METHOD(DoAutomaticFreeDiskSpaceControl, OUT_result);
}
gboolean cryptohome_async_do_automatic_free_disk_space_control(
    Cryptohome *self,
    gint *OUT_async_id,
    GError **error) {
  CRYPTOHOME_WRAP_METHOD(AsyncDoAutomaticFreeDiskSpaceControl, OUT_async_id);
}
gboolean cryptohome_tpm_is_ready(Cryptohome *self,
                                 gboolean *OUT_ready,
                                 GError **error) {
  CRYPTOHOME_WRAP_METHOD(TpmIsReady, OUT_ready);
}
gboolean cryptohome_tpm_is_enabled(Cryptohome *self,
                                   gboolean *OUT_enabled,
                                   GError **error) {
  CRYPTOHOME_WRAP_METHOD(TpmIsEnabled, OUT_enabled);
}
gboolean cryptohome_tpm_get_password(Cryptohome *self,
                                     gchar **OUT_password,
                                     GError **error) {
  CRYPTOHOME_WRAP_METHOD(TpmGetPassword, OUT_password);
}
gboolean cryptohome_tpm_is_owned(Cryptohome *self,
                                 gboolean *OUT_owned,
                                 GError **error) {
  CRYPTOHOME_WRAP_METHOD(TpmIsOwned, OUT_owned);
}
gboolean cryptohome_tpm_is_being_owned(Cryptohome *self,
                                       gboolean *OUT_owning,
                                       GError **error) {
  CRYPTOHOME_WRAP_METHOD(TpmIsBeingOwned, OUT_owning);
}
gboolean cryptohome_tpm_can_attempt_ownership(Cryptohome *self,
                                              GError **error) {
  CRYPTOHOME_WRAP_METHOD_NO_ARGS(TpmCanAttemptOwnership);
}
gboolean cryptohome_tpm_clear_stored_password(Cryptohome *self,
                                              GError **error) {
  CRYPTOHOME_WRAP_METHOD_NO_ARGS(TpmClearStoredPassword);
}
gboolean cryptohome_pkcs11_get_tpm_token_info(Cryptohome *self,
                                              gchar **OUT_label,
                                              gchar **OUT_user_pin,
                                              GError **error) {
  CRYPTOHOME_WRAP_METHOD(Pkcs11GetTpmTokenInfo, OUT_label, OUT_user_pin);
}
gboolean cryptohome_pkcs11_is_tpm_token_ready(Cryptohome *self,
                                    gboolean *OUT_ready,
                                    GError **error) {
  CRYPTOHOME_WRAP_METHOD(Pkcs11IsTpmTokenReady, OUT_ready);
}
gboolean cryptohome_get_status_string(Cryptohome *self,
                                      gchar **OUT_status,
                                      GError **error) {
  CRYPTOHOME_WRAP_METHOD(GetStatusString, OUT_status);
}

gboolean cryptohome_install_attributes_get(Cryptohome *self,
                                gchar* name,
                                GArray** OUT_value,
                                gboolean* OUT_successful,
                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesGet, name, OUT_value, OUT_successful);
}

gboolean cryptohome_install_attributes_set(Cryptohome *self,
                                           gchar* name,
                                           GArray* value,
                                           gboolean* OUT_successful,
                                           GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesSet, name, value, OUT_successful);
}

gboolean cryptohome_install_attributes_finalize(Cryptohome *self,
                                                gboolean* OUT_successful,
                                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesFinalize, OUT_successful);
}

gboolean cryptohome_install_attributes_count(Cryptohome *self,
                                             gint* OUT_count,
                                             GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesCount, OUT_count);
}

gboolean cryptohome_install_attributes_is_ready(Cryptohome *self,
                                                gboolean* OUT_is_ready,
                                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesIsReady, OUT_is_ready);
}

gboolean cryptohome_install_attributes_is_secure(Cryptohome *self,
                                                 gboolean* OUT_is_secure,
                                                 GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesIsSecure, OUT_is_secure);
}

gboolean cryptohome_install_attributes_is_invalid(Cryptohome *self,
                                                  gboolean* OUT_is_invalid,
                                                  GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesIsInvalid, OUT_is_invalid);
}

gboolean cryptohome_install_attributes_is_first_install(
  Cryptohome *self,
  gboolean* OUT_is_first_install,
  GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesIsFirstInstall, OUT_is_first_install);
}

#undef CRYPTOHOME_WRAP_METHOD

}  // namespace gobject
}  // namespace cryptohome
