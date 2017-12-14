// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "priv_code_verifier.h"  // NOLINT - TODO(b/32971714): fix it properly.

#include <fcntl.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>
#include <base/timer/elapsed_timer.h>
#include <chromeos/libminijail.h>
#include <crypto/secure_hash.h>
#include <crypto/sha2.h>

#include "arc_setup_util.h"  // NOLINT - TODO(b/32971714): fix it properly.
#include "art_container.h"   // NOLINT - TODO(b/32971714): fix it properly.

// Ported from Android art/runtime/base/macros.h.
#define PACKED(x) __attribute__((__aligned__(x), __packed__))

namespace arc {

namespace {

// Must be synced with <android_src>/art/runtime/digest.cc.
constexpr size_t kAlgorithmSha256DigestFilesize = 68;
constexpr char kMultiHashSha256Prefix[] = "1220";
constexpr size_t kBufferSize = 4096;
constexpr char kArtFilePrefix[] = "system@framework@";
constexpr char kDigestFileExtension[] = ".digest";
constexpr char kSignatureFilename[] = "digest.sig";
constexpr base::TimeDelta kCryptohomedTimeout = base::TimeDelta::FromSeconds(3);

// Converts to digest to string format.
std::string ToHex(const uint8_t* digest, size_t len) {
  std::string hex_str_upper_case = base::HexEncode(digest, len);
  return base::ToLowerASCII(hex_str_upper_case);
}

// Calculate the digest of a file.
bool CalculateSha256Digest(const base::FilePath& file_path,
                           std::string* digest_str) {
  std::unique_ptr<crypto::SecureHash> sha256(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  std::vector<uint8_t> digest(crypto::kSHA256Length);

  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return false;

  std::vector<uint8_t> buffer(kBufferSize);
  while (true) {
    int bytes_read = file.ReadAtCurrentPos(
        reinterpret_cast<char*>(buffer.data()), buffer.size());
    if (bytes_read < 0)
      return false;

    if (bytes_read == 0)
      break;
    sha256->Update(buffer.data(), bytes_read);
  }
  sha256->Finish(digest.data(), digest.size());
  std::string hex_digest = ToHex(digest.data(), digest.size());
  // Use multihash format:  https://github.com/multiformats/multihash
  *digest_str = kMultiHashSha256Prefix + hex_digest;

  return true;
}

// Calculate the digest of all digest files.
bool GenerateHashList(const base::FilePath& cache_directory,
                      const std::vector<std::string>& art_file_list,
                      uint8_t* out_digest) {
  SHA256_CTX sha256_ctx;
  if (!SHA256_Init(&sha256_ctx)) {
    LOG(ERROR) << "Failed to initialize SHA256_CTX";
    return false;
  }
  for (const std::string& art_filename : art_file_list) {
    std::string digest_filename = art_filename + kDigestFileExtension;
    base::FilePath digest_filepath = cache_directory.Append(digest_filename);
    std::string digest;
    if (!base::ReadFileToStringWithMaxSize(digest_filepath, &digest,
                                           kAlgorithmSha256DigestFilesize)) {
      PLOG(ERROR) << "Failed to read digest file " << digest_filename;
      return false;
    }

    // Validates digest of art files before merging its digest.
    const base::FilePath art_filepath = cache_directory.Append(art_filename);
    std::string calculated_digest;
    if (!(CalculateSha256Digest(art_filepath, &calculated_digest))) {
      PLOG(ERROR) << "Cannot validate the digest for file: " << art_filename;
      return false;
    }
    if (calculated_digest != digest)
      return false;

    if (!SHA256_Update(&sha256_ctx, digest.c_str(),
                       kAlgorithmSha256DigestFilesize)) {
      LOG(ERROR) << "Failed to call SHA256_Update";
      return false;
    }
  }
  if (!SHA256_Final(out_digest, &sha256_ctx)) {
    LOG(ERROR) << "Failed to call SHA256_Final";
    return false;
  }
  return true;
}

// Partial image header to read the checksum. Need to be sync'ed with
// <Android>/art/runtime/image.h.
// TODO(xzhou): Check if P uses the same layout.
struct PACKED(4) ImagePartialHeader {
  uint8_t image_magic[4];
  uint8_t image_version[4];
  uint32_t image_begin;
  uint32_t image_size;
  uint32_t oat_checksum;
};

bool ReadImageChecksums(const std::string& image_filename,
                        uint32_t* out_oat_checksum) {
  base::ScopedFD scoped_fd(open(image_filename.c_str(), O_RDONLY));
  if (!scoped_fd.is_valid()) {
    LOG(ERROR) << "Failed to open image file: " << image_filename;
    return false;
  }
  ImagePartialHeader h;
  if (!base::ReadFromFD(scoped_fd.get(), reinterpret_cast<char*>(&h),
                        sizeof(ImagePartialHeader))) {
    PLOG(ERROR) << "Failed to read image header from " << image_filename;
    return false;
  }
  *out_oat_checksum = h.oat_checksum;
  return true;
}

bool ChecksumsMatch(const std::string& image_a, const std::string& image_b) {
  uint32_t oat_checksum_a;
  uint32_t oat_checksum_b;
  if (!ReadImageChecksums(image_a, &oat_checksum_a) ||
      !ReadImageChecksums(image_b, &oat_checksum_b)) {
    return false;
  }
  return oat_checksum_a == oat_checksum_b;
}

// Get a list of file that need to be verified.
std::vector<std::string> GetArtFileList(std::string isa) {
  std::vector<std::string> art_file_list;
  const base::FilePath isa_dir = base::FilePath(kFrameworkPath).Append(isa);
  base::FileEnumerator art_files(isa_dir, false /* recursive */,
                                 base::FileEnumerator::FILES,
                                 FILE_PATH_LITERAL("*.art"));
  for (auto src_file = art_files.Next(); !src_file.empty();
       src_file = art_files.Next()) {
    art_file_list.push_back(kArtFilePrefix + src_file.BaseName().value());
  }
  std::sort(art_file_list.begin(), art_file_list.end());
  return art_file_list;
}

// Checks whether host art files are in sync with system art files.
bool IsSynced(const base::FilePath system_isa_image_dir,
              const base::FilePath cache_isa_image_dir) {
  base::FileEnumerator art_files(system_isa_image_dir, false /* recursive */,
                                 base::FileEnumerator::FILES,
                                 FILE_PATH_LITERAL("*.art"));
  for (auto art_file = art_files.Next(); !art_file.empty();
       art_file = art_files.Next()) {
    const base::FilePath cache_art_file = cache_isa_image_dir.Append(
        kArtFilePrefix + art_file.BaseName().value());
    if (!ChecksumsMatch(art_file.value(), cache_art_file.value())) {
      LOG(ERROR) << "Art file out of sync: " << art_file.value()
                 << " != " << cache_art_file.value();
      return false;
    }
  }
  return true;
}

}  // namespace

std::unique_ptr<PrivCodeVerifier> CreatePrivCodeVerifier() {
  return std::make_unique<PrivCodeVerifier>(
      BootLockboxClient::CreateBootLockboxClient());
}

bool PrivCodeVerifier::WaitForCryptohomed() {
  base::ElapsedTimer timer;
  while (!boot_lockbox_client_->IsServiceReady()) {
    if (timer.Elapsed() > kCryptohomedTimeout) {
      LOG(ERROR) << "Giving up waiting on cryptohomed";
      return false;
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  }
  LOG(INFO) << "Waiting for cryptohomed took "
            << timer.Elapsed().InMillisecondsRoundedUp() << "ms";
  return true;
}

PrivCodeVerifier::PrivCodeVerifier(
    std::unique_ptr<BootLockboxClient> boot_lockbox_client)
    : boot_lockbox_client_(std::move(boot_lockbox_client)) {}

PrivCodeVerifier::~PrivCodeVerifier() = default;

bool PrivCodeVerifier::Verify(const base::FilePath& cache_directory,
                              const std::string& isa) {
  // Make sure the cache is synced with system partition.
  base::FilePath system_isa_image_dir =
      base::FilePath(kFrameworkPath).Append(isa);
  base::FilePath cache_isa_image_dir = cache_directory.Append(isa);
  if (!IsSynced(system_isa_image_dir, cache_isa_image_dir))
    return false;

  auto files_to_verify = GetArtFileList(isa);

  uint8_t digest_buffer[SHA256_DIGEST_LENGTH];
  if (!GenerateHashList(cache_directory.Append(isa), files_to_verify,
                        digest_buffer)) {
    LOG(ERROR) << "Failed to merge digest files";
    return false;
  }
  std::string digest(reinterpret_cast<char*>(digest_buffer),
                     SHA256_DIGEST_LENGTH);
  std::string signature;
  const base::FilePath signature_filepath =
      cache_directory.Append(isa).Append(kSignatureFilename);
  if (!ReadFileToString(signature_filepath, &signature)) {
    LOG(ERROR) << "Failed to read digest file " << signature_filepath.value();
    return false;
  }
  return boot_lockbox_client_->Verify(digest, signature);
}

bool PrivCodeVerifier::CheckCodeIntegrity(
    const base::FilePath& dalvik_cache_dir) {
  if (!IsCodeValid(dalvik_cache_dir)) {
    LOG(ERROR) << "Check Code Integrity of " << dalvik_cache_dir.value()
               << " Failed";
    return false;
  }
  return true;
}

bool PrivCodeVerifier::IsCodeValid(const base::FilePath& dalvik_cache_dir) {
  for (const std::string& isa : ArtContainer::GetIsas()) {
    // TODO(xzhou): merge the digest for all isas and call Sign() just once.
    if (!Verify(dalvik_cache_dir, isa))
      return false;
  }
  return true;
}

bool PrivCodeVerifier::Sign(const base::FilePath& directory) {
  for (const std::string& isa : ArtContainer::GetIsas()) {
    const base::FilePath isa_dir = directory.Append(isa);
    auto files_to_verify = GetArtFileList(isa);
    uint8_t digest_buffer[SHA256_DIGEST_LENGTH];
    if (!GenerateHashList(isa_dir, files_to_verify, digest_buffer)) {
      LOG(ERROR) << "Failed to merge digest files";
      return false;
    }
    std::string digest(reinterpret_cast<char*>(digest_buffer),
                       SHA256_DIGEST_LENGTH);
    std::string signature;
    boot_lockbox_client_->Sign(digest, &signature);
    const base::FilePath signature_filepath =
        isa_dir.Append(kSignatureFilename);
    if (!WriteToFile(signature_filepath, 0644, signature)) {
      LOG(ERROR) << "Failed to write signature file";
      return false;
    }
  }
  return true;
}

bool PrivCodeVerifier::IsTpmReady() {
  return boot_lockbox_client_->IsTpmReady();
}

}  // namespace arc
