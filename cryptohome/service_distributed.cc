// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/service_distributed.h"

namespace cryptohome {

ServiceDistributed::ServiceDistributed() {
}

ServiceDistributed::~ServiceDistributed() {
}

void ServiceDistributed::AttestationInitialize() {
}

void ServiceDistributed::AttestationInitializeTpm() {
}

void ServiceDistributed::AttestationInitializeTpmComplete() {
}

void ServiceDistributed::AttestationGetTpmStatus(GetTpmStatusReply* reply) {
}

bool ServiceDistributed::AttestationGetDelegateCredentials(
      brillo::SecureBlob* blob,
      brillo::SecureBlob* secret,
      bool* has_reset_lock_permissions) {
  return false;
}

gboolean ServiceDistributed::TpmIsAttestationPrepared(
    gboolean* OUT_prepared,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmVerifyAttestationData(
    gboolean is_cros_core,
    gboolean* OUT_verified,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmVerifyEK(
    gboolean is_cros_core,
    gboolean* OUT_verified,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationCreateEnrollRequest(
    gint pca_type,
    GArray** OUT_pca_request,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::AsyncTpmAttestationCreateEnrollRequest(
    gint pca_type,
    gint* OUT_async_id,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationEnroll(
    gint pca_type,
    GArray* pca_response,
    gboolean* OUT_success,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::AsyncTpmAttestationEnroll(
    gint pca_type,
    GArray* pca_response,
    gint* OUT_async_id,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationCreateCertRequest(
    gint pca_type,
    gint certificate_profile,
    gchar* username,
    gchar* request_origin,
    GArray** OUT_pca_request,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::AsyncTpmAttestationCreateCertRequest(
    gint pca_type,
    gint certificate_profile,
    gchar* username,
    gchar* request_origin,
    gint* OUT_async_id,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationFinishCertRequest(
    GArray* pca_response,
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    GArray** OUT_cert,
    gboolean* OUT_success,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::AsyncTpmAttestationFinishCertRequest(
    GArray* pca_response,
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    gint* OUT_async_id,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmIsAttestationEnrolled(
    gboolean* OUT_is_enrolled,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationDoesKeyExist(
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    gboolean *OUT_exists,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationGetCertificate(
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    GArray **OUT_certificate,
    gboolean* OUT_success,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationGetPublicKey(
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    GArray **OUT_public_key,
    gboolean* OUT_success,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationRegisterKey(
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    gint *OUT_async_id,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationSignEnterpriseChallenge(
      gboolean is_user_specific,
      gchar* username,
      gchar* key_name,
      gchar* domain,
      GArray* device_id,
      gboolean include_signed_public_key,
      GArray* challenge,
      gint *OUT_async_id,
      GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationSignSimpleChallenge(
      gboolean is_user_specific,
      gchar* username,
      gchar* key_name,
      GArray* challenge,
      gint *OUT_async_id,
      GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationGetKeyPayload(
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    GArray** OUT_payload,
    gboolean* OUT_success,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationSetKeyPayload(
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    GArray* payload,
    gboolean* OUT_success,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationDeleteKeys(
    gboolean is_user_specific,
    gchar* username,
    gchar* key_prefix,
    gboolean* OUT_success,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationGetEK(
    gchar** OUT_ek_info,
    gboolean* OUT_success,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::TpmAttestationResetIdentity(
    gchar* reset_token,
    GArray** OUT_reset_request,
    gboolean* OUT_success,
    GError** error) {
  return FALSE;
}

gboolean ServiceDistributed::GetEndorsementInfo(const GArray* request,
                                     DBusGMethodInvocation* context) {
  return FALSE;
}

gboolean ServiceDistributed::InitializeCastKey(const GArray* request,
                                    DBusGMethodInvocation* context) {
  return FALSE;
}

}  // namespace cryptohome
