// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_MOUNT_MANAGER_H_
#define SMBPROVIDER_MOUNT_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include <base/callback.h>
#include <base/files/file_util.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "smbprovider/constants.h"
#include "smbprovider/id_map.h"
#include "smbprovider/metadata_cache.h"
#include "smbprovider/samba_interface.h"
#include "smbprovider/smb_credential.h"

namespace base {
class TickClock;
};

namespace smbprovider {

class CredentialStore;

// MountManager maintains a mapping of open mounts and the metadata associated
// with each mount.
class MountManager : public base::SupportsWeakPtr<MountManager> {
 public:
  using SambaInterfaceFactory =
      base::Callback<std::unique_ptr<SambaInterface>(MountManager*)>;

  MountManager(std::unique_ptr<CredentialStore> credential_store,
               std::unique_ptr<base::TickClock> tick_clock,
               SambaInterfaceFactory samba_interface_factory);
  ~MountManager();

  // Returns true if |mount_id| is already mounted.
  bool IsAlreadyMounted(int32_t mount_id) const;

  // Returns true if |mount_root| is already mounted.
  bool IsAlreadyMounted(const std::string& mount_root) const;

  // Adds |mount_root| to the |mounts_| map and outputs the |mount_id|
  // that was assigned to this mount. Ids are >=0 and are not
  // re-used within the lifetime of this class. If |mount_root| is already
  // mounted, this returns false and |mount_id| will be unmodified. If
  // |workgroup|, |username|, and |password_fd| are provided, they will be used
  // as a credential when interacting with the mount.
  // TODO(zentaro): Review if this should have a maximum number of mounts,
  // even if it is relatively large. It may already be enforced at a higher
  // level.
  bool AddMount(const std::string& mount_root,
                const std::string& workgroup,
                const std::string& username,
                const base::ScopedFD& password_fd,
                int32_t* mount_id);

  // Adds |mount_root| to the |mounts_| map with a specific mount_id. Must not
  // be called after AddMount is called for the first time. Returns false if
  // |mount_root| is already mounted. If |workgroup| and |username| are
  // provided, they will be used as a credential when interacting with the
  // mount.
  bool Remount(const std::string& mount_root,
               int32_t mount_id,
               const std::string& workgroup,
               const std::string& username,
               const base::ScopedFD& password_fd);

  // Returns true if |mount_id| was mounted and removes the mount.
  bool RemoveMount(int32_t mount_id);

  // Returns the number of mounts.
  size_t MountCount() const { return mounts_.Count(); }

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

 private:
  // Maintains the state of a single mount. Contains the mount root path and
  // the metadata cache.
  struct MountInfo {
    MountInfo() = default;
    MountInfo(MountInfo&& other) = default;
    MountInfo(const std::string& mount_root,
              base::TickClock* tick_clock,
              SmbCredential credential,
              std::unique_ptr<SambaInterface> samba_interface)
        : mount_root(mount_root),
          credential(std::move(credential)),
          samba_interface(std::move(samba_interface)) {
      cache = std::make_unique<MetadataCache>(
          tick_clock, base::TimeDelta::FromMicroseconds(
                          kMetadataCacheLifetimeMicroseconds));
    }

    MountInfo& operator=(MountInfo&& other) = default;

    std::string mount_root;
    SmbCredential credential;
    std::unique_ptr<SambaInterface> samba_interface;
    std::unique_ptr<MetadataCache> cache;

    DISALLOW_COPY_AND_ASSIGN(MountInfo);
  };

  // Returns true if |mount_root| exists as a value in mounts_. This method is
  // only used for DCHECK to ensure that credential_store_ is in sync with
  // MountManager.
  bool ExistsInMounts(const std::string& mount_root) const;

  // Runs |samba_interface_factory_|.
  std::unique_ptr<SambaInterface> CreateSambaInterface();

  // Returns a new MountInfo.
  MountInfo CreateMountInfo(const std::string& mount_root,
                            SmbCredential credential);

  // Returns the SambaInterfaceId from |system_samba_interface_|.
  SambaInterface::SambaInterfaceId GetSystemSambaInterfaceId();

  bool can_remount_ = true;
  IdMap<MountInfo> mounts_;

  // Maps SambaInterfaceId to MountId.
  std::unordered_map<SambaInterface::SambaInterfaceId, int32_t>
      samba_interface_map_;
  std::unique_ptr<CredentialStore> credential_store_;
  std::unique_ptr<base::TickClock> tick_clock_;
  std::unique_ptr<SambaInterface> system_samba_interface_;
  SambaInterfaceFactory samba_interface_factory_;

  DISALLOW_COPY_AND_ASSIGN(MountManager);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_MOUNT_MANAGER_H_
