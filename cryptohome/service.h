// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
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
#include "mount.h"
#include "mount_task.h"
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
  Service();
  virtual ~Service();

  // From chromeos::dbus::AbstractDbusService
  // Setup the wrapped GObject and the GMainLoop
  virtual bool Initialize();
  virtual bool SeedUrandom();
  virtual bool Reset();

  // Used internally during registration to set the
  // proper service information.
  virtual const char *service_name() const
    { return kCryptohomeServiceName; }
  virtual const char *service_path() const
    { return kCryptohomeServicePath; }
  virtual const char *service_interface() const
    { return kCryptohomeInterface; }
  virtual GObject* service_object() const {
    return G_OBJECT(cryptohome_);
  }
  virtual void set_mount(Mount* mount)
    { mount_ = mount; }
  virtual void set_tpm_init(TpmInit* tpm_init)
    { tpm_init_ = tpm_init; }
  virtual void set_initialize_tpm(bool value)
    { initialize_tpm_ = value; }


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

  virtual gboolean TpmIsReady(gboolean* OUT_ready, GError** error);
  virtual gboolean TpmIsEnabled(gboolean* OUT_enabled, GError** error);
  virtual gboolean TpmGetPassword(gchar** OUT_password, GError** error);
  virtual gboolean TpmIsOwned(gboolean* OUT_owned, GError** error);
  virtual gboolean TpmIsBeingOwned(gboolean* OUT_owning, GError** error);
  virtual gboolean TpmCanAttemptOwnership(GError** error);
  virtual gboolean TpmClearStoredPassword(GError** error);
  virtual gboolean GetStatusString(gchar** OUT_status, GError** error);

 protected:
  virtual GMainLoop *main_loop() { return loop_; }

 private:
  GMainLoop *loop_;
  // Can't use scoped_ptr for cryptohome_ because memory is allocated by glib.
  gobject::Cryptohome *cryptohome_;
  chromeos::Blob system_salt_;
  scoped_ptr<cryptohome::Mount> default_mount_;
  cryptohome::Mount* mount_;
  scoped_ptr<TpmInit> default_tpm_init_;
  TpmInit *tpm_init_;
  bool initialize_tpm_;
  base::Thread mount_thread_;
  guint async_complete_signal_;
  guint tpm_init_signal_;
  CryptohomeEventSource event_source_;
  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_SERVICE_H_
