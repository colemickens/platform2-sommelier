// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/install_attributes.h"

#include <sys/types.h>

#include <limits>
#include <string>
#include <utility>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/time/time.h>

#include "cryptohome/lockbox.h"
#include "cryptohome/tpm_init.h"

#include "install_attributes.pb.h"  // NOLINT(build/include)

using base::FilePath;

namespace cryptohome {

// By default, we store this with other cryptohome state.
const char InstallAttributes::kDefaultDataFile[] =
  "/home/.shadow/install_attributes.pb";
const mode_t InstallAttributes::kDataFilePermissions = 0644;
// This is the default location for the cache file.
const char InstallAttributes::kDefaultCacheFile[] =
  "/run/lockbox/install_attributes.pb";
const mode_t InstallAttributes::kCacheFilePermissions = 0644;

InstallAttributes::InstallAttributes(Tpm* tpm)
  : is_first_install_(false),
    is_invalid_(false),
    is_initialized_(false),
    data_file_(kDefaultDataFile),
    cache_file_(kDefaultCacheFile),
    default_attributes_(new SerializedInstallAttributes()),
    default_lockbox_(new Lockbox(tpm, Tpm::kLockboxIndex)),
    default_platform_(new Platform()),
    attributes_(default_attributes_.get()),
    lockbox_(default_lockbox_.get()),
    platform_(default_platform_.get()) {
  SetTpm(tpm);  // make sure to check TPM status if needed.
  version_ = attributes_->version();  // versioning controlled by pb default.
}

InstallAttributes::~InstallAttributes() {
}

bool InstallAttributes::PrepareSystem() {
  // If install attributes are already locked, there's nothing to do. This can
  // happen when we reset the TPM but preserve system state, such as for TPM
  // firmware updates.
  brillo::Blob blob;
  if (platform_->ReadFile(cache_file_, &blob)) {
    return true;
  }

  set_is_first_install(true);
  LockboxError error_id;
  // Delete the attributes file if it exists.
  if (platform_->FileExists(data_file_) &&
      !platform_->DeleteFile(data_file_, false)) {
    LOG(ERROR) << "PrepareSystem() unable to delete old data!";
    return false;
  }
  // If a TPM is in use, let's clean up the lockbox.
  if (is_secure())
    return lockbox()->Destroy(&error_id);
  return true;
}

void InstallAttributes::SetIsInvalid(bool is_invalid) {
  // If a store is invalid, make sure it is forced to be empty.
  is_invalid_ = is_invalid;
  if (is_invalid) {
    set_is_first_install(false);
    attributes_->Clear();
  }
}

void InstallAttributes::SetTpm(Tpm* tpm) {
  // Technically, it is safe to call SetTpm, then Init() again, but it could
  // also cause weirdness and report that data is TPM-backed when it isn't.
  DCHECK(!is_initialized()) << "SetTpm used after a successful Init().";
  if (tpm && !tpm->IsEnabled()) {
    LOG(WARNING) << "set_tpm() missing or disabled TPM provided.";
    tpm = NULL;  // Don't give it to Lockbox.
  }
  set_is_secure(tpm != NULL);
  lockbox_->set_tpm(tpm);
}

bool InstallAttributes::Init(TpmInit* tpm_init) {
  LockboxError error_id;

  // Insure that if Init() was called and it failed, we can retry cleanly.
  attributes_->Clear();
  SetIsInvalid(false);
  set_is_initialized(false);

  if (is_first_install()) {
    if (!is_secure()) {
      LOG(WARNING) << "InstallAttributes are insecure without a TPM.";
      set_is_initialized(true);
      return true;
    }
    if (!lockbox()->Create(&error_id)) {
      if (error_id == LockboxError::kInsufficientAuthorization)
        LOG(ERROR) << "First install, but no TPM credentials provided.";
      SetIsInvalid(true);
      return false;
    }

    set_is_initialized(true);
    tpm_init->RemoveTpmOwnerDependency(
        TpmPersistentState::TpmOwnerDependency::kInstallAttributes);
    return true;
  }

  if (is_secure() && !lockbox()->Load(&error_id)) {
    // There are two non-terminal error cases:
    // 1. No NVRAM space is defined.  This will occur on systems that
    //    are autoupdated with this code but never went through the OOBE,
    //    or if creation was interrupted after TPM Ownership happened.
    // 2. NVRAM space exists and is unlocked. It means the system was powered
    //    off before any data was stored.
    switch (error_id) {
    case LockboxError::kNoNvramSpace:
      LOG(INFO) << "Resuming interrupted InstallAttributes. (Create needed.)";
      if (!lockbox()->Create(&error_id)) {
        if (error_id == LockboxError::kInsufficientAuthorization)
          DLOG(INFO) << "Legacy install. (Can never create NVRAM space.)";
        else
          LOG(ERROR) << "Create failed, Lockbox error: " << error_id;
      } else {
        // Create worked, so act like the LockboxError::kNoNvramData path now.
        set_is_first_install(true);
      }
      set_is_initialized(true);
      tpm_init->RemoveTpmOwnerDependency(
          TpmPersistentState::TpmOwnerDependency::kInstallAttributes);
      // No data.
      return true;
      break;
    case LockboxError::kNoNvramData:
      LOG(INFO) << "Resuming interrupted InstallAttributes. (Store needed.)";
      set_is_first_install(true);
      set_is_initialized(true);
      tpm_init->RemoveTpmOwnerDependency(
          TpmPersistentState::TpmOwnerDependency::kInstallAttributes);
      // Since we write when we finalize, we don't try to reparse.
      return true;
      break;
    default:
      LOG(ERROR) << "InstallAttributes failed to initialize.";
      SetIsInvalid(true);
      return false;
    }
  }

  // Load the file from disk.
  brillo::Blob blob;
  if (!platform_->ReadFile(data_file_, &blob)) {
    LOG(WARNING) << "Init() failed to read attributes file.";
    // If this is an insecure install, then we can just start the
    // file fresh, otherwise it signifies tampering.
    if (is_secure()) {
      SetIsInvalid(true);
      return false;
    }
    LOG(INFO) << "Init() assuming first-time install for TPM-less system.";
    set_is_first_install(true);
    set_is_initialized(true);
    return true;
  }

  // Prior to attempting to deserialize the data, ensure it has not
  // been tampered with.
  if (is_secure() && !lockbox()->Verify(blob, &error_id)) {
    LOG(ERROR) << "Init() could not verify attribute data!";
    SetIsInvalid(true);
    return false;
  }

  if (!attributes_->ParseFromArray(
         static_cast<google::protobuf::uint8*>(blob.data()),
         blob.size())) {
    LOG(ERROR) << "Failed to parse data file (" << blob.size() << " bytes)";
    SetIsInvalid(true);
    return false;
  }

  set_is_initialized(true);
  // If everything went well, we know that NVRAM space was created OK, and
  // don't need to hold owner dependency. So, repeat removing owner dependency
  // in case it didn't succeed during the first boot.
  tpm_init->RemoveTpmOwnerDependency(
      TpmPersistentState::TpmOwnerDependency::kInstallAttributes);
  return true;
}

int InstallAttributes::FindIndexByName(const std::string& name) const {
  std::string name_str(name);
  for (int i = 0; i < attributes_->attributes_size(); ++i) {
    if (attributes_->attributes(i).name().compare(name_str) == 0)
      return i;
  }
  return -1;
}

bool InstallAttributes::Get(const std::string& name,
                            brillo::Blob* value) const {
  int index = FindIndexByName(name);
  if (index == -1)
    return false;
  return GetByIndex(index, NULL, value);
}

bool InstallAttributes::GetByIndex(int index,
                                   std::string* name,
                                   brillo::Blob* value) const {
  if (index < 0 || index >= attributes_->attributes_size()) {
    LOG(ERROR) << "GetByIndex called with invalid index.";
    return false;
  }
  const SerializedInstallAttributes::Attribute* const attr =
    &attributes_->attributes(index);
  if (name) {
    name->assign(attr->name());
  }
  if (value) {
    value->resize(attr->value().length());
    memcpy(&value->at(0), attr->value().c_str(), value->size());
  }
  return true;
}

bool InstallAttributes::Set(const std::string& name,
                            const brillo::Blob& value) {
  if (!is_first_install()) {
    LOG(ERROR) << "Set() called on immutable attributes.";
    return false;
  }

  if (Count() == std::numeric_limits<int>::max()) {
    LOG(ERROR) << "Set() cannot insert into full attribute store.";
    return false;
  }

  // Clobber an existing entry if it exists.
  int index = FindIndexByName(name);
  if (index != -1) {
    SerializedInstallAttributes::Attribute* attr =
      attributes_->mutable_attributes(index);
    attr->set_value(std::string(reinterpret_cast<const char*>(value.data()),
                                value.size()));
    return true;
  }

  SerializedInstallAttributes::Attribute* attr = attributes_->add_attributes();
  if (!attr) {
    LOG(ERROR) << "Failed to add a new attribute.";
    return false;
  }
  attr->set_name(name);
  attr->set_value(std::string(reinterpret_cast<const char*>(value.data()),
                              value.size()));
  return true;
}

bool InstallAttributes::Finalize() {
  if (!IsReady()) {
    LOG(ERROR) << "Finalize() called with invalid/uninitialized data.";
    return false;
  }
  // Repeated calls to Finalize() are idempotent.
  if (!is_first_install())
    return true;

  // Restamp the version.
  attributes_->set_version(version_);

  // Serialize the bytestream
  brillo::Blob attr_bytes;
  if (!SerializeAttributes(&attr_bytes)) {
    LOG(ERROR) << "Finalize() failed to serialize the attributes.";
    return false;
  }

  LockboxError error;
  DLOG(INFO) << "Finalizing() " << attr_bytes.size() << " bytes.";
  if (is_secure() && !lockbox()->Store(attr_bytes, &error)) {
    LOG(ERROR) << "Finalize() failed with Lockbox error: " << error;
    // It may be possible to recover from a failed NVRAM store. So the
    // instance is not marked invalid.
    return false;
  }

  if (!platform_->WriteFileAtomicDurable(data_file_, attr_bytes,
                                         kDataFilePermissions)) {
    LOG(ERROR) << "Finalize() write failed after locking the Lockbox.";
    SetIsInvalid(true);
    return false;
  }

  // As the cache file is stored on tmpfs, durable write is not required but we
  // need atomicity to be safe in case of concurrent reads.
  if (!platform_->WriteFileAtomic(cache_file_, attr_bytes,
                                  kCacheFilePermissions)) {
    LOG(WARNING) << "Finalize() failed to create cache file.";
  }

  LOG(INFO) << "InstallAttributes have been finalized.";
  set_is_first_install(false);
  NotifyFinalized();
  return true;
}

int InstallAttributes::Count() const {
  return attributes_->attributes_size();
}

bool InstallAttributes::SerializeAttributes(brillo::Blob* out_bytes) {
  out_bytes->resize(attributes_->ByteSize());
  attributes_->SerializeWithCachedSizesToArray(
    static_cast<google::protobuf::uint8*>(out_bytes->data()));
  return true;
}

std::unique_ptr<base::Value> InstallAttributes::GetStatus() {
  auto dv = std::make_unique<base::DictionaryValue>();
  dv->SetBoolean("initialized", is_initialized());
  dv->SetInteger("version", version());
  dv->SetInteger("lockbox_index", lockbox()->nvram_index());
  dv->SetInteger("lockbox_nvram_version",
                 GetNvramVersionNumber(lockbox()->nvram_version()));
  dv->SetBoolean("secure", is_secure());
  dv->SetBoolean("invalid", is_invalid());
  dv->SetBoolean("first_install", is_first_install());
  dv->SetInteger("size", Count());
  if (Count()) {
    auto attrs = std::make_unique<base::DictionaryValue>();
    std::string key;
    brillo::Blob value;
    for (int i = 0; i < Count(); i++) {
      GetByIndex(i, &key, &value);
      std::string value_str(reinterpret_cast<const char*>(value.data()));
      attrs->SetString(key, value_str);
    }
    dv->Set("attrs", std::move(attrs));
  }
  return std::move(dv);
}

}  // namespace cryptohome
