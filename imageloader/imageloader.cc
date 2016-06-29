// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloader.h"

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
#include <base/files/scoped_file.h>
#include <base/guid.h>
#include <base/json/json_string_value_serializer.h>
#include <base/logging.h>
#include <base/numerics/safe_conversions.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <brillo/flag_helper.h>
#include <crypto/secure_hash.h>
#include <crypto/sha2.h>
#include <crypto/signature_verifier.h>
#include <dbus-c++/error.h>

#include "imageloader_common.h"

namespace imageloader {

namespace {

using imageloader::kBadResult;

// Generate a good enough (unique) mount point.
base::FilePath GenerateMountPoint(const char prefix[]) {
  return base::FilePath(prefix + base::GenerateGUID());
}

// The path where the components are stored on the device.
const char kComponentsPath[] = "/mnt/stateful_partition/encrypted/imageloader";
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

// TODO(kerrnel): Switch to the prod keys before shipping this feature.
const uint8_t kDevPublicKey[] = {
    0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00,
    0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xea, 0x07, 0x7b,
    0x40, 0xba, 0x3d, 0x9f, 0x3c, 0x1c, 0xf1, 0xb0, 0x52, 0xd3, 0x51, 0xa2,
    0xa4, 0x0f, 0x24, 0xee, 0x5a, 0xa2, 0x7f, 0x38, 0xa5, 0xc0, 0x23, 0xb0,
    0x33, 0x60, 0xf1, 0xd4, 0xb3, 0xc8, 0xb1, 0x2e, 0xe0, 0xaa, 0x42, 0xca,
    0x47, 0x80, 0x5f, 0x44, 0x7d, 0x31, 0x46, 0xa9, 0x86, 0xaa, 0x01, 0x63,
    0x88, 0x85, 0xfd, 0x0b, 0xe3, 0xe6, 0x77, 0x11, 0x93, 0x3d, 0x71, 0x4b,
    0xec, 0xff, 0x8d, 0x0c, 0x09, 0xbf, 0x41, 0x7d, 0xa7, 0x79, 0x85, 0xa3,
    0x89, 0xf8, 0xc7, 0x32, 0x91, 0xd6, 0x2e, 0xe1, 0x75, 0xdb, 0x33, 0x72,
    0xe7, 0xd8, 0xcc, 0xbf, 0xa1, 0xd2, 0x39, 0x44, 0xf4, 0x6c, 0x8e, 0x31,
    0xfd, 0xab, 0xe9, 0xc1, 0x3d, 0xf9, 0xac, 0xcd, 0x04, 0xb7, 0xcf, 0x0c,
    0x9e, 0x18, 0xf3, 0x4e, 0x4d, 0x5e, 0x8b, 0x31, 0x92, 0x48, 0xcf, 0x96,
    0x5d, 0x90, 0xf0, 0xaf, 0xe4, 0xb7, 0x31, 0x20, 0xa6, 0xe4, 0x1b, 0xce,
    0xc4, 0x89, 0x37, 0xea, 0x8c, 0xc4, 0xfd, 0x8c, 0xe9, 0x33, 0x35, 0xd7,
    0xa8, 0x1f, 0x91, 0x34, 0xdc, 0xab, 0x3e, 0xf0, 0xcb, 0x1b, 0x65, 0x96,
    0x24, 0x79, 0x1c, 0x0d, 0x24, 0xb3, 0x97, 0x08, 0x93, 0x7b, 0x89, 0x9d,
    0xe6, 0x5f, 0x45, 0x58, 0x2a, 0xb4, 0x49, 0x2b, 0x26, 0x95, 0x7a, 0xa5,
    0xe4, 0x16, 0xfe, 0xec, 0xb0, 0xeb, 0x5a, 0xa8, 0x28, 0xf3, 0x35, 0xab,
    0x10, 0x0a, 0xc4, 0xf0, 0x32, 0x30, 0xf6, 0x38, 0x5d, 0x04, 0xff, 0xe6,
    0x10, 0xf5, 0x2a, 0xc1, 0x13, 0x9e, 0xd4, 0x5a, 0x26, 0xdf, 0xf1, 0xaa,
    0xd1, 0x7d, 0x16, 0x01, 0x23, 0xd6, 0x2d, 0xc4, 0xf4, 0xb0, 0x60, 0xf1,
    0x26, 0xfe, 0x29, 0xaa, 0xb7, 0x34, 0xd9, 0x5b, 0xd3, 0xcf, 0x97, 0x21,
    0x78, 0x1a, 0xa0, 0x0a, 0x93, 0x6e, 0x4b, 0x6c, 0xb5, 0xcc, 0xce, 0x66,
    0x57, 0x02, 0x03, 0x01, 0x00, 0x01};

bool AssertComponentDirPerms() {
  int mode;
  base::FilePath components_dir(kComponentsPath);
  if (!GetPosixFilePermissions(components_dir, &mode)) return false;
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
// static
bool ImageLoader::IsValidFingerprintFile(const std::string& contents) {
  return contents.size() <= 256 &&
         std::find_if_not(contents.begin(), contents.end(), [](char ch) {
           return base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch) || ch == '.';
         }) == contents.end();
}

// static
bool ImageLoader::CopyFingerprintFile(const base::FilePath& src,
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

bool ImageLoader::CopyAndHashFile(const base::FilePath& src_path,
                                  const base::FilePath& dest_path,
                                  const std::vector<uint8_t>& expected_hash) {
  base::File file(src_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) return false;

  base::ScopedFD dest(HANDLE_EINTR(open(dest_path.value().c_str(),
                                        O_CREAT | O_WRONLY | O_EXCL,
                                        kComponentFilePerms)));
  if (!dest.is_valid()) return false;

  base::File out_file(dest.release());
  std::unique_ptr<crypto::SecureHash> sha256(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  int64_t size = file.GetLength();
  if (size < 0) return false;

  size_t bytes_read = 0;
  int rv = 0;
  char  buf[4096];
  do {
    int remaining = size - bytes_read;
    int bytes_to_read =
        std::min(remaining, base::checked_cast<int>(sizeof(buf)));

    rv = file.ReadNoBestEffort(bytes_read, buf, bytes_to_read);

    if (rv <= 0) break;

    bytes_read += rv;
    sha256->Update(buf, rv);

    if (out_file.WriteAtCurrentPos(buf, rv) != rv)
      return false;
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

// static
bool ImageLoader::VerifyAndParseManifest(const std::string& manifest_contents,
                                         const std::string& signature,
                                         Manifest* manifest) {
  // Verify the manifest before trusting any of its contents.
  if (!RsaVerify(manifest_contents, signature)) {
    LOG(INFO) << "Manifest did not pass RSA verification.";
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

// static
bool ImageLoader::CopyComponentDirectory(
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

// static
bool ImageLoader::RsaVerify(const base::StringPiece data,
                            const base::StringPiece sig) {
  crypto::SignatureVerifier verifier;

  if (!verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA256,
                           reinterpret_cast<const uint8_t*>(sig.data()),
                           base::checked_cast<int>(sig.size()),
                           reinterpret_cast<const uint8_t*>(kDevPublicKey),
                           base::checked_cast<int>(sizeof(kDevPublicKey)))) {
    return false;
  }

  verifier.VerifyUpdate(reinterpret_cast<const uint8_t*>(data.data()),
                        base::checked_cast<int>(data.size()));

  return verifier.VerifyFinal();
}

// Mount component at location generated.
std::string ImageLoader::LoadComponentUtil(const std::string& name) {
  base::FilePath mount_point = GenerateMountPoint("/mnt/");
  // Is this somehow taken up by any other name or mount?
  for (auto it = mounts.begin(); it != mounts.end(); ++it) {
    if ((it->second).first == mount_point) {
      return kBadResult;
    }
  }
  if (PathExists(mount_point)) {
    LOG(INFO) << "Generated mount_point is already stat-able : "
              << mount_point.value();
    return kBadResult;
  }
  // The mount point is not yet taken, so go ahead.
  base::ScopedFD loopctl_fd(open("/dev/loop-control", O_RDONLY | O_CLOEXEC));
  if (!loopctl_fd.is_valid()) {
    PLOG(ERROR) << "loopctl_fd";
    return kBadResult;
  }
  int device_free_number = ioctl(loopctl_fd.get(), LOOP_CTL_GET_FREE);
  if (device_free_number < 0) {
    PLOG(ERROR) << "ioctl : LOOP_CTL_GET_FREE";
    return kBadResult;
  }
  std::ostringstream device_path;
  device_path << "/dev/loop" << device_free_number;
  base::ScopedFD device_path_fd(
      open(device_path.str().c_str(), O_RDONLY | O_CLOEXEC));
  if (!device_path_fd.is_valid()) {
    PLOG(ERROR) << "device_path_fd";
    return kBadResult;
  }
  base::ScopedFD fs_image_fd(
      open(reg[name].second.value().c_str(), O_RDONLY | O_CLOEXEC));
  if (!fs_image_fd.is_valid()) {
    PLOG(ERROR) << "fs_image_fd";
    return kBadResult;
  }
  if (ioctl(device_path_fd.get(), LOOP_SET_FD, fs_image_fd.get()) < 0) {
    PLOG(ERROR) << "ioctl: LOOP_SET_FD";
    return kBadResult;
  }
  if (!base::CreateDirectory(mount_point)) {
    PLOG(ERROR) << "CreateDirectory : " << mount_point.value();
    ioctl(device_path_fd.get(), LOOP_CLR_FD, 0);
    return kBadResult;
  }
  if (mount(device_path.str().c_str(), mount_point.value().c_str(), "squashfs",
            MS_RDONLY | MS_NOSUID | MS_NODEV, "") < 0) {
    PLOG(ERROR) << "mount";
    ioctl(device_path_fd.get(), LOOP_CLR_FD, 0);
    return kBadResult;
  }
  mounts[name] = std::make_pair(mount_point, base::FilePath(device_path.str()));
  return mount_point.value();
}

// Unmount the given component.
bool ImageLoader::UnloadComponentUtil(const std::string& name) {
  std::string device_path = mounts[name].second.value();
  if (umount(mounts[name].first.value().c_str()) < 0) {
    PLOG(ERROR) << "umount";
    return false;
  }
  const base::FilePath fp_mount_point(mounts[name].first);
  if (!DeleteFile(fp_mount_point, false)) {
    PLOG(ERROR) << "DeleteFile : " << fp_mount_point.value();
    return false;
  }
  base::ScopedFD device_path_fd(
      open(device_path.c_str(), O_RDONLY | O_CLOEXEC));
  if (!device_path_fd.is_valid()) {
    PLOG(ERROR) << "device_path_fd";
    return false;
  }
  if (ioctl(device_path_fd.get(), LOOP_CLR_FD, 0) < 0) {
    PLOG(ERROR) << "ioctl: LOOP_CLR_FD";
    return false;
  }
  mounts.erase(mounts.find(name));
  return true;
}

// Following functions are required directly for the DBus functionality.

ImageLoader::ImageLoader(DBus::Connection* conn)
    : DBus::ObjectAdaptor(*conn, kImageLoaderPath) {}

// TODO(kerrnel): Add version rollback protection.
// TODO(kerrnel): Do not write into the temporary storage, write the versioned
// directory directly.
// TODO(kerrnel): base::Move needs to be an atomic operation using a symlink.
bool ImageLoader::RegisterComponent(
    const std::string& name, const std::string& version,
    const std::string& component_folder_abs_path, ::DBus::Error& err) {
  base::FilePath components_dir(kComponentsPath);
  if (!base::PathExists(components_dir)) {
    if (mkdir(components_dir.value().c_str(), kComponentDirPerms) != 0) {
      return false;
    }
  }

  if (!AssertComponentDirPerms()) return false;

  // Take ownership of the component and verify it.
  base::FilePath folder_path(component_folder_abs_path);
  base::FilePath temp_path;
  if (!base::CreateNewTempDirectory("", &temp_path)) return false;
  if (!base::SetPosixFilePermissions(temp_path, kComponentDirPerms))
    return false;
  base::FilePath component_temp_path = temp_path.Append(folder_path.BaseName());
  if (!CopyComponentDirectory(folder_path, component_temp_path, version)) {
    // TODO(kerrnel): cleanup if this fails.
    return false;
  }

  base::FilePath component_folder = components_dir.Append(name);
  // TODO(kerrnel): is this the best thing to use for atomic replacement?
  return base::Move(component_temp_path, component_folder);
}

std::string ImageLoader::GetComponentVersion(const std::string& name,
                                             ::DBus::Error& err) {
  if (reg.find(name) != reg.end()) {
    LOG(INFO) << "Found entry (" << name << ", " << reg[name].first << ", "
              << reg[name].second.value() << ")";
    return reg[name].first;
  }
  LOG(ERROR) << "Entry not found : " << name;
  return kBadResult;
}

std::string ImageLoader::LoadComponent(const std::string& name,
                                       ::DBus::Error& err) {
  if (reg.find(name) != reg.end()) {
    if (mounts.find(name) != mounts.end()) {
      LOG(ERROR) << "Already mounted at " << mounts[name].first.value() << ".";
      return kBadResult;
    }
    std::string mount_point = LoadComponentUtil(name);
    if (mount_point == kBadResult) {
      LOG(ERROR) << "Unable to mount : " << mount_point;
      return kBadResult;
    }
    LOG(INFO) << "Mounted successfully at " << mount_point << ".";
    return mount_point;
  }
  LOG(ERROR) << "Entry not found : " << name;
  return kBadResult;
}

bool ImageLoader::UnloadComponent(const std::string& name, ::DBus::Error& err) {
  if (UnloadComponentUtil(name)) {
    LOG(INFO) << "Unmount " << name << " successful.";
    return true;
  }
  LOG(ERROR) << "Unmount " << name << " unsucessful.";
  return false;
}

}  // namespace imageloader
