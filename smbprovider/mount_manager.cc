// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/mount_manager.h"

#include <algorithm>

#include <base/strings/string_util.h>
#include <base/time/tick_clock.h>

#include "smbprovider/credential_store.h"
#include "smbprovider/smb_credential.h"
#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {

MountManager::MountManager(std::unique_ptr<CredentialStore> credential_store,
                           std::unique_ptr<base::TickClock> tick_clock,
                           SambaInterfaceFactory samba_interface_factory)
    : credential_store_(std::move(credential_store)),
      tick_clock_(std::move(tick_clock)),
      samba_interface_factory_(std::move(samba_interface_factory)) {
  system_samba_interface_ = CreateSambaInterface();
}

MountManager::~MountManager() = default;

bool MountManager::IsAlreadyMounted(int32_t mount_id) const {
  auto mount_iter = mounts_.Find(mount_id);
  if (mount_iter == mounts_.End()) {
    return false;
  }

  DCHECK(credential_store_->HasCredential(mount_iter->second.mount_root));
  return true;
}

bool MountManager::IsAlreadyMounted(const std::string& mount_root) const {
  const bool has_credential = credential_store_->HasCredential(mount_root);
  if (!has_credential) {
    DCHECK(!ExistsInMounts(mount_root));
    return false;
  }

  DCHECK(ExistsInMounts(mount_root));
  return true;
}

bool MountManager::AddMount(const std::string& mount_root,
                            const std::string& workgroup,
                            const std::string& username,
                            const base::ScopedFD& password_fd,
                            int32_t* mount_id) {
  DCHECK(mount_id);

  SmbCredential credential(workgroup, username, GetPassword(password_fd));
  if (!credential_store_->AddCredential(mount_root, std::move(credential))) {
    return false;
  }

  can_remount_ = false;
  // TODO(jimmyxgong): Pass the credential into MountInfo. MountInfo will hold
  // the SmbCredential.
  *mount_id = mounts_.Insert(
      MountInfo(mount_root, tick_clock_.get(), CreateSambaInterface()));
  return true;
}

bool MountManager::Remount(const std::string& mount_root,
                           int32_t mount_id,
                           const std::string& workgroup,
                           const std::string& username,
                           const base::ScopedFD& password_fd) {
  DCHECK(can_remount_);
  DCHECK(!IsAlreadyMounted(mount_id));
  DCHECK_GE(mount_id, 0);

  SmbCredential credential(workgroup, username, GetPassword(password_fd));
  if (!credential_store_->AddCredential(mount_root, std::move(credential))) {
    return false;
  }

  // TODO(jimmyxgong): Pass the credential into MountInfo. MountInfo will hold
  // the SmbCredential.
  mounts_.InsertWithSpecificId(
      mount_id,
      MountInfo(mount_root, tick_clock_.get(), CreateSambaInterface()));
  return true;
}

bool MountManager::RemoveMount(int32_t mount_id) {
  auto mount_iter = mounts_.Find(mount_id);
  if (mount_iter == mounts_.End()) {
    return false;
  }

  bool removed =
      credential_store_->RemoveCredential(mount_iter->second.mount_root);
  DCHECK(removed);

  mounts_.Remove(mount_iter->first);
  return true;
}

bool MountManager::GetFullPath(int32_t mount_id,
                               const std::string& entry_path,
                               std::string* full_path) const {
  DCHECK(full_path);

  auto mount_iter = mounts_.Find(mount_id);
  if (mount_iter == mounts_.End()) {
    return false;
  }

  *full_path = AppendPath(mount_iter->second.mount_root, entry_path);
  return true;
}

bool MountManager::GetMetadataCache(int32_t mount_id,
                                    MetadataCache** cache) const {
  DCHECK(cache);

  auto mount_iter = mounts_.Find(mount_id);
  if (mount_iter == mounts_.End()) {
    return false;
  }

  *cache = mount_iter->second.cache.get();
  DCHECK(*cache);
  return true;
}

std::string MountManager::GetRelativePath(int32_t mount_id,
                                          const std::string& full_path) const {
  auto mount_iter = mounts_.Find(mount_id);
  DCHECK(mount_iter != mounts_.End());

  DCHECK(StartsWith(full_path, mount_iter->second.mount_root,
                    base::CompareCase::INSENSITIVE_ASCII));

  return full_path.substr(mount_iter->second.mount_root.length());
}

bool MountManager::GetSambaInterface(int32_t mount_id,
                                     SambaInterface** samba_interface) const {
  DCHECK(samba_interface);

  auto mount_iter = mounts_.Find(mount_id);
  if (mount_iter == mounts_.End()) {
    return false;
  }

  *samba_interface = mount_iter->second.samba_interface.get();
  DCHECK(*samba_interface);

  return true;
}

SambaInterface* MountManager::GetSystemSambaInterface() const {
  return system_samba_interface_.get();
}

std::unique_ptr<SambaInterface> MountManager::CreateSambaInterface() {
  return samba_interface_factory_.Run(this);
}

bool MountManager::GetAuthentication(
    SambaInterface::SambaInterfaceId samba_interface_id,
    const std::string& share_path,
    char* workgroup,
    int32_t workgroup_length,
    char* username,
    int32_t username_length,
    char* password,
    int32_t password_length) const {
  DCHECK_GT(workgroup_length, 0);
  DCHECK_GT(username_length, 0);
  DCHECK_GT(password_length, 0);

  return credential_store_->GetAuthentication(
      share_path, workgroup, workgroup_length, username, username_length,
      password, password_length);
}

bool MountManager::ExistsInMounts(const std::string& mount_root) const {
  for (auto mount_iter = mounts_.Begin(); mount_iter != mounts_.End();
       ++mount_iter) {
    if (mount_iter->second.mount_root == mount_root) {
      return true;
    }
  }

  return false;
}

}  // namespace smbprovider
