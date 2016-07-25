// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloader_impl.h"

#include <fcntl.h>
#include <linux/loop.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sstream>
#include <string>
#include <utility>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/important_file_writer.h>
#include <base/files/scoped_file.h>
#include <base/guid.h>
#include <base/json/json_string_value_serializer.h>
#include <base/logging.h>
#include <base/numerics/safe_conversions.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/version.h>
#include <brillo/flag_helper.h>
#include <crypto/secure_hash.h>
#include <crypto/sha2.h>
#include <crypto/signature_verifier.h>
#include <dbus-c++/error.h>

#include "imageloader_common.h"

namespace imageloader {

namespace {

using imageloader::kBadResult;

// The name of the fingerprint file.
const char kFingerprintName[] = "manifest.fingerprint";
// The name of the imageloader manifest file.
const char kManifestName[] = "imageloader.json";
// The manifest signature.
const char kManifestSignatureName[] = "imageloader.sig.1";
// The current version of the hints file.
const int kCurrentManifestVersion = 1;
// The name of the version field in the manifest.
const char kManifestVersionField[] = "manifest-version";
// The name of the component version field in the manifest.
const char kVersionField[] = "version";
// The name of the field containing the image hash.
const char kImageHashField[] = "image-sha256-hash";
// The name of the image file.
const char kImageFileName[] = "image.squash";
// The name of the field containing the parameters hash.
const char kParamsHashField[] = "params-sha256-hash";
// The name of the params file.
const char kParamsFileName[] = "params";
// The permissions that the component update directory must use.
const int kComponentDirPerms = 0755;
// The permissions that files in the component should have.
const int kComponentFilePerms = 0644;
// The maximum size of any file to read into memory.
const size_t kMaximumFilesize = 4096 * 10;

bool AssertComponentDirPerms(const base::FilePath& path) {
  int mode;
  if (!GetPosixFilePermissions(path, &mode)) return false;
  return mode == kComponentDirPerms;
}

bool GetSHA256FromString(const std::string& hash_str,
                         std::vector<uint8_t>* bytes) {
  if (!base::HexStringToBytes(hash_str, bytes)) return false;
  return bytes->size() == crypto::kSHA256Length;
}

bool WriteFileToDisk(const base::FilePath& path, const std::string& contents) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(),
                                      O_CREAT | O_WRONLY | O_EXCL,
                                      kComponentFilePerms)));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Error creating file for " << path.value();
    return false;
  }

  base::File file(fd.release());
  int size = base::checked_cast<int>(contents.size());
  return file.Write(0, contents.data(), contents.size()) == size;
}

}  // namespace {}

// The client inserts manifest.fingerprint into components after unpacking the
// CRX. The file is used for delta updates. Since Chrome OS doesn't rely on it
// for security of the disk image, we are fine with sanity checking the contents
// and then preserving the unsigned file.
bool ImageLoaderImpl::IsValidFingerprintFile(const std::string& contents) {
  return contents.size() <= 256 &&
         std::find_if_not(contents.begin(), contents.end(), [](char ch) {
           return base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch) || ch == '.';
         }) == contents.end();
}

bool ImageLoaderImpl::CopyFingerprintFile(const base::FilePath& src,
                                      const base::FilePath& dest) {
  base::FilePath fingerprint_path = src.Append(kFingerprintName);
  if (base::PathExists(fingerprint_path)) {
    std::string fingerprint_contents;
    if (!base::ReadFileToStringWithMaxSize(
            fingerprint_path, &fingerprint_contents, kMaximumFilesize)) {
      return false;
    }

    if (!IsValidFingerprintFile(fingerprint_contents)) return false;

    if (!WriteFileToDisk(dest.Append(kFingerprintName), fingerprint_contents)) {
      return false;
    }
  }
  return true;
}

bool ImageLoaderImpl::CopyAndHashFile(
    const base::FilePath& src_path, const base::FilePath& dest_path,
    const std::vector<uint8_t>& expected_hash) {
  base::File file(src_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) return false;

  base::ScopedFD dest(
      HANDLE_EINTR(open(dest_path.value().c_str(), O_CREAT | O_WRONLY | O_EXCL,
                        kComponentFilePerms)));
  if (!dest.is_valid()) return false;

  base::File out_file(dest.release());
  std::unique_ptr<crypto::SecureHash> sha256(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  int64_t size = file.GetLength();
  if (size < 0) return false;

  size_t bytes_read = 0;
  int rv = 0;
  char buf[4096];
  do {
    int remaining = size - bytes_read;
    int bytes_to_read =
        std::min(remaining, base::checked_cast<int>(sizeof(buf)));

    rv = file.ReadNoBestEffort(bytes_read, buf, bytes_to_read);

    if (rv <= 0) break;

    bytes_read += rv;
    sha256->Update(buf, rv);

    if (out_file.WriteAtCurrentPos(buf, rv) != rv) return false;
  } while (bytes_read <= size);

  if (bytes_read != size) return false;

  std::vector<uint8_t> file_hash(crypto::kSHA256Length);
  sha256->Finish(file_hash.data(), file_hash.size());

  if (expected_hash != file_hash) {
    LOG(ERROR) << "Image is corrupt or modified.";
    return false;
  }
  return true;
}

bool ImageLoaderImpl::VerifyAndParseManifest(const std::string& manifest_contents,
                                         const std::string& signature,
                                         Manifest* manifest) {
  // Verify the manifest before trusting any of its contents.
  if (!ECVerify(manifest_contents, signature)) {
    LOG(INFO) << "Manifest did not pass signature verification.";
    return false;
  }

  // Now deserialize the manifest json and read out the rest of the component.
  int error_code;
  std::string error_message;
  JSONStringValueDeserializer deserializer(manifest_contents);
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_message);

  if (!value) {
    LOG(ERROR) << "Could not deserialize the manifest file. Error "
               << error_code << ": " << error_message;
    return false;
  }

  base::DictionaryValue* manifest_dict = nullptr;
  if (!value->GetAsDictionary(&manifest_dict)) {
    LOG(ERROR) << "Could not parse manifest file as JSON.";
    return false;
  }

  // This will have to be changed if the manifest version is bumped.
  int version;
  if (!manifest_dict->GetInteger(kManifestVersionField, &version)) {
    LOG(ERROR) << "Could not parse manifest version field from manifest.";
    return false;
  }
  if (version != kCurrentManifestVersion) {
    LOG(ERROR) << "Unsupported version of the manifest.";
    return false;
  }
  manifest->manifest_version = version;

  std::string image_hash_str;
  if (!manifest_dict->GetString(kImageHashField, &image_hash_str)) {
    LOG(ERROR) << "Could not parse image hash from manifest.";
    return false;
  }

  if (!GetSHA256FromString(image_hash_str, &(manifest->image_sha256))) {
    LOG(ERROR) << "Could not convert image hash to bytes.";
    return false;
  }

  std::string params_hash_str;
  if (!manifest_dict->GetString(kParamsHashField, &params_hash_str)) {
    LOG(ERROR) << "Could not parse parameters hash from manifest.";
    return false;
  }

  if (!GetSHA256FromString(params_hash_str, &(manifest->params_sha256))) {
    LOG(ERROR) << "Could not convert params hash to bytes.";
    return false;
  }

  if (!manifest_dict->GetString(kVersionField, &(manifest->version))) {
    LOG(ERROR) << "Could not parse component version from manifest.";
    return false;
  }

  return true;
}

bool ImageLoaderImpl::CopyComponentDirectory(
    const base::FilePath& component_path,
    const base::FilePath& destination_folder, const std::string& version) {
  if (mkdir(destination_folder.value().c_str(), kComponentDirPerms) != 0) {
    PLOG(ERROR) << "Failed to create directory " << destination_folder.value();
    return false;
  }

  // Load in the manifest.
  std::string manifest_contents;
  if (!base::ReadFileToStringWithMaxSize(component_path.Append(kManifestName),
                                         &manifest_contents,
                                         kMaximumFilesize)) {
    return false;
  }

  // Read in the manifest signature.
  std::string manifest_sig;
  base::FilePath manifest_sig_path =
      component_path.Append(kManifestSignatureName);
  if (!base::ReadFileToStringWithMaxSize(manifest_sig_path, &manifest_sig,
                                         kMaximumFilesize)) {
    return false;
  }

  Manifest manifest;
  if (!VerifyAndParseManifest(manifest_contents, manifest_sig, &manifest)) {
    LOG(ERROR) << "Could not verify and parse the manifest.";
    return false;
  }

  if (manifest.version != version) {
    LOG(ERROR) << "The client provided a different component version than the "
                  "manifest.";
    return false;
  }

  // Now write them both out to disk.
  if (!WriteFileToDisk(destination_folder.Append(kManifestName),
                       manifest_contents) ||
      !WriteFileToDisk(destination_folder.Append(kManifestSignatureName),
                       manifest_sig)) {
    return false;
  }

  base::FilePath params_src = component_path.Append(kParamsFileName);
  base::FilePath params_dest = destination_folder.Append(kParamsFileName);
  if (!CopyAndHashFile(params_src, params_dest, manifest.params_sha256)) {
    LOG(ERROR) << "Could not copy params file.";
    return false;
  }

  base::FilePath image_src = component_path.Append(kImageFileName);
  base::FilePath image_dest = destination_folder.Append(kImageFileName);
  if (!CopyAndHashFile(image_src, image_dest, manifest.image_sha256)) {
    LOG(ERROR) << "Could not copy image file.";
    return false;
  }

  if (!CopyFingerprintFile(component_path, destination_folder)) {
    LOG(ERROR) << "Could not copy manifest.fingerprint file.";
    return false;
  }

  return true;
}

bool ImageLoaderImpl::ECVerify(const base::StringPiece data,
                                  const base::StringPiece sig) {
  crypto::SignatureVerifier verifier;

  if (!verifier.VerifyInit(
          crypto::SignatureVerifier::ECDSA_SHA256,
          reinterpret_cast<const uint8_t*>(sig.data()),
          base::checked_cast<int>(sig.size()),
          config_.key.data(),
          base::checked_cast<int>(config_.key.size()))) {
    return false;
  }

  verifier.VerifyUpdate(reinterpret_cast<const uint8_t*>(data.data()),
                        base::checked_cast<int>(data.size()));

  return verifier.VerifyFinal();
}

// Mount component at location generated.
std::string ImageLoaderImpl::LoadComponentUtil(const std::string& name) {
  // Not implemented yet.
  return kBadResult;
}

bool ImageLoaderImpl::RegisterComponent(
    const std::string& name, const std::string& version,
    const std::string& component_folder_abs_path) {
  base::FilePath components_dir(config_.storage_dir);
  if (!base::PathExists(components_dir)) {
    if (mkdir(components_dir.value().c_str(), kComponentDirPerms) != 0) {
      PLOG(ERROR) << "Could not create the ImageLoader components directory.";
      return false;
    }
  }

  if (!AssertComponentDirPerms(config_.storage_dir)) return false;

  base::FilePath component_root = components_dir.Append(name);

  std::string current_version_hint;
  base::FilePath version_hint_path = component_root.Append(name);
  bool was_previous_version = base::PathExists(version_hint_path);
  if (was_previous_version) {
    if (!base::ReadFileToStringWithMaxSize(
            version_hint_path, &current_version_hint, kMaximumFilesize)) {
      return false;
    }

    // Check for version rollback. We trust the version from the directory name
    // because it had to be validated to ever be registered.
    base::Version current_version(current_version_hint);
    base::Version new_version(version);
    if (!current_version.IsValid() || !new_version.IsValid()) {
      return false;
    }

    if (new_version <= current_version) {
      LOG(ERROR) << "Version [" << new_version << "] is not newer than ["
                 << current_version << "] for component [" << name
                 << "] and cannot be registered.";
      return false;
    }
  }

  // Check if this specific component already exists in the filesystem.
  if (!base::PathExists(component_root)) {
    if (mkdir(component_root.value().c_str(), kComponentDirPerms) != 0) {
      PLOG(ERROR) << "Could not create component specific directory.";
      return false;
    }
  }

  // Take ownership of the component and verify it.
  base::FilePath version_path = component_root.Append(version);
  base::FilePath folder_path(component_folder_abs_path);
  if (!CopyComponentDirectory(folder_path, version_path, version)) {
    base::DeleteFile(version_path, /*recursive=*/true);
    return false;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(version_hint_path,
                                                      version)) {
    base::DeleteFile(version_path, /*recursive=*/true);
    LOG(ERROR) << "Failed to update current version hint file.";
    return false;
  }

  // Now delete the old component version, if there was one.
  if (was_previous_version) {
    base::DeleteFile(component_root.Append(current_version_hint), true);
  }

  return true;
}

std::string ImageLoaderImpl::GetComponentVersion(const std::string& name) {
  base::FilePath component_path =
      base::FilePath(config_.storage_dir).Append(name);
  if (!base::PathExists(component_path)) {
    LOG(ERROR) << "Component does not exist.";
    return kBadResult;
  }

  std::string current_version_hint;
  base::FilePath version_hint_path = component_path.Append(name);
  if (!base::ReadFileToStringWithMaxSize(
          version_hint_path, &current_version_hint, kMaximumFilesize)) {
    return kBadResult;
  }

  base::FilePath versioned_path = component_path.Append(current_version_hint);
  // The version can be security sensitive (i.e. which flash player does Chrome
  // load), so check the signed manfiest as the final answer.
  std::string manifest_contents;
  if (!base::ReadFileToStringWithMaxSize(versioned_path.Append(kManifestName),
                                         &manifest_contents,
                                         kMaximumFilesize)) {
    return kBadResult;
  }

  // Read in the manifest signature.
  std::string manifest_sig;
  base::FilePath manifest_sig_path =
      versioned_path.Append(kManifestSignatureName);
  if (!base::ReadFileToStringWithMaxSize(manifest_sig_path, &manifest_sig,
                                         kMaximumFilesize)) {
    return kBadResult;
  }

  Manifest manifest;
  if (!VerifyAndParseManifest(manifest_contents, manifest_sig, &manifest)) {
    LOG(ERROR) << "Could not verify and parse the manifest.";
    return kBadResult;
  }

  return manifest.version == current_version_hint ? manifest.version
                                                  : kBadResult;
}

std::string ImageLoaderImpl::LoadComponent(const std::string& name) {
  std::string mount_point = LoadComponentUtil(name);
  if (mount_point == kBadResult) {
    LOG(ERROR) << "Unable to mount : " << mount_point;
    return kBadResult;
  }
  LOG(INFO) << "Mounted successfully at " << mount_point << ".";
  return mount_point;
}

}  // namespace imageloader
