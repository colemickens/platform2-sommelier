// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_SERVICE_H_
#define CRYPTOHOME_SERVICE_H_

#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>
#include <dbus/dbus-glib.h>
#include <glib-object.h>

#include "mount.h"

namespace cryptohome {
namespace gobject {

struct Cryptohome;
}  // namespace gobject

// Service
// Provides a wrapper for exporting CryptohomeInterface to
// D-Bus and entering the glib run loop.
//
// ::g_type_init() must be called before this class is used.
//
// TODO(wad) make an AbstractDbusService class which wraps a placeholder
//           GObject struct that references the service. Then subclass to
//           implement and push the abstract to common/chromeos/dbus/
class Service : public chromeos::dbus::AbstractDbusService {
 public:
  Service();
  virtual ~Service();

  // From chromeos::dbus::AbstractDbusService
  // Setup the wrapped GObject and the GMainLoop
  virtual bool Initialize();
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

  // Service implementation functions as wrapped in interface.cc
  // and defined in cryptohome.xml.
  virtual gboolean CheckKey(gchar *user,
                            gchar *key,
                            gboolean *OUT_success,
                            GError **error);
  virtual gboolean MigrateKey(gchar *user,
                              gchar *from_key,
                              gchar *to_key,
                              gboolean *OUT_success,
                              GError **error);
  virtual gboolean Remove(gchar *user,
                          gboolean *OUT_success,
                          GError **error);
  virtual gboolean GetSystemSalt(GArray **OUT_salt, GError **error);
  virtual gboolean IsMounted(gboolean *OUT_is_mounted, GError **error);
  virtual gboolean Mount(gchar *user,
                         gchar *key,
                         gint *OUT_error,
                         gboolean *OUT_done,
                         GError **error);
  virtual gboolean MountGuest(gint *OUT_error,
                              gboolean *OUT_done,
                              GError **error);
  virtual gboolean Unmount(gboolean *OUT_done, GError **error);

 protected:
  virtual GMainLoop *main_loop() { return loop_; }

 private:
  GMainLoop *loop_;
  // Can't use scoped_ptr for cryptohome_ because memory is allocated by glib.
  gobject::Cryptohome *cryptohome_;
  chromeos::Blob system_salt_;
  scoped_ptr<cryptohome::Mount> default_mount_;
  cryptohome::Mount* mount_;
  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_SERVICE_H_
