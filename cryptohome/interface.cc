// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cryptohome/interface.h"

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

GObject* cryptohome_constructor(GType gtype,
                                guint n_properties,
                                GObjectConstructParam* properties) {
  GObject* obj;
  GObjectClass* parent_class;
  // Instantiate using the parent class, then extend for local properties.
  parent_class = G_OBJECT_CLASS(cryptohome_parent_class);
  obj = parent_class->constructor(gtype, n_properties, properties);

  Cryptohome* cryptohome = reinterpret_cast<Cryptohome* >(obj);
  cryptohome->service = NULL;

  // We don't have any thing we care to expose to the glib class system.
  return obj;
}

void cryptohome_class_init(CryptohomeClass* real_class) {
  // Called once to configure the "class" structure.
  GObjectClass* gobject_class = G_OBJECT_CLASS(real_class);
  gobject_class->constructor = cryptohome_constructor;
}

void cryptohome_init(Cryptohome* self) { }

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

gboolean cryptohome_check_key(Cryptohome* self,
                              gchar* userid,
                              gchar* key,
                              gboolean* OUT_result,
                              GError** error) {
  CRYPTOHOME_WRAP_METHOD(CheckKey, userid, key, OUT_result);
}
gboolean cryptohome_async_check_key(Cryptohome* self,
                                    gchar* userid,
                                    gchar* key,
                                    gint* OUT_async_id,
                                    GError** error) {
  CRYPTOHOME_WRAP_METHOD(AsyncCheckKey, userid, key, OUT_async_id);
}
gboolean cryptohome_check_key_ex(Cryptohome* self,
                                 GArray* identifier,
                                 GArray* authorization,
                                 GArray* request,
                                 DBusGMethodInvocation* error) {
  // Leave the response called error to reuse WRAP.
  CRYPTOHOME_WRAP_METHOD(CheckKeyEx, identifier, authorization, request);
}
gboolean cryptohome_remove_key_ex(Cryptohome* self,
                                  GArray* identifier,
                                  GArray* authorization,
                                  GArray* request,
                                  DBusGMethodInvocation* error) {
  // Leave the response called error to reuse WRAP.
  CRYPTOHOME_WRAP_METHOD(RemoveKeyEx, identifier, authorization, request);
}
gboolean cryptohome_get_key_data_ex(Cryptohome* self,
                                    GArray* identifier,
                                    GArray* authorization,
                                    GArray* request,
                                    DBusGMethodInvocation* error) {
  // Leave the response called error to reuse WRAP.
  CRYPTOHOME_WRAP_METHOD(GetKeyDataEx, identifier, authorization, request);
}
gboolean cryptohome_list_keys_ex(Cryptohome* self,
                                 GArray* identifier,
                                 GArray* authorization,
                                 GArray* request,
                                 DBusGMethodInvocation* error) {
  // Leave the response called error to reuse WRAP.
  CRYPTOHOME_WRAP_METHOD(ListKeysEx, identifier, authorization, request);
}
gboolean cryptohome_migrate_key(Cryptohome* self,
                                gchar* userid,
                                gchar* from_key,
                                gchar* to_key,
                                gboolean* OUT_result,
                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(MigrateKey, userid, from_key, to_key, OUT_result);
}
gboolean cryptohome_async_migrate_key(Cryptohome* self,
                                      gchar* userid,
                                      gchar* from_key,
                                      gchar* to_key,
                                      gint* OUT_async_id,
                                      GError** error) {
  CRYPTOHOME_WRAP_METHOD(AsyncMigrateKey, userid, from_key, to_key,
                         OUT_async_id);
}
gboolean cryptohome_add_key(Cryptohome* self,
                            gchar* userid,
                            gchar* key,
                            gchar* new_key,
                            gint* OUT_key_id,
                            gboolean* OUT_result,
                            GError** error) {
  CRYPTOHOME_WRAP_METHOD(AddKey, userid, key, new_key, OUT_key_id, OUT_result);
}
gboolean cryptohome_async_add_key(Cryptohome* self,
                                  gchar* userid,
                                  gchar* key,
                                  gchar* new_key,
                                  gint* OUT_async_id,
                                  GError** error) {
  CRYPTOHOME_WRAP_METHOD(AsyncAddKey, userid, key, new_key, OUT_async_id);
}
gboolean cryptohome_add_key_ex(Cryptohome* self,
                               GArray* id,
                               GArray* auth,
                               GArray* params,
                               DBusGMethodInvocation* error) {
  // Leave the response called error to reuse WRAP.
  CRYPTOHOME_WRAP_METHOD(AddKeyEx, id, auth, params);
}
gboolean cryptohome_update_key_ex(Cryptohome* self,
                                  GArray* id,
                                  GArray* auth,
                                  GArray* params,
                                  DBusGMethodInvocation* error) {
  // Leave the response called error to reuse WRAP.
  CRYPTOHOME_WRAP_METHOD(UpdateKeyEx, id, auth, params);
}
gboolean cryptohome_remove(Cryptohome* self,
                           gchar* userid,
                           gboolean* OUT_result,
                           GError** error) {
  CRYPTOHOME_WRAP_METHOD(Remove, userid, OUT_result);
}
gboolean cryptohome_async_remove(Cryptohome* self,
                                 gchar* userid,
                                 gint* OUT_async_id,
                                 GError** error) {
  CRYPTOHOME_WRAP_METHOD(AsyncRemove, userid, OUT_async_id);
}
gboolean cryptohome_rename_cryptohome(Cryptohome* self,
                                      GArray* account_id_from,
                                      GArray* account_id_to,
                                      DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(RenameCryptohome, account_id_from, account_id_to);
}
gboolean cryptohome_get_account_disk_usage(Cryptohome* self,
                                           GArray* account_id,
                                           DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(GetAccountDiskUsage, account_id);
}
gboolean cryptohome_get_system_salt(Cryptohome* self,
                                    GArray** OUT_salt,
                                    GError** error) {
  CRYPTOHOME_WRAP_METHOD(GetSystemSalt, OUT_salt);
}
gboolean cryptohome_get_sanitized_username(Cryptohome* self,
                                           gchar* username,
                                           gchar** OUT_sanitized,
                                           GError** error) {
  CRYPTOHOME_WRAP_METHOD(GetSanitizedUsername, username, OUT_sanitized);
}
gboolean cryptohome_is_mounted(Cryptohome* self,
                               gboolean* OUT_is_mounted,
                               GError** error) {
  CRYPTOHOME_WRAP_METHOD(IsMounted, OUT_is_mounted);
}
gboolean cryptohome_is_mounted_for_user(Cryptohome* self,
                                        gchar* userid,
                                        gboolean* OUT_is_mounted,
                                        gboolean* OUT_is_ephemeral_mount,
                                        GError** error) {
  CRYPTOHOME_WRAP_METHOD(IsMountedForUser, userid,
                         OUT_is_mounted, OUT_is_ephemeral_mount);
}
gboolean cryptohome_mount(Cryptohome* self,
                          gchar* userid,
                          gchar* key,
                          gboolean create_if_missing,
                          gboolean ensure_ephemeral,
                          gchar** tracked_directories,
                          gint* OUT_error_code,
                          gboolean* OUT_result,
                          GError** error) {
  CRYPTOHOME_WRAP_METHOD(Mount, userid, key, create_if_missing,
                         ensure_ephemeral, OUT_error_code, OUT_result);
}
gboolean cryptohome_async_mount(Cryptohome* self,
                                gchar* userid,
                                gchar* key,
                                gboolean create_if_missing,
                                gboolean ensure_ephemeral,
                                gchar** tracked_directories,
                                DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(AsyncMount, userid, key, create_if_missing,
                         ensure_ephemeral);
}
gboolean cryptohome_mount_ex(Cryptohome* self,
                             GArray* id,
                             GArray* auth,
                             GArray* params,
                             DBusGMethodInvocation* error) {
  // Leave the response called error to reuse WRAP.
  CRYPTOHOME_WRAP_METHOD(MountEx, id, auth, params);
}
gboolean cryptohome_mount_guest(Cryptohome* self,
                                gint* OUT_error_code,
                                gboolean* OUT_result,
                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(MountGuest, OUT_error_code, OUT_result);
}
gboolean cryptohome_async_mount_guest(Cryptohome* self,
                                      gint* OUT_async_id,
                                      GError** error) {
  CRYPTOHOME_WRAP_METHOD(AsyncMountGuest, OUT_async_id);
}
gboolean cryptohome_mount_public(Cryptohome* self,
                                 gchar* public_mount_id,
                                 gboolean create_if_missing,
                                 gboolean ensure_ephemeral,
                                 gint* OUT_error_code,
                                 gboolean* OUT_result,
                                 GError** error) {
  CRYPTOHOME_WRAP_METHOD(MountPublic, public_mount_id, create_if_missing,
                         ensure_ephemeral, OUT_error_code, OUT_result);
}
gboolean cryptohome_async_mount_public(Cryptohome* self,
                                       gchar* public_mount_id,
                                       gboolean create_if_missing,
                                       gboolean ensure_ephemeral,
                                       DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(AsyncMountPublic, public_mount_id, create_if_missing,
                         ensure_ephemeral);
}
gboolean cryptohome_unmount(Cryptohome* self,
                            gboolean* OUT_result,
                            GError** error) {
  CRYPTOHOME_WRAP_METHOD(Unmount, OUT_result);
}
gboolean cryptohome_unmount_for_user(Cryptohome* self,
                                     gchar* userid,
                                     gboolean* OUT_result,
                                     GError** error) {
  CRYPTOHOME_WRAP_METHOD(UnmountForUser, userid, OUT_result);
}
gboolean cryptohome_do_automatic_free_disk_space_control(Cryptohome* self,
                                                         gboolean* OUT_result,
                                                         GError** error) {
  CRYPTOHOME_WRAP_METHOD(DoAutomaticFreeDiskSpaceControl, OUT_result);
}
gboolean cryptohome_async_do_automatic_free_disk_space_control(
    Cryptohome* self,
    gint* OUT_async_id,
    GError** error) {
  CRYPTOHOME_WRAP_METHOD(AsyncDoAutomaticFreeDiskSpaceControl, OUT_async_id);
}
gboolean cryptohome_update_current_user_activity_timestamp(
    Cryptohome* self,
    gint time_shift_sec,
    GError** error) {
  CRYPTOHOME_WRAP_METHOD(UpdateCurrentUserActivityTimestamp,
                         time_shift_sec);
}
gboolean cryptohome_tpm_is_ready(Cryptohome* self,
                                 gboolean* OUT_ready,
                                 GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmIsReady, OUT_ready);
}
gboolean cryptohome_tpm_is_enabled(Cryptohome* self,
                                   gboolean* OUT_enabled,
                                   GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmIsEnabled, OUT_enabled);
}
gboolean cryptohome_tpm_get_password(Cryptohome* self,
                                     gchar** OUT_password,
                                     GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmGetPassword, OUT_password);
}
gboolean cryptohome_tpm_is_owned(Cryptohome* self,
                                 gboolean* OUT_owned,
                                 GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmIsOwned, OUT_owned);
}
gboolean cryptohome_tpm_is_being_owned(Cryptohome* self,
                                       gboolean* OUT_owning,
                                       GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmIsBeingOwned, OUT_owning);
}
gboolean cryptohome_tpm_can_attempt_ownership(Cryptohome* self,
                                              GError** error) {
  CRYPTOHOME_WRAP_METHOD_NO_ARGS(TpmCanAttemptOwnership);
}
gboolean cryptohome_tpm_clear_stored_password(Cryptohome* self,
                                              GError** error) {
  CRYPTOHOME_WRAP_METHOD_NO_ARGS(TpmClearStoredPassword);
}
gboolean cryptohome_tpm_is_attestation_prepared(Cryptohome* self,
                                                gboolean* OUT_prepared,
                                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmIsAttestationPrepared, OUT_prepared);
}
gboolean cryptohome_tpm_verify_attestation_data(Cryptohome* self,
                                                gboolean is_cros_core,
                                                gboolean* OUT_verified,
                                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmVerifyAttestationData, is_cros_core, OUT_verified);
}
gboolean cryptohome_tpm_verify_ek(Cryptohome* self,
                                  gboolean is_cros_core,
                                  gboolean* OUT_verified,
                                  GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmVerifyEK, is_cros_core, OUT_verified);
}
gboolean cryptohome_tpm_attestation_create_enroll_request(
    Cryptohome* self,
    gint pca_type,
    GArray** OUT_pca_request,
    GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationCreateEnrollRequest, pca_type,
                         OUT_pca_request);
}
gboolean cryptohome_async_tpm_attestation_create_enroll_request(
    Cryptohome* self,
    gint pca_type,
    gint* OUT_async_id,
    GError** error) {
  CRYPTOHOME_WRAP_METHOD(AsyncTpmAttestationCreateEnrollRequest, pca_type,
                         OUT_async_id);
}
gboolean cryptohome_tpm_attestation_enroll(Cryptohome* self,
                                           gint pca_type,
                                           GArray* pca_response,
                                           gboolean* OUT_success,
                                           GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationEnroll, pca_type, pca_response,
                         OUT_success);
}
gboolean cryptohome_async_tpm_attestation_enroll(Cryptohome* self,
                                                 gint pca_type,
                                                 GArray* pca_response,
                                                 gint* OUT_async_id,
                                                 GError** error) {
  CRYPTOHOME_WRAP_METHOD(AsyncTpmAttestationEnroll, pca_type, pca_response,
                         OUT_async_id);
}
gboolean cryptohome_tpm_attestation_create_cert_request(
    Cryptohome* self,
    gint pca_type,
    gint certificate_profile,
    gchar* username,
    gchar* request_origin,
    GArray** OUT_pca_request,
    GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationCreateCertRequest,
                         pca_type,
                         certificate_profile,
                         username,
                         request_origin,
                         OUT_pca_request);
}
gboolean cryptohome_async_tpm_attestation_create_cert_request(
    Cryptohome* self,
    gint pca_type,
    gint certificate_profile,
    gchar* username,
    gchar* request_origin,
    gint* OUT_async_id,
    GError** error) {
  CRYPTOHOME_WRAP_METHOD(AsyncTpmAttestationCreateCertRequest,
                         pca_type,
                         certificate_profile,
                         username,
                         request_origin,
                         OUT_async_id);
}
gboolean cryptohome_tpm_attestation_finish_cert_request(
    Cryptohome* self,
    GArray* pca_response,
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    GArray** OUT_cert,
    gboolean* OUT_success,
    GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationFinishCertRequest,
                         pca_response,
                         is_user_specific,
                         username,
                         key_name,
                         OUT_cert,
                         OUT_success);
}
gboolean cryptohome_async_tpm_attestation_finish_cert_request(
    Cryptohome* self,
    GArray* pca_response,
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    gint* OUT_async_id,
    GError** error) {
  CRYPTOHOME_WRAP_METHOD(AsyncTpmAttestationFinishCertRequest,
                         pca_response,
                         is_user_specific,
                         username,
                         key_name,
                         OUT_async_id);
}
gboolean cryptohome_tpm_is_attestation_enrolled(Cryptohome* self,
                                                gboolean* OUT_is_enrolled,
                                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmIsAttestationEnrolled, OUT_is_enrolled);
}
gboolean cryptohome_tpm_attestation_does_key_exist(Cryptohome* self,
                                                   gboolean is_user_specific,
                                                   gchar* username,
                                                   gchar* key_name,
                                                   gboolean* OUT_exists,
                                                   GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationDoesKeyExist,
                         is_user_specific,
                         username,
                         key_name,
                         OUT_exists);
}
gboolean cryptohome_tpm_attestation_get_certificate(Cryptohome* self,
                                                    gboolean is_user_specific,
                                                    gchar* username,
                                                    gchar* key_name,
                                                    GArray** OUT_certificate,
                                                    gboolean* OUT_success,
                                                    GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationGetCertificate,
                         is_user_specific,
                         username,
                         key_name,
                         OUT_certificate,
                         OUT_success);
}
gboolean cryptohome_tpm_attestation_get_public_key(Cryptohome* self,
                                                   gboolean is_user_specific,
                                                   gchar* username,
                                                   gchar* key_name,
                                                   GArray** OUT_public_key,
                                                   gboolean* OUT_success,
                                                   GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationGetPublicKey,
                         is_user_specific,
                         username,
                         key_name,
                         OUT_public_key,
                         OUT_success);
}
gboolean cryptohome_tpm_attestation_register_key(Cryptohome* self,
                                                 gboolean is_user_specific,
                                                 gchar* username,
                                                 gchar* key_name,
                                                 gint* OUT_async_id,
                                                 GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationRegisterKey,
                         is_user_specific,
                         username,
                         key_name,
                         OUT_async_id);
}
gboolean cryptohome_tpm_attestation_sign_enterprise_challenge(
    Cryptohome* self,
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    gchar* domain,
    GArray* device_id,
    gboolean include_signed_public_key,
    GArray* challenge,
    gint* OUT_async_id,
    GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationSignEnterpriseChallenge,
                         is_user_specific,
                         username,
                         key_name,
                         domain,
                         device_id,
                         include_signed_public_key,
                         challenge,
                         OUT_async_id);
}
gboolean cryptohome_tpm_attestation_sign_simple_challenge(
    Cryptohome* self,
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    GArray* challenge,
    gint* OUT_async_id,
    GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationSignSimpleChallenge,
                         is_user_specific,
                         username,
                         key_name,
                         challenge,
                         OUT_async_id);
}
gboolean cryptohome_tpm_attestation_get_key_payload(Cryptohome* self,
                                                    gboolean is_user_specific,
                                                    gchar* username,
                                                    gchar* key_name,
                                                    GArray** OUT_payload,
                                                    gboolean* OUT_success,
                                                    GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationGetKeyPayload,
                         is_user_specific,
                         username,
                         key_name,
                         OUT_payload,
                         OUT_success);
}
gboolean cryptohome_tpm_attestation_set_key_payload(Cryptohome* self,
                                                    gboolean is_user_specific,
                                                    gchar* username,
                                                    gchar* key_name,
                                                    GArray* payload,
                                                    gboolean* OUT_success,
                                                    GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationSetKeyPayload,
                         is_user_specific,
                         username,
                         key_name,
                         payload,
                         OUT_success);
}
gboolean cryptohome_tpm_attestation_delete_keys(Cryptohome* self,
                                                gboolean is_user_specific,
                                                gchar* username,
                                                gchar* key_prefix,
                                                gboolean* OUT_success,
                                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationDeleteKeys,
                         is_user_specific,
                         username,
                         key_prefix,
                         OUT_success);
}
gboolean cryptohome_tpm_attestation_get_ek(Cryptohome* self,
                                           gchar** OUT_ek_info,
                                           gboolean* OUT_success,
                                           GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationGetEK, OUT_ek_info, OUT_success);
}
gboolean cryptohome_tpm_attestation_reset_identity(Cryptohome* self,
                                                   gchar* reset_token,
                                                   GArray** OUT_reset_request,
                                                   gboolean* OUT_success,
                                                   GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmAttestationResetIdentity,
                         reset_token,
                         OUT_reset_request,
                         OUT_success);
}
gboolean cryptohome_tpm_get_version(Cryptohome* self,
                                    gchar** OUT_result,
                                    GError** error) {
  CRYPTOHOME_WRAP_METHOD(TpmGetVersion,
                         OUT_result);
}
gboolean cryptohome_pkcs11_get_tpm_token_info(Cryptohome* self,
                                              gchar** OUT_label,
                                              gchar** OUT_user_pin,
                                              gint* OUT_slot,
                                              GError** error) {
  CRYPTOHOME_WRAP_METHOD(Pkcs11GetTpmTokenInfo,
                         OUT_label,
                         OUT_user_pin,
                         OUT_slot);
}
gboolean cryptohome_pkcs11_get_tpm_token_info_for_user(Cryptohome* self,
                                                       gchar* username,
                                                       gchar** OUT_label,
                                                       gchar** OUT_user_pin,
                                                       gint* OUT_slot,
                                                       GError** error) {
  CRYPTOHOME_WRAP_METHOD(Pkcs11GetTpmTokenInfoForUser,
                         username,
                         OUT_label,
                         OUT_user_pin,
                         OUT_slot);
}
gboolean cryptohome_pkcs11_is_tpm_token_ready(Cryptohome* self,
                                    gboolean* OUT_ready,
                                    GError** error) {
  CRYPTOHOME_WRAP_METHOD(Pkcs11IsTpmTokenReady, OUT_ready);
}
gboolean cryptohome_pkcs11_is_tpm_token_ready_for_user(Cryptohome* self,
                                    gchar* username,
                                    gboolean* OUT_ready,
                                    GError** error) {
  CRYPTOHOME_WRAP_METHOD(Pkcs11IsTpmTokenReadyForUser, username, OUT_ready);
}
gboolean cryptohome_pkcs11_terminate(Cryptohome* self,
                                     gchar* username,
                                     GError** error) {
  CRYPTOHOME_WRAP_METHOD(Pkcs11Terminate, username);
}
gboolean cryptohome_get_status_string(Cryptohome* self,
                                      gchar** OUT_status,
                                      GError** error) {
  CRYPTOHOME_WRAP_METHOD(GetStatusString, OUT_status);
}

gboolean cryptohome_install_attributes_get(Cryptohome* self,
                                gchar* name,
                                GArray** OUT_value,
                                gboolean* OUT_successful,
                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesGet, name, OUT_value, OUT_successful);
}

gboolean cryptohome_install_attributes_set(Cryptohome* self,
                                           gchar* name,
                                           GArray* value,
                                           gboolean* OUT_successful,
                                           GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesSet, name, value, OUT_successful);
}

gboolean cryptohome_install_attributes_finalize(Cryptohome* self,
                                                gboolean* OUT_successful,
                                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesFinalize, OUT_successful);
}

gboolean cryptohome_install_attributes_count(Cryptohome* self,
                                             gint* OUT_count,
                                             GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesCount, OUT_count);
}

gboolean cryptohome_install_attributes_is_ready(Cryptohome* self,
                                                gboolean* OUT_is_ready,
                                                GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesIsReady, OUT_is_ready);
}

gboolean cryptohome_install_attributes_is_secure(Cryptohome* self,
                                                 gboolean* OUT_is_secure,
                                                 GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesIsSecure, OUT_is_secure);
}

gboolean cryptohome_install_attributes_is_invalid(Cryptohome* self,
                                                  gboolean* OUT_is_invalid,
                                                  GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesIsInvalid, OUT_is_invalid);
}

gboolean cryptohome_install_attributes_is_first_install(
  Cryptohome* self,
  gboolean* OUT_is_first_install,
  GError** error) {
  CRYPTOHOME_WRAP_METHOD(InstallAttributesIsFirstInstall, OUT_is_first_install);
}

gboolean cryptohome_store_enrollment_state(
  Cryptohome* self,
  GArray* enrollment_state,
  gboolean* OUT_success,
  GError** error) {
  CRYPTOHOME_WRAP_METHOD(StoreEnrollmentState, enrollment_state, OUT_success);
}

gboolean cryptohome_load_enrollment_state(
  Cryptohome* self,
  GArray** OUT_enrollment_state,
  gboolean* OUT_success,
  GError** error) {
  CRYPTOHOME_WRAP_METHOD(LoadEnrollmentState, OUT_enrollment_state,
                         OUT_success);
}

gboolean cryptohome_sign_boot_lockbox(Cryptohome* self,
                                      GArray* request,
                                      DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(SignBootLockbox, request);
}

gboolean cryptohome_verify_boot_lockbox(Cryptohome* self,
                                        GArray* request,
                                        DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(VerifyBootLockbox, request);
}

gboolean cryptohome_finalize_boot_lockbox(Cryptohome* self,
                                          GArray* request,
                                          DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(FinalizeBootLockbox, request);
}

gboolean cryptohome_get_boot_attribute(Cryptohome* self,
                                       GArray* request,
                                       DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(GetBootAttribute, request);
}

gboolean cryptohome_set_boot_attribute(Cryptohome* self,
                                       GArray* request,
                                       DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(SetBootAttribute, request);
}

gboolean cryptohome_flush_and_sign_boot_attributes(
    Cryptohome* self,
    GArray* request,
    DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(FlushAndSignBootAttributes, request);
}

gboolean cryptohome_get_login_status(Cryptohome* self,
                                     GArray* request,
                                     DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(GetLoginStatus, request);
}

gboolean cryptohome_get_tpm_status(Cryptohome* self,
                                   GArray* request,
                                   DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(GetTpmStatus, request);
}

gboolean cryptohome_get_endorsement_info(Cryptohome* self,
                                         GArray* request,
                                         DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(GetEndorsementInfo, request);
}

gboolean cryptohome_initialize_cast_key(Cryptohome* self,
                                        GArray* request,
                                        DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(InitializeCastKey, request);
}

gboolean cryptohome_get_firmware_management_parameters(
    Cryptohome* self,
    GArray* request,
    DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(GetFirmwareManagementParameters, request);
}

gboolean cryptohome_set_firmware_management_parameters(
    Cryptohome* self,
    GArray* request,
    DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(SetFirmwareManagementParameters, request);
}

gboolean cryptohome_remove_firmware_management_parameters(
    Cryptohome* self,
    GArray* request,
    DBusGMethodInvocation* error) {
  CRYPTOHOME_WRAP_METHOD(RemoveFirmwareManagementParameters, request);
}

gboolean cryptohome_migrate_to_dircrypto(Cryptohome* self,
                                         GArray* id,
                                         GError** error) {
  CRYPTOHOME_WRAP_METHOD(MigrateToDircrypto, id);
}

gboolean cryptohome_needs_dircrypto_migration(Cryptohome* self,
                                              GArray* identifier,
                                              gboolean* OUT_needs_migration,
                                              GError** error) {
  CRYPTOHOME_WRAP_METHOD(
      NeedsDircryptoMigration, identifier, OUT_needs_migration);
}

#undef CRYPTOHOME_WRAP_METHOD

}  // namespace gobject
}  // namespace cryptohome
