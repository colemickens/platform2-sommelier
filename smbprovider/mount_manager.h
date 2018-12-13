// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_MOUNT_MANAGER_H_
#define SMBPROVIDER_MOUNT_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <base/callback.h>
#include <base/files/file_util.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <libpasswordprovider/password.h>

#include "smbprovider/constants.h"
#include "smbprovider/id_map.h"
#include "smbprovider/metadata_cache.h"
#include "smbprovider/mount_config.h"
#include "smbprovider/mount_tracker.h"
#include "smbprovider/samba_interface.h"
#include "smbprovider/smb_credential.h"

namespace base {
class TickClock;
};

namespace smbprovider {

// Gets a password_provider::Password object from |password_fd|. The data has to
// be in the format of "{password_length}{password}". If the read fails, this
// returns an empty unique_ptr.
std::unique_ptr<password_provider::Password> GetPassword(
    const base::ScopedFD& password_fd);

// MountManager maintains a mapping of open mounts and the metadata associated
// with each mount.
class MountManager : public base::SupportsWeakPtr<MountManager> {
 public:
  using SambaInterfaceFactory = base::Callback<std::unique_ptr<SambaInterface>(
      MountManager*, const MountConfig& mount_config)>;

  MountManager(std::unique_ptr<MountTracker> mount_tracker,
               SambaInterfaceFactory samba_interface_factory);
  ~MountManager();

  // Returns true if |mount_id| is already mounted.
  bool IsAlreadyMounted(int32_t mount_id) const;

  // Returns true if |mount_root| is already mounted.
  bool IsAlreadyMounted(const std::string& mount_root) const;

  // Adds |mount_root| to the |mounts_| map and outputs the |mount_id|
  // that was assigned to this mount. Ids are >=0 and are not
  // re-used within the lifetime of this class. |mount_config| holds the mount
  // options set by the client. If |mount_root| is already mounted, this
  // returns false and |mount_id| will be unmodified.
  // TODO(zentaro): Review if this should have a maximum number of mounts,
  // even if it is relatively large. It may already be enforced at a higher
  // level.
  bool AddMount(const std::string& mount_root,
                SmbCredential credential,
                const MountConfig& mount_config,
                int32_t* mount_id);

  // Adds |mount_root| to the |mounts_| map with a specific mount_id. Must not
  // be called after AddMount is called for the first time. |mount_config| holds
  // the mount options set by the client.  Will always remount a share
  // regardless if it can be reached on the network or not. Returns false if
  // |mount_root| is already mounted.
  bool Remount(const std::string& mount_root,
               int32_t mount_id,
               SmbCredential credential,
               const MountConfig& mount_config);

  // Returns true if |mount_id| was mounted and removes the mount.
  bool RemoveMount(int32_t mount_id);

  // Returns the number of mounts.
  size_t MountCount() const { return mount_tracker_->MountCount(); }

  // Uses the mount root associated with |mount_id| and appends |entry_path|
  // to form |full_path|.
  bool GetFullPath(int32_t mount_id,
                   const std::string& entry_path,
                   std::string* full_path) const;

  // Gets a pointer to the metadata cache for |mount_id|.
  bool GetMetadataCache(int32_t mount_id, MetadataCache** cache) const;

  // Uses the mount root associated with |mount_id| to remove the root path
  // from |full_path| to yield a relative path.
  std::string GetRelativePath(int32_t mount_id,
                              const std::string& full_path) const;

  // Returns a pointer to the SambaInterface corresponding to |mount_id|.
  bool GetSambaInterface(int32_t mount_id,
                         SambaInterface** samba_interface) const;

  // Returns a pointer to the system SambaInterface.
  SambaInterface* GetSystemSambaInterface() const;

  // Samba authentication function callback. DCHECKS that the buffer lengths are
  // non-zero. Returns false when buffer lengths cannot support credential
  // length or when credential are not found for |share_path|.
  bool GetAuthentication(SambaInterface::SambaInterfaceId samba_interface_id,
                         const std::string& share_path,
                         char* workgroup,
                         int32_t workgroup_length,
                         char* username,
                         int32_t username_length,
                         char* password,
                         int32_t password_length) const;

  // Updates the SmbCredential for the given mount. Returns true if updating the
  // mount's credential was successful. Returns false if credentials were not
  // updated.
  bool UpdateMountCredential(int32_t mount_id, SmbCredential credential);

 private:
  // Runs |samba_interface_factory_|.
  std::unique_ptr<SambaInterface> CreateSambaInterface(
      const MountConfig& mount_config);

  // Returns the SambaInterfaceId from |system_samba_interface_|.
  SambaInterface::SambaInterfaceId GetSystemSambaInterfaceId();

  // Returns the SmbCredential for |samba_interface_id|.
  const SmbCredential& GetCredential(
      SambaInterface::SambaInterfaceId samba_interface_id) const;

  bool can_remount_ = true;

  std::unique_ptr<MountTracker> mount_tracker_;
  std::unique_ptr<SambaInterface> system_samba_interface_;
  SambaInterfaceFactory samba_interface_factory_;

  DISALLOW_COPY_AND_ASSIGN(MountManager);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_MOUNT_MANAGER_H_
