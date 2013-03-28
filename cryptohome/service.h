// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_SERVICE_H_
#define CRYPTOHOME_SERVICE_H_

#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/threading/thread.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>
#include <chromeos/secure_blob.h>
#include <dbus/dbus-glib.h>
#include <glib-object.h>
#include <metrics/metrics_library.h>
#include <metrics/timer.h>

#include "cryptohome_event_source.h"
#include "install_attributes.h"
#include "mount.h"
#include "mount_factory.h"
#include "mount_task.h"
#include "pkcs11_init.h"
#include "tpm_init.h"

namespace cryptohome {
namespace gobject {

struct Cryptohome;
}  // namespace gobject

// Wrapper for all timers used by the cryptohome daemon.
class TimerCollection;


// Service
// Provides a wrapper for exporting CryptohomeInterface to
// D-Bus and entering the glib run loop.
//
// ::g_type_init() must be called before this class is used.
class Service : public chromeos::dbus::AbstractDbusService,
                public CryptohomeEventSourceSink,
                public TpmInit::TpmInitCallback {
 public:
  Service();
  virtual ~Service();

  // From chromeos::dbus::AbstractDbusService
  // Setup the wrapped GObject and the GMainLoop
  virtual bool Initialize();
  virtual bool SeedUrandom();
  virtual void InitializeInstallAttributes(bool first_time);
  virtual void InitializePkcs11(cryptohome::Mount* mount);
  virtual bool Reset();

  // Used internally during registration to set the
  // proper service information.
  virtual const char *service_name() const {
    return kCryptohomeServiceName;
  }
  virtual const char *service_path() const {
    return kCryptohomeServicePath;
  }
  virtual const char *service_interface() const {
    return kCryptohomeInterface;
  }
  virtual GObject* service_object() const {
    return G_OBJECT(cryptohome_);
  }
  virtual void set_tpm(Tpm* tpm) {
    tpm_ = tpm;
  }
  virtual void set_tpm_init(TpmInit* tpm_init) {
    tpm_init_ = tpm_init;
  }
  virtual void set_initialize_tpm(bool value) {
    initialize_tpm_ = value;
  }
  virtual void set_auto_cleanup_period(int value) {
    auto_cleanup_period_ = value;
  }
  virtual void set_install_attrs(InstallAttributes* install_attrs) {
    install_attrs_ = install_attrs;
  }
  virtual void set_update_user_activity_period(int value) {
    update_user_activity_period_ = value;
  }
  virtual void set_mount_for_user(const std::string& username,
                                  cryptohome::Mount* m) {
    mounts_[username] = m;
  }
  virtual void set_mount_factory(cryptohome::MountFactory* mf) {
    mount_factory_ = mf;
  }
  virtual void set_use_tpm(bool value) {
    use_tpm_ = value;
  }

  // Overrides the Platform implementation for Service.
  virtual void set_platform(cryptohome::Platform *platform) {
    platform_ = platform;
  }

  virtual cryptohome::Crypto* crypto() { return crypto_; }

  virtual void set_homedirs(cryptohome::HomeDirs* value) { homedirs_ = value; }

  virtual cryptohome::HomeDirs* homedirs() { return homedirs_; }

  // CryptohomeEventSourceSink
  virtual void NotifyEvent(CryptohomeEventBase* event);

  // TpmInitCallback
  virtual void InitializeTpmComplete(bool status, bool took_ownership);

  // Called during initialization (and on mount events) to ensure old mounts
  // are marked for unmount when possible by the kernel.  Returns true if any
  // mounts were stale and not cleaned up (because of open files).
  //
  // Parameters
  // - force: if true, unmounts all existing shadow mounts.
  //          if false, unmounts shadows mounts with no open files.
  virtual bool CleanUpStaleMounts(bool force);

  // Service implementation functions as wrapped in interface.cc
  // and defined in cryptohome.xml.
  virtual gboolean CheckKey(gchar *user,
                            gchar *key,
                            gboolean *OUT_result,
                            GError **error);
  virtual gboolean AsyncCheckKey(gchar *user,
                                 gchar *key,
                                 gint *OUT_async_id,
                                 GError **error);
  virtual gboolean MigrateKey(gchar *user,
                              gchar *from_key,
                              gchar *to_key,
                              gboolean *OUT_result,
                              GError **error);
  virtual gboolean AsyncMigrateKey(gchar *user,
                                   gchar *from_key,
                                   gchar *to_key,
                                   gint *OUT_async_id,
                                   GError **error);
  virtual gboolean Remove(gchar *user,
                          gboolean *OUT_result,
                          GError **error);
  virtual gboolean AsyncRemove(gchar *user,
                               gint *OUT_async_id,
                               GError **error);
  virtual gboolean GetSystemSalt(GArray **OUT_salt, GError **error);
  virtual gboolean GetSanitizedUsername(gchar *username,
                                        gchar **OUT_sanitized,
                                        GError **error);
  virtual gboolean IsMountedForUser(gchar *user,
                                    gboolean *OUT_is_mounted,
                                    gboolean *OUT_is_ephemeral_mount,
                                    GError **error);
  virtual gboolean IsMounted(gboolean *OUT_is_mounted, GError **error);
  virtual gboolean Mount(const gchar *user,
                         const gchar *key,
                         gboolean create_if_missing,
                         gboolean ensure_ephemeral,
                         gint *OUT_error_code,
                         gboolean *OUT_result,
                         GError **error);
  virtual gboolean AsyncMount(
      const gchar *user,
      const gchar *key,
      gboolean create_if_missing,
      gboolean ensure_ephemeral,
      gint *OUT_async_id,
      GError **error);
  virtual gboolean MountGuest(gint *OUT_error_code,
                              gboolean *OUT_result,
                              GError **error);
  virtual gboolean AsyncMountGuest(gint *OUT_async_id,
                                   GError **error);
  virtual gboolean Unmount(gboolean *OUT_result, GError **error);
  virtual gboolean UnmountForUser(gchar* userid, gboolean *OUT_result,
                                  GError **error);
  virtual gboolean DoAutomaticFreeDiskSpaceControl(gboolean *OUT_result,
                                                   GError **error);
  virtual gboolean AsyncDoAutomaticFreeDiskSpaceControl(gint *OUT_async_id,
                                                        GError **error);
  virtual gboolean UpdateCurrentUserActivityTimestamp(gint time_shift_sec,
                                                      GError **error);

  virtual gboolean TpmIsReady(gboolean* OUT_ready, GError** error);
  virtual gboolean TpmIsEnabled(gboolean* OUT_enabled, GError** error);
  virtual gboolean TpmGetPassword(gchar** OUT_password, GError** error);
  virtual gboolean TpmIsOwned(gboolean* OUT_owned, GError** error);
  virtual gboolean TpmIsBeingOwned(gboolean* OUT_owning, GError** error);
  virtual gboolean TpmCanAttemptOwnership(GError** error);
  virtual gboolean TpmClearStoredPassword(GError** error);
  virtual gboolean TpmIsAttestationPrepared(gboolean* OUT_prepared,
                                            GError** error);
  virtual gboolean TpmVerifyAttestationData(gboolean* OUT_verified,
                                            GError** error);
  virtual gboolean TpmVerifyEK(gboolean* OUT_verified, GError** error);
  virtual gboolean TpmAttestationCreateEnrollRequest(GArray** OUT_pca_request,
                                                     GError** error);
  virtual gboolean AsyncTpmAttestationCreateEnrollRequest(
      gint* OUT_async_id,
      GError** error);
  virtual gboolean TpmAttestationEnroll(GArray* pca_response,
                                        gboolean* OUT_success,
                                        GError** error);
  virtual gboolean AsyncTpmAttestationEnroll(GArray* pca_response,
                                             gint* OUT_async_id,
                                             GError** error);
  virtual gboolean TpmAttestationCreateCertRequest(
      gboolean include_stable_id,
      gboolean include_device_state,
      GArray** OUT_pca_request,
      GError** error);
  virtual gboolean AsyncTpmAttestationCreateCertRequest(
      gboolean include_stable_id,
      gboolean include_device_state,
      gint* OUT_async_id,
      GError** error);
  virtual gboolean TpmAttestationFinishCertRequest(GArray* pca_response,
                                                   gboolean is_user_specific,
                                                   gchar* key_name,
                                                   GArray** OUT_cert,
                                                   gboolean* OUT_success,
                                                   GError** error);
  virtual gboolean AsyncTpmAttestationFinishCertRequest(
      GArray* pca_response,
      gboolean is_user_specific,
      gchar* key_name,
      gint* OUT_async_id,
      GError** error);
  virtual gboolean TpmIsAttestationEnrolled(gboolean* OUT_is_enrolled,
                                            GError** error);
  virtual gboolean TpmAttestationDoesKeyExist(gboolean is_user_specific,
                                              gchar* key_name,
                                              gboolean *OUT_exists,
                                              GError** error);
  virtual gboolean TpmAttestationGetCertificate(gboolean is_user_specific,
                                                gchar* key_name,
                                                GArray **OUT_certificate,
                                                gboolean* OUT_success,
                                                GError** error);
  virtual gboolean TpmAttestationGetPublicKey(gboolean is_user_specific,
                                              gchar* key_name,
                                              GArray **OUT_public_key,
                                              gboolean* OUT_success,
                                              GError** error);
  virtual gboolean TpmAttestationRegisterKey(gboolean is_user_specific,
                                             gchar* key_name,
                                             gint *OUT_async_id,
                                             GError** error);
  virtual gboolean TpmAttestationSignEnterpriseChallenge(
      gboolean is_user_specific,
      gchar* key_name,
      gchar* domain,
      GArray* device_id,
      GArray* challenge,
      gint *OUT_async_id,
      GError** error);
  virtual gboolean TpmAttestationSignSimpleChallenge(
      gboolean is_user_specific,
      gchar* key_name,
      GArray* challenge,
      gint *OUT_async_id,
      GError** error);

  // Returns the label of the TPM token along with its user PIN.
  virtual gboolean Pkcs11GetTpmTokenInfo(gchar** OUT_label,
                                         gchar** OUT_user_pin,
                                         GError** error);
  // Returns the label of the TPM token along with its user PIN.
  virtual gboolean Pkcs11GetTpmTokenInfoForUser(gchar *username,
                                                gchar** OUT_label,
                                                gchar** OUT_user_pin,
                                                GError** error);

  // Returns in |OUT_ready| whether the TPM token is ready for use.
  virtual gboolean Pkcs11IsTpmTokenReady(gboolean* OUT_ready, GError** error);
  virtual gboolean Pkcs11IsTpmTokenReadyForUser(gchar *username,
                                                gboolean* OUT_ready,
                                                GError** error);
  virtual gboolean Pkcs11Terminate(gchar* username, GError** error);
  virtual gboolean GetStatusString(gchar** OUT_status, GError** error);

  // InstallAttributes methods
  virtual gboolean InstallAttributesGet(gchar* name,
                              GArray** OUT_value,
                              gboolean* OUT_successful,
                              GError** error);
  virtual gboolean InstallAttributesSet(gchar* name,
                              GArray* value,
                              gboolean* OUT_successful,
                              GError** error);
  virtual gboolean InstallAttributesFinalize(gboolean* OUT_finalized,
                                             GError** error);
  virtual gboolean InstallAttributesCount(gint* OUT_count, GError** error);
  virtual gboolean InstallAttributesIsReady(gboolean* OUT_ready,
                                            GError** error);
  virtual gboolean InstallAttributesIsSecure(gboolean* OUT_secure,
                                             GError** error);
  virtual gboolean InstallAttributesIsInvalid(gboolean* OUT_invalid,
                                              GError** error);
  virtual gboolean InstallAttributesIsFirstInstall(gboolean* OUT_first_install,
                                                   GError** error);

 protected:
  virtual GMainLoop *main_loop() { return loop_; }

  // Called periodically on Mount thread to initiate automatic disk
  // cleanup if needed.
  virtual void AutoCleanupCallback();
  // Returns true if there are any existing mounts and populates
  // |mounts| with the mount point.
  virtual bool GetExistingMounts(
                   std::multimap<const std::string, const std::string>* mounts);

  // Checks if the machine is enterprise owned and report to mount_ then.
  virtual void DetectEnterpriseOwnership();

  // Runs the event loop once. Only for testing.
  virtual void DispatchEvents();

  virtual scoped_refptr<cryptohome::Mount> GetMountForUser(
      const std::string& username);

  // Ensures only one Mount is ever created per username.
  virtual scoped_refptr<cryptohome::Mount> GetOrCreateMountForUser(
      const std::string& username);

  // Safely removes the MountMap reference for the given Mount.
  virtual bool RemoveMountForUser(const std::string& username);

  // Safelt removes the given Mount from MountMap.
  virtual void RemoveMount(cryptohome::Mount* mount);

  // Safely empties the MountMap and may request unmounting. If |unmount| is
  // true, the return value will reflect if all mounts unmounted cleanly or not.
  virtual bool RemoveAllMounts(bool unmount);

 private:
  bool CreateSystemSaltIfNeeded();
  bool use_tpm_;

  GMainLoop* loop_;
  // Can't use scoped_ptr for cryptohome_ because memory is allocated by glib.
  gobject::Cryptohome* cryptohome_;
  chromeos::SecureBlob system_salt_;
  scoped_ptr<cryptohome::Platform> default_platform_;
  cryptohome::Platform* platform_;
  cryptohome::Crypto* crypto_;
  // TPM doesn't use the scoped_ptr for default pattern, since the tpm is a
  // singleton - we don't want it getting destroyed when we are.
  Tpm* tpm_;
  scoped_ptr<TpmInit> default_tpm_init_;
  TpmInit* tpm_init_;
  scoped_ptr<Pkcs11Init> default_pkcs11_init_;
  Pkcs11Init* pkcs11_init_;
  bool initialize_tpm_;
  base::Thread mount_thread_;
  guint async_complete_signal_;
  // A completion signal for async calls that return data.
  guint async_data_complete_signal_;
  guint tpm_init_signal_;
  CryptohomeEventSource event_source_;
  int auto_cleanup_period_;
  scoped_ptr<cryptohome::InstallAttributes> default_install_attrs_;
  cryptohome::InstallAttributes* install_attrs_;
  int update_user_activity_period_;
  // Metrics library used by all metrics reporters in cryptohome.
  MetricsLibrary metrics_lib_;
  // Collection of timers for UMA reports.
  scoped_ptr<TimerCollection> timer_collection_;
  // Keeps track of whether a failure on PKCS#11 initialization was reported
  // during this user login. We use this not to report a same failure multiple
  // times.
  bool reported_pkcs11_init_fail_;
  // Keeps track of whether the device is enterprise-owned.
  bool enterprise_owned_;

  // Tracks Mount objects for each user by username.
  typedef std::map<const std::string,
                   scoped_refptr<cryptohome::Mount> > MountMap;
  MountMap mounts_;
  base::Lock mounts_lock_;  // Protects against parallel insertions only.
  scoped_ptr<cryptohome::MountFactory> default_mount_factory_;
  cryptohome::MountFactory* mount_factory_;

  typedef std::map<int,
                   scoped_refptr<MountTaskPkcs11Init> > Pkcs11TaskMap;
  Pkcs11TaskMap pkcs11_tasks_;

  scoped_ptr<HomeDirs> default_homedirs_;
  HomeDirs* homedirs_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_SERVICE_H_
