// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "service.h"

#include <stdio.h>
#include <stdlib.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/time.h>
#include <chromeos/dbus/dbus.h>

#include "cryptohome/marshal.glibmarshal.h"
#include "cryptohome_event_source.h"
#include "interface.h"
#include "crypto.h"
#include "mount.h"
#include "secure_blob.h"
#include "username_passkey.h"

// Forcibly namespace the dbus-bindings generated server bindings instead of
// modifying the files afterward.
namespace cryptohome {  // NOLINT
namespace gobject {  // NOLINT
#include "cryptohome/bindings/server.h"
}  // namespace gobject
}  // namespace cryptohome

namespace cryptohome {

const int kDefaultRandomSeedLength = 64;
const char* kMountThreadName = "MountThread";

Service::Service()
    : loop_(NULL),
      cryptohome_(NULL),
      system_salt_(),
      default_mount_(new cryptohome::Mount()),
      mount_(default_mount_.get()),
      default_tpm_init_(new tpm_init::TpmInit()),
      tpm_init_(default_tpm_init_.get()),
      initialize_tpm_(true),
      mount_thread_(kMountThreadName),
      async_complete_signal_(-1),
      event_source_() {
}

Service::~Service() {
  if (loop_)
    g_main_loop_unref(loop_);
  if (cryptohome_)
    g_object_unref(cryptohome_);
  if (mount_thread_.IsRunning()) {
    mount_thread_.Stop();
  }
}

bool Service::Initialize() {
  bool result = true;

  if (initialize_tpm_) {
    if (!SeedUrandom()) {
      LOG(ERROR) << "FAILED TO SEED /dev/urandom AT START";
    }
    if (!tpm_init_->StartInitializeTpm()) {
      LOG(ERROR) << "Failed start initializing the TPM";
      result = false;
    }
  }

  mount_->Init();

  // Install the type-info for the service with dbus.
  dbus_g_object_type_install_info(gobject::cryptohome_get_type(),
                                  &gobject::dbus_glib_cryptohome_object_info);
  if (!Reset()) {
    result = false;
  }

  async_complete_signal_ = g_signal_new("async_call_status",
                                        gobject::cryptohome_get_type(),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL,
                                        NULL,
                                        cryptohome_VOID__INT_BOOLEAN_INT,
                                        G_TYPE_NONE,
                                        3,
                                        G_TYPE_INT,
                                        G_TYPE_BOOLEAN,
                                        G_TYPE_INT);

  mount_thread_.Start();

  return result;
}

bool Service::SeedUrandom() {
  SecureBlob random;
  if (!tpm_init_->GetRandomData(kDefaultRandomSeedLength, &random)) {
    LOG(ERROR) << "Could not get random data from the TPM";
    return false;
  }
  size_t written = file_util::WriteFile(FilePath(kDefaultEntropySource),
      static_cast<const char*>(random.const_data()), random.size());
  if (written != random.size()) {
    LOG(ERROR) << "Error writing data to /dev/urandom";
    return false;
  }
  return true;
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
  // Install the local event source for handling async results
  event_source_.Reset(this, g_main_loop_get_context(loop_));
  return true;
}

void Service::MountTaskObserve(const MountTaskResult& result) {
  event_source_.AddEvent(result);
}

void Service::NotifyComplete(const MountTaskResult& result) {
  g_signal_emit(cryptohome_, async_complete_signal_, 0, result.sequence_id(),
                result.return_status(), result.return_code());
}

gboolean Service::CheckKey(gchar *userid,
                           gchar *key,
                           gboolean *OUT_result,
                           GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskTestCredentials* mount_task =
      new MountTaskTestCredentials(NULL, mount_, credentials);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  event.Wait();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncCheckKey(gchar *userid,
                                gchar *key,
                                gint *OUT_async_id,
                                GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  // Freed by the message loop
  MountTaskTestCredentials* mount_task = new MountTaskTestCredentials(
      this,
      mount_,
      credentials);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  return TRUE;
}

gboolean Service::MigrateKey(gchar *userid,
                             gchar *from_key,
                             gchar *to_key,
                             gboolean *OUT_result,
                             GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(to_key, strlen(to_key)));

  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskMigratePasskey* mount_task =
      new MountTaskMigratePasskey(NULL, mount_, credentials, from_key);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  event.Wait();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncMigrateKey(gchar *userid,
                                  gchar *from_key,
                                  gchar *to_key,
                                  gint *OUT_async_id,
                                  GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(to_key, strlen(to_key)));

  MountTaskMigratePasskey* mount_task =
      new MountTaskMigratePasskey(this, mount_, credentials, from_key);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  return TRUE;
}

gboolean Service::Remove(gchar *userid,
                         gboolean *OUT_result,
                         GError **error) {
  UsernamePasskey credentials(userid, chromeos::Blob());

  *OUT_result = mount_->RemoveCryptohome(credentials);
  return TRUE;
}

gboolean Service::GetSystemSalt(GArray **OUT_salt, GError **error) {
  if (system_salt_.size() == 0) {
    mount_->GetSystemSalt(&system_salt_);
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
                        gint *OUT_error_code,
                        gboolean *OUT_result,
                        GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  if (mount_->IsCryptohomeMounted()) {
    if (mount_->IsCryptohomeMountedForUser(credentials)) {
      LOG(INFO) << "Cryptohome already mounted for this user";
      *OUT_error_code = Mount::MOUNT_ERROR_NONE;
      *OUT_result = TRUE;
      return TRUE;
    } else {
      if (!mount_->UnmountCryptohome()) {
        LOG(ERROR) << "Could not unmount cryptohome from previous user";
        *OUT_error_code = Mount::MOUNT_ERROR_MOUNT_POINT_BUSY;
        *OUT_result = FALSE;
        return TRUE;
      }
    }
  }

  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskMount* mount_task = new MountTaskMount(NULL, mount_, credentials);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  event.Wait();
  *OUT_error_code = result.return_code();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncMount(gchar *userid,
                             gchar *key,
                             gint *OUT_async_id,
                             GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  if (mount_->IsCryptohomeMounted()) {
    if (mount_->IsCryptohomeMountedForUser(credentials)) {
      LOG(INFO) << "Cryptohome already mounted for this user";
      MountTaskNop* mount_task = new MountTaskNop(this);
      mount_task->result()->set_return_code(Mount::MOUNT_ERROR_NONE);
      mount_task->result()->set_return_status(true);
      *OUT_async_id = mount_task->sequence_id();
      mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
      return TRUE;
    } else {
      if (!mount_->UnmountCryptohome()) {
        LOG(ERROR) << "Could not unmount cryptohome from previous user";
        MountTaskNop* mount_task = new MountTaskNop(this);
        mount_task->result()->set_return_code(
            Mount::MOUNT_ERROR_MOUNT_POINT_BUSY);
        mount_task->result()->set_return_status(false);
        *OUT_async_id = mount_task->sequence_id();
        mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
        return TRUE;
      }
    }
  }

  MountTaskMount* mount_task = new MountTaskMount(this, mount_, credentials);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  return TRUE;
}

gboolean Service::MountGuest(gint *OUT_error_code,
                             gboolean *OUT_result,
                             GError **error) {
  if (mount_->IsCryptohomeMounted()) {
    if (!mount_->UnmountCryptohome()) {
      LOG(ERROR) << "Could not unmount cryptohome from previous user";
      *OUT_error_code = Mount::MOUNT_ERROR_MOUNT_POINT_BUSY;
      *OUT_result = FALSE;
      return TRUE;
    }
  }

  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskMountGuest* mount_task = new MountTaskMountGuest(NULL, mount_);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  event.Wait();
  *OUT_error_code = result.return_code();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncMountGuest(gint *OUT_async_id,
                                  GError **error) {
  if (mount_->IsCryptohomeMounted()) {
    if (!mount_->UnmountCryptohome()) {
      LOG(ERROR) << "Could not unmount cryptohome from previous user";
      MountTaskNop* mount_task = new MountTaskNop(this);
      mount_task->result()->set_return_code(
          Mount::MOUNT_ERROR_MOUNT_POINT_BUSY);
      mount_task->result()->set_return_status(false);
      *OUT_async_id = mount_task->sequence_id();
      mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
      return TRUE;
    }
  }

  MountTaskMountGuest* mount_task = new MountTaskMountGuest(this, mount_);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  return TRUE;
}

gboolean Service::Unmount(gboolean *OUT_result, GError **error) {
  if (mount_->IsCryptohomeMounted()) {
    *OUT_result = mount_->UnmountCryptohome();
  } else {
    *OUT_result = true;
  }
  return TRUE;
}

gboolean Service::TpmIsReady(gboolean* OUT_ready, GError** error) {
  *OUT_ready = tpm_init_->IsTpmReady();
  return TRUE;
}

gboolean Service::TpmIsEnabled(gboolean* OUT_enabled, GError** error) {
  *OUT_enabled = tpm_init_->IsTpmEnabled();
  return TRUE;
}

gboolean Service::TpmGetPassword(gchar** OUT_password, GError** error) {
  SecureBlob password;
  if (!tpm_init_->GetTpmPassword(&password)) {
    *OUT_password = NULL;
    return TRUE;
  }
  *OUT_password = g_strdup_printf("%.*s", password.size(),
                                  static_cast<char*>(password.data()));
  return TRUE;
}

}  // namespace cryptohome
