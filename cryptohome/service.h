// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_SERVICE_H_
#define CRYPTOHOME_SERVICE_H_

#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <base/thread.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>
#include <dbus/dbus-glib.h>
#include <glib-object.h>

#include "cryptohome_event_source.h"
#include "install_attributes.h"
#include "mount.h"
#include "mount_task.h"
#include "pkcs11_init.h"
#include "tpm_init.h"

namespace cryptohome {
namespace gobject {

struct Cryptohome;
}  // namespace gobject

// Service
// Provides a wrapper for exporting CryptohomeInterface to
// D-Bus and entering the glib run loop.
//
// ::g_type_init() must be called before this class is used.
class Service : public chromeos::dbus::AbstractDbusService,
                public MountTaskObserver,
                public CryptohomeEventSourceSink,
                public TpmInit::TpmInitCallback {
 public:
  Service(bool enable_pkcs11_init);
  virtual ~Service();

  // From chromeos::dbus::AbstractDbusService
  // Setup the wrapped GObject and the GMainLoop
  virtual bool Initialize();
  virtual bool SeedUrandom();
  virtual void InitializeInstallAttributes(bool first_time);
  virtual void InitializePkcs11();
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
  virtual void set_mount(Mount* mount) {
    mount_ = mount;
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

  // MountTaskObserver
  virtual void MountTaskObserve(const MountTaskResult& result);

  // CryptohomeEventSourceSink
  virtual void NotifyEvent(CryptohomeEventBase* event);

  // TpmInitCallback
  virtual void InitializeTpmComplete(bool status, bool took_ownership);

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
  virtual gboolean IsMounted(gboolean *OUT_is_mounted, GError **error);
  virtual gboolean Mount(gchar *user,
                         gchar *key,
                         gboolean create_if_missing,
                         gboolean deprecated_replace_tracked_subdirectories,
                         gchar** deprecated_tracked_subdirectories,
                         gint *OUT_error_code,
                         gboolean *OUT_result,
                         GError **error);
  virtual gboolean AsyncMount(
      gchar *user,
      gchar *key,
      gboolean create_if_missing,
      gboolean deprecated_replace_tracked_subdirectories,
      gchar** deprecated_tracked_subdirectories,
      gint *OUT_async_id,
      GError **error);
  virtual gboolean MountGuest(gint *OUT_error_code,
                              gboolean *OUT_result,
                              GError **error);
  virtual gboolean AsyncMountGuest(gint *OUT_async_id,
                                   GError **error);
  virtual gboolean Unmount(gboolean *OUT_result, GError **error);
  virtual gboolean RemoveTrackedSubdirectories(gboolean *OUT_result,
                                               GError **error);
  virtual gboolean AsyncRemoveTrackedSubdirectories(gint *OUT_async_id,
                                                    GError **error);
  virtual gboolean DoAutomaticFreeDiskSpaceControl(gboolean *OUT_result,
                                                   GError **error);
  virtual gboolean AsyncDoAutomaticFreeDiskSpaceControl(gint *OUT_async_id,
                                                        GError **error);

  virtual gboolean TpmIsReady(gboolean* OUT_ready, GError** error);
  virtual gboolean TpmIsEnabled(gboolean* OUT_enabled, GError** error);
  virtual gboolean TpmGetPassword(gchar** OUT_password, GError** error);
  virtual gboolean TpmIsOwned(gboolean* OUT_owned, GError** error);
  virtual gboolean TpmIsBeingOwned(gboolean* OUT_owning, GError** error);
  virtual gboolean TpmCanAttemptOwnership(GError** error);
  virtual gboolean TpmClearStoredPassword(GError** error);

  // Returns the label of the TPM token along with its user PIN.
  virtual gboolean Pkcs11GetTpmTokenInfo(gchar** OUT_label,
                                         gchar** OUT_user_pin,
                                         GError** error);

  // Returns in |OUT_ready| whether the TPM token is ready for use.
  virtual gboolean Pkcs11IsTpmTokenReady(gboolean* OUT_ready, GError** error);
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

 private:
  GMainLoop* loop_;
  // Can't use scoped_ptr for cryptohome_ because memory is allocated by glib.
  gobject::Cryptohome* cryptohome_;
  chromeos::Blob system_salt_;
  scoped_ptr<cryptohome::Mount> default_mount_;
  cryptohome::Mount* mount_;
  scoped_ptr<TpmInit> default_tpm_init_;
  TpmInit* tpm_init_;
  scoped_ptr<Pkcs11Init> default_pkcs11_init_;
  Pkcs11Init* pkcs11_init_;
  bool initialize_tpm_;
  base::Thread mount_thread_;
  guint async_complete_signal_;
  guint tpm_init_signal_;
  CryptohomeEventSource event_source_;
  int auto_cleanup_period_;
  scoped_ptr<cryptohome::InstallAttributes> default_install_attrs_;
  cryptohome::InstallAttributes* install_attrs_;
  int update_user_activity_period_;
  // Flag indicating if PKCS#11 initialization via cryptohomed is enabled.
  bool enable_pkcs11_init_;
  // Flag indicating if PKCS#11 is ready.
  typedef enum {
    kUninitialized = 0,  // PKCS#11 initialization hasn't been attempted.
    kIsWaitingOnTPM,  // PKCS#11 initialization is waiting on TPM ownership,
    kIsBeingInitialized,  // PKCS#11 is being attempted asynchronously.
    kIsInitialized,  // PKCS#11 was attempted and succeeded.
    kIsFailed,  // PKCS#11 was attempted and failed.
    kInvalidState,  // We should never be in this state.
  } Pkcs11State;
  // State of PKCS#11 initialization.
  Pkcs11State pkcs11_state_;
  // Sequence id of an asynchronous mount request that must trigger
  // a pkcs11 init request.
  int async_mount_pkcs11_init_sequence_id_;
  bool tpminit_must_pkcs11_init_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_SERVICE_H_
