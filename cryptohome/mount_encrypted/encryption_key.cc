// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/encryption_key.h"

#include <utility>

#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>

#include <brillo/file_utils.h>

#include "cryptohome/cryptolib.h"
#include "cryptohome/mount_encrypted.h"
#include "cryptohome/mount_encrypted/tpm.h"
#include "cryptohome/mount_helpers.h"

namespace {

namespace paths {
const char kStatefulMount[] = "mnt/stateful_partition";
const char kEncryptedKey[] = "encrypted.key";
const char kNeedsFinalization[] = "encrypted.needs-finalization";
const char kKernelCmdline[] = "/proc/cmdline";
const char kProductUUID[] = "/sys/class/dmi/id/product_uuid";
}

const char kKernelCmdlineOption[] = "encrypted-stateful-key=";
const char kStaticKeyDefault[] = "default unsafe static key";
const char kStaticKeyFactory[] = "factory unsafe static key";
const char kStaticKeyFinalizationNeeded[] = "needs finalization";

const size_t kMaxReadSize = 4 * 1024;

// Simplify logging of base::FilePath. Note that this has appeared in upstream
// chromium base/ already and can be removed once it has propagated to Chrome
// OS' base copy.
static inline std::ostream& operator<<(std::ostream& out,
                                       const base::FilePath& file_path) {
  return out << file_path.value();
}

bool ReadKeyFile(const base::FilePath& path,
                 brillo::SecureBlob* plaintext,
                 const brillo::SecureBlob& encryption_key) {
  std::string ciphertext;
  if (!base::ReadFileToStringWithMaxSize(path, &ciphertext, kMaxReadSize)) {
    LOG(ERROR) << "Data read failed from " << path;
    return false;
  }

  if (!cryptohome::CryptoLib::AesDecryptSpecifyBlockMode(
          brillo::Blob(ciphertext.begin(), ciphertext.end()), 0,
          ciphertext.size(), encryption_key,
          brillo::SecureBlob(cryptohome::kAesBlockSize),
          cryptohome::CryptoLib::kPaddingStandard, cryptohome::CryptoLib::kCbc,
          plaintext)) {
    LOG(ERROR) << "Decryption failed for data from " << path;
    return false;
  }

  return true;
}

bool WriteKeyFile(const base::FilePath& path,
                  const brillo::SecureBlob& plaintext,
                  const brillo::SecureBlob& encryption_key) {
  if (base::PathExists(path)) {
    LOG(ERROR) << path << " already exists.";
    return false;
  }

  // Note that we pass an all-zeros IV. In general, this is dangerous since
  // identical plaintext will lead to identical ciphertext, revealing the fact
  // that the same message has been encrypted. This can potentially be used in
  // chosen plaintext attacks to determine the plaintext for a given ciphertext.
  // In the case at hand, we only ever encrypt a single message using the system
  // key and don't allow attackers to inject plaintext, so we are good.
  //
  // Ideally, we'd generate a random IV and stored it to disk as well, but
  // switching over to the safer scheme would have to be done in a
  // backwards-compatible way, so for now it isn't worth it.
  brillo::SecureBlob ciphertext;
  if (!cryptohome::CryptoLib::AesEncryptSpecifyBlockMode(
          plaintext, 0, plaintext.size(), encryption_key,
          brillo::SecureBlob(cryptohome::kAesBlockSize),
          cryptohome::CryptoLib::kPaddingStandard, cryptohome::CryptoLib::kCbc,
          &ciphertext)) {
    LOG(ERROR) << "Encryption failed for " << path;
    return false;
  }

  if (!brillo::WriteBlobToFileAtomic(path, ciphertext, 0600) ||
      !brillo::SyncFileOrDirectory(path.DirName(), true, false)) {
    PLOG(ERROR) << "Unable to write " << path;
    return false;
  }

  return true;
}

std::string HexEncode(const brillo::SecureBlob& data) {
  return base::HexEncode(data.data(), data.size());
}

brillo::SecureBlob Sha256(const std::string& str) {
  brillo::Blob blob(str.data(), str.data() + str.size());
  return cryptohome::CryptoLib::Sha256(blob);
}

brillo::SecureBlob GetUselessKey() {
  return Sha256(kStaticKeyFinalizationNeeded);
}

// Extract the desired system key from the kernel's boot command line.
brillo::SecureBlob GetKeyFromKernelCmdline() {
  std::string cmdline;
  if (!base::ReadFileToStringWithMaxSize(base::FilePath(paths::kKernelCmdline),
                                         &cmdline, kMaxReadSize)) {
    PLOG(ERROR) << "Failed to read kernel command line";
    return brillo::SecureBlob();
  }

  // Find a string match either at start of string or following a space.
  size_t pos = cmdline.find(kKernelCmdlineOption);
  if (pos == std::string::npos || !(pos == 0 || cmdline[pos - 1] == ' ')) {
    return brillo::SecureBlob();
  }

  std::string value = cmdline.substr(pos + strlen(kKernelCmdlineOption));
  value = value.substr(0, value.find(' '));

  brillo::SecureBlob key = Sha256(value);
  VLOG(1) << "system key: " << HexEncode(key);
  return key;
}

}  // namespace

EncryptionKey::EncryptionKey(Tpm* tpm, const base::FilePath& rootdir)
    : tpm_(tpm) {
  base::FilePath stateful_mount = rootdir.AppendASCII(paths::kStatefulMount);
  key_path_ = stateful_mount.AppendASCII(paths::kEncryptedKey);
  needs_finalization_path_ =
      stateful_mount.AppendASCII(paths::kNeedsFinalization);
}

result_code EncryptionKey::SetFactorySystemKey() {
  LOG(INFO) << "Using factory insecure system key.";
  system_key_ = Sha256(kStaticKeyFactory);
  VLOG(1) << "system key: " << HexEncode(system_key_);
  return RESULT_SUCCESS;
}

result_code EncryptionKey::SetTpmSystemKey() {
  // By default, do not allow migration.
  migration_allowed_ = false;

  brillo::SecureBlob key(DIGEST_LENGTH);
  int migrate = 0;
  result_code rc = get_nvram_key(tpm_, key.data(), &migrate);
  migration_allowed_ = migrate;

  if (rc == RESULT_SUCCESS) {
    system_key_ = std::move(key);
    LOG(INFO) << "Using NVRAM as system key; already populated"
              << (migration_allowed_ ? " (legacy)" : "");
  } else {
    LOG(INFO) << "Using NVRAM as system key; finalization needed.";
  }

  return rc;
}

result_code EncryptionKey::SetInsecureFallbackSystemKey() {
  migration_allowed_ = false;

  system_key_ = GetKeyFromKernelCmdline();
  if (!system_key_.empty()) {
    LOG(INFO) << "Using kernel command line argument as system key.";
    return RESULT_SUCCESS;
  }

  std::string product_uuid;
  if (base::ReadFileToStringWithMaxSize(base::FilePath(paths::kProductUUID),
                                         &product_uuid, kMaxReadSize)) {
    system_key_ = Sha256(product_uuid);
    VLOG(1) << "system key: " << HexEncode(system_key_);
    LOG(INFO) << "Using UUID as system key.";
    return RESULT_SUCCESS;
  }

  LOG(INFO) << "Using default insecure system key.";
  system_key_ = Sha256(kStaticKeyDefault);
  VLOG(1) << "system key: " << HexEncode(system_key_);
  return RESULT_SUCCESS;
}

result_code EncryptionKey::LoadChromeOSSystemKey() {
  SetTpmSystemKey();
  return RESULT_SUCCESS;
}

result_code EncryptionKey::SetExternalSystemKey(
    const brillo::SecureBlob& system_key) {
  if (system_key.size() != DIGEST_LENGTH) {
    LOG(ERROR) << "Invalid key length.";
    return RESULT_FAIL_FATAL;
  }

  system_key_ = std::move(system_key);
  return RESULT_SUCCESS;
}

result_code EncryptionKey::LoadEncryptionKey() {
  if (!system_key_.empty()) {
    if (ReadKeyFile(key_path_, &encryption_key_, system_key_)) {
      // If we found a stored encryption key, we've already finished a complete
      // login and Cryptohome Finalize so migration is finished.
      migration_allowed_ = false;
      return RESULT_SUCCESS;
    } else {
      LOG(INFO) << "Failed to load encryption key from disk.";
    }
  } else {
    LOG(INFO) << "No usable system key found.";
  }

  // Delete any stale encryption key files from disk. This is important because
  // presence of the key file determines whether finalization requests from
  // cryptohome do need to write a key file.
  base::DeleteFile(key_path_, false /* recursive */);
  encryption_key_.clear();

  // Check if there's a to-be-finalized key on disk.
  if (!ReadKeyFile(needs_finalization_path_, &encryption_key_,
                   GetUselessKey())) {
    // This is a brand new system with no keys, so generate a fresh one.
    LOG(INFO) << "Generating new encryption key.";
    encryption_key_.resize(DIGEST_LENGTH);
    cryptohome::CryptoLib::GetSecureRandom(encryption_key_.data(),
                                           encryption_key_.size());
    is_fresh_ = true;
  } else {
    LOG(ERROR) << "Finalization unfinished! Encryption key still on disk!";
  }

  // At this point, we have an encryption key but it has not been finalized yet
  // (i.e. encrypted under the system key and stored on disk in the key file).
  //
  // However, when we are creating the encrypted mount for the first time, the
  // TPM might not be in a state where we have a system key. In this case we
  // fall back to writing the obfuscated encryption key to disk (*sigh*).
  //
  // NB: We'd ideally never write an insufficiently protected key to disk. This
  // is already the case for TPM 2.0 devices as they can create system keys as
  // needed, and we can improve the situation for TPM 1.2 devices as well by (1)
  // using an NVRAM space that doesn't get lost on TPM clear and (2) allowing
  // mount-encrypted to take ownership and create the NVRAM space if necessary.
  if (system_key_.empty()) {
    if (is_fresh_) {
      LOG(INFO) << "Writing finalization intent " << needs_finalization_path_;
      if (!WriteKeyFile(needs_finalization_path_, encryption_key_,
                        GetUselessKey())) {
        LOG(ERROR) << "Failed to write " << needs_finalization_path_;
      }
    }
    return RESULT_SUCCESS;
  }

  // We have a system key, so finalize now.
  Finalize();

  return RESULT_SUCCESS;
}

void EncryptionKey::PersistEncryptionKey(
    const brillo::SecureBlob& encryption_key) {
  encryption_key_ = encryption_key;
  base::DeleteFile(key_path_, false /* recursive */);
  Finalize();
}

void EncryptionKey::Finalize() {
  CHECK(!system_key_.empty());
  CHECK(!encryption_key_.empty());

  LOG(INFO) << "Writing keyfile " << key_path_;
  if (!WriteKeyFile(key_path_, encryption_key_, system_key_)) {
    LOG(ERROR) << "Failed to write " << key_path_;
    return;
  }

  // Finalization is complete at this point.
  did_finalize_ = true;

  // Make a best effort attempt to wipe the obfuscated key file from disk. This
  // is unreliable on many levels, in particular ext4 doesn't support secure
  // delete so the data may end up sticking around in the journal. Furthermore,
  // SSDs may remap flash blocks on write, so the data may physically remain in
  // the old block. See comment above regarding options to get rid of the
  // finalization intent file in the long run.
  if (base::PathExists(needs_finalization_path_)) {
    shred(needs_finalization_path_.value().c_str());
    base::DeleteFile(needs_finalization_path_, false /* recursive */);
  }
}
