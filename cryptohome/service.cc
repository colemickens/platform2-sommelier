// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cryptohome/service.h"

#include <base/logging.h>
#include <chromeos/dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>

#include "cryptohome/interface.h"
#include "cryptohome/mount.h"
#include "cryptohome/secure_blob.h"
#include "cryptohome/username_passkey.h"

// Forcibly namespace the dbus-bindings generated server bindings instead of
// modifying the files afterward.
namespace cryptohome {  // NOLINT
namespace gobject {  // NOLINT
#include "cryptohome/bindings/server.h"
}  // namespace gobject
}  // namespace cryptohome

namespace cryptohome {

Service::Service() : loop_(NULL),
                     cryptohome_(NULL),
                     system_salt_(),
                     mount_(NULL) { }

Service::~Service() {
  if (loop_)
    g_main_loop_unref(loop_);
  if (cryptohome_)
    g_object_unref(cryptohome_);
  if (mount_)
    delete mount_;
}

bool Service::Initialize() {
  if(mount_ == NULL) {
    mount_ = new cryptohome::Mount();
    mount_->Init();
  }
  // Install the type-info for the service with dbus.
  dbus_g_object_type_install_info(gobject::cryptohome_get_type(),
                                  &gobject::dbus_glib_cryptohome_object_info);
  return Reset();
}

bool Service::Reset() {
  if (cryptohome_)
    g_object_unref(cryptohome_);
  cryptohome_ = reinterpret_cast<gobject::Cryptohome*>(
      g_object_new(gobject::cryptohome_get_type(), NULL));
  // Allow references to this instance.
  cryptohome_->service = this;

  if (loop_) {
    ::g_main_loop_unref(loop_);
  }
  loop_ = g_main_loop_new(NULL, false);
  if (!loop_) {
    LOG(ERROR) << "Failed to create main loop";
    return false;
  }
  return true;
}

gboolean Service::CheckKey(gchar *userid,
                           gchar *key,
                           gboolean *OUT_success,
                           GError **error) {
  UsernamePasskey credentials(userid, strlen(userid),
                              chromeos::Blob(key, key + strlen(key)));

  // TODO(fes): Handle CHROMEOS_PAM_LOCALACCOUNT
  *OUT_success = mount_->TestCredentials(credentials);
  return TRUE;
}

gboolean Service::MigrateKey(gchar *userid,
                             gchar *from_key,
                             gchar *to_key,
                             gboolean *OUT_success,
                             GError **error) {
  UsernamePasskey credentials(userid, strlen(userid),
                              chromeos::Blob(to_key, to_key + strlen(to_key)));

  *OUT_success = mount_->MigratePasskey(credentials, from_key);
  return TRUE;
}

gboolean Service::Remove(gchar *userid,
                         gboolean *OUT_success,
                         GError **error) {
  UsernamePasskey credentials(userid, strlen(userid),
                              chromeos::Blob());

  *OUT_success = mount_->RemoveCryptohome(credentials);
  return TRUE;
}

gboolean Service::GetSystemSalt(GArray **OUT_salt, GError **error) {
  if(system_salt_.size() == 0) {
    system_salt_ = mount_->GetSystemSalt();
  }
  *OUT_salt = g_array_new(false, false, 1);
  g_array_append_vals(*OUT_salt, &system_salt_.front(), system_salt_.size());

  return TRUE;
}

gboolean Service::IsMounted(gboolean *OUT_is_mounted, GError **error) {
  *OUT_is_mounted = mount_->IsCryptohomeMounted();
  return TRUE;
}

gboolean Service::Mount(gchar *userid,
                        gchar *key,
                        gboolean *OUT_done,
                        GError **error) {
  UsernamePasskey credentials(userid, strlen(userid),
                              chromeos::Blob(key, key + strlen(key)));

  if(mount_->IsCryptohomeMounted()) {
    if(mount_->IsCryptohomeMountedForUser(credentials)) {
      LOG(INFO) << "Cryptohome already mounted for this user";
      *OUT_done = TRUE;
      return TRUE;
    } else {
      if(!mount_->UnmountCryptohome()) {
        LOG(ERROR) << "Could not unmount cryptohome from previous user";
        *OUT_done = FALSE;
        return TRUE;
      }
    }
  }

  // TODO(fes): Iterate keys if we change how cryptohome keeps track of key
  // indexes.  Right now, 0 is always the current, and 0+n should only be used
  // temporarily during password migration.
  Mount::MountError mount_error = Mount::MOUNT_ERROR_NONE;
  *OUT_done = mount_->MountCryptohome(credentials, 0, &mount_error);
  if(!(*OUT_done) && (mount_error == Mount::MOUNT_ERROR_KEY_FAILURE)) {
    // If there is a key failure, create cryptohome from scratch.
    // TODO(fes): remove this when Chrome is no longer expecting this behavior.
    if(mount_->RemoveCryptohome(credentials)) {
      *OUT_done = mount_->MountCryptohome(credentials, 0, &mount_error);
    }
  }
  return TRUE;
}

gboolean Service::Unmount(gboolean *OUT_done, GError **error) {
  if(mount_->IsCryptohomeMounted()) {
    *OUT_done = mount_->UnmountCryptohome();
  } else {
    *OUT_done = true;
  }
  return TRUE;
}

}  // namespace cryptohome
