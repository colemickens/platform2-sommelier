// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloader_impl.h"

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include </usr/include/linux/magic.h>

#include <sstream>
#include <string>
#include <utility>

#include <base/command_line.h>
#include <base/containers/adapters.h>
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
// The current version of the manifest file.
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
// The name of the file containing the latest component version.
const char kLatestVersionFile[] = "latest-version";
// The name of the params file.
const char kParamsFileName[] = "params";
// The permissions that the component update directory must use.
const int kComponentDirPerms = 0755;
// The permissions that files in the component should have.
const int kComponentFilePerms = 0644;
// The maximum size of any file to read into memory.
const size_t kMaximumFilesize = 4096 * 10;

// TODO(kerrnel): A component should be abstracted into a class that hides the
// disk structure.
// A component is a directory on disk with a hierarchy of files inside of it. It
// looks like this for an example component, "PepperFlashPlayer":
// PepperFlashPlayer/
//  - latest_version # a file containing the latest version, in this case
//                   # (22.0.0.158)
//  - 22.0.0.158/    # the folder containing the actual component.
//    - imageloader.json  # a manifest file containing the hashes of all the
//                        # files, and some other metadata.
//    - imageloader.sig.1 # a signature blob of the imageloader.json manifest.
//    - params            # contains the parameters (including merkle tree root
//                        # hash), of the dm-verity image.
//    - image.squash      # The squashfs disk image containing the actual
//                        # component files. The dm-verity hash tree is appended
//                        # to the end of the disk image, and the |params| tell
//                        # the kernel the offset of the tree.

// Functions to generate paths to various files in the components.
base::FilePath GetComponentPath(const base::FilePath& storage_root,
                                const std::string& component_name) {
  return storage_root.Append(component_name);
}

// |component_root_path| is the "PepperFlashPlayer" folder in the above example.
base::FilePath GetVersionFilePath(const base::FilePath& component_root_path) {
  return component_root_path.Append(kLatestVersionFile);
}

base::FilePath GetVersionedPath(const base::FilePath& component_root_path,
                                const std::string& current_version) {
  return component_root_path.Append(current_version);
}

// |component_version_path| must refer to an actual version of the component,
// such as "22.0.0.158" in the above example.
base::FilePath GetManifestPath(const base::FilePath& component_version_path) {
  return component_version_path.Append(kManifestName);
}

base::FilePath GetSignaturePath(const base::FilePath& component_version_path) {
  return component_version_path.Append(kManifestSignatureName);
}

base::FilePath GetFingerprintPath(
    const base::FilePath& component_version_path) {
  return component_version_path.Append(kFingerprintName);
}

base::FilePath GetParamsPath(const base::FilePath& component_version_path) {
  return component_version_path.Append(kParamsFileName);
}

base::FilePath GetImagePath(const base::FilePath& component_version_path) {
  return component_version_path.Append(kImageFileName);
}

// |mount_base_path| is the subfolder where all components are mounted.
// For example "/mnt/imageloader."
base::FilePath GetMountPoint(const base::FilePath& mount_base_path,
                             const std::string& component_name,
                             const std::string& component_version) {
  return mount_base_path.Append(component_name).Append(component_version);
}

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

bool CreateDirectoryWithMode(const base::FilePath& full_path, int mode) {
  std::vector<base::FilePath> subpaths;

  // Collect a list of all parent directories.
  base::FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (base::FilePath path = full_path.DirName();
       path.value() != last_path.value(); path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (const auto& subpath : base::Reversed(subpaths)) {
    if (base::DirectoryExists(subpath)) continue;
    if (mkdir(subpath.value().c_str(), mode) == 0) continue;
    // Mkdir failed, but it might have failed with EEXIST, or some other error
    // due to the the directory appearing out of thin air. This can occur if
    // two processes are trying to create the same file system tree at the same
    // time. Check to see if it exists and make sure it is a directory.
    if (!base::DirectoryExists(subpath)) {
      PLOG(ERROR) << "Failed to create directory";
      return false;
    }
  }
  return true;
}

bool GetAndVerifyParams(const base::FilePath& path,
                        const std::vector<uint8_t>& hash,
                        std::string* out_params) {
  std::string params;
  if (!base::ReadFileToStringWithMaxSize(path, &params, kMaximumFilesize)) {
    return false;
  }

  std::vector<uint8_t> params_hash(crypto::kSHA256Length);
  crypto::SHA256HashString(params, params_hash.data(), params_hash.size());
  if (params_hash != hash) {
    LOG(ERROR) << "dm-verity parameters file has the wrong hash.";
    return false;
  }

  out_params->assign(params);
  return true;
}

// |callback| is repeatedly called on data read from the file, can return false on
// error to terminate the read early.
bool ReadFileWithCallback(base::File* file,
                          std::function<bool(const char*, int)> callback) {
  int size = file->GetLength();
  if (size <= 0) return false;

  int rv = 0, bytes_read = 0;
  char buf[4096];
  do {
    int remaining = size - bytes_read;
    int bytes_to_read =
        std::min(remaining, base::checked_cast<int>(sizeof(buf)));

    rv = file->ReadAtCurrentPos(buf, bytes_to_read);
    if (rv <= 0) break;

    bytes_read += rv;
    if (!callback(buf, rv)) return false;
  } while (bytes_read <= size);

  return bytes_read == size;
}

bool GetAndVerifyImage(const base::FilePath& path,
                       const std::vector<uint8_t>& hash,
                       base::ScopedFD* fd) {
  base::File image(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!image.IsValid()) {
    LOG(ERROR) << "Could not open image file.";
    return false;
  }

  std::unique_ptr<crypto::SecureHash> sha256(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  if (!ReadFileWithCallback(&image, [&sha256](const char* buf, int length) {
        sha256->Update(buf, length);
        return true;
      })) {
    LOG(ERROR) << "Failed to read image file.";
    return false;
  }

  std::vector<uint8_t> file_hash(crypto::kSHA256Length);
  sha256->Finish(file_hash.data(), file_hash.size());

  if (hash != file_hash) {
    LOG(ERROR) << "Image is corrupt or modified.";
    return false;
  }

  fd->reset(image.TakePlatformFile());
  return true;
}

bool CreateMountPointIfNeeded(const base::FilePath& mount_point,
                              bool* already_mounted) {
  *already_mounted = false;
  // Is this mount point somehow already taken?
  struct stat st;
  if (lstat(mount_point.value().c_str(), &st) == 0) {
    if (!S_ISDIR(st.st_mode)) {
      LOG(ERROR) << "Mount point exists but is not a directory.";
      return false;
    }

    base::FilePath mount_parent = mount_point.DirName();
    struct stat st2;
    if (stat(mount_parent.value().c_str(), &st2) != 0) {
      PLOG(ERROR) << "Could not stat the mount point parent";
      return false;
    }
    if (st.st_dev != st2.st_dev) {
      struct statfs st_fs;
      if (statfs(mount_point.value().c_str(), &st_fs) != 0) {
        PLOG(ERROR) << "statfs";
        return false;
      }
      if (st_fs.f_type != SQUASHFS_MAGIC || !(st_fs.f_flags & ST_NODEV) ||
          !(st_fs.f_flags & ST_NOSUID) || !(st_fs.f_flags & ST_RDONLY)) {
        LOG(ERROR) << "File system is not the expected type.";
        return false;
      }
      LOG(INFO) << "The mount point already exists: " << mount_point.value();
      *already_mounted = true;
      return true;
    }
  } else if (!CreateDirectoryWithMode(mount_point, kComponentDirPerms)) {
    LOG(ERROR) << "Failed to create mount point: " << mount_point.value();
    return false;
  }
  return true;
}

}  // namespace {}

bool ImageLoaderImpl::GetManifestForComponent(const std::string& name,
                                              Manifest* manifest) {
  base::FilePath component_path(GetComponentPath(config_.storage_dir, name));
  if (!base::PathExists(component_path)) {
    LOG(ERROR) << "No valid component [" << name << "] on disk.";
    return false;
  }
  return GetAndVerifyManifest(name, component_path, manifest, nullptr, nullptr);
}

bool ImageLoaderImpl::GetAndVerifyManifest(const std::string& component_name,
                                           const base::FilePath& component_path,
                                           Manifest* manifest,
                                           std::string* manifest_out,
                                           std::string* sig) {
  std::string version;
  base::FilePath versioned_path;
  if (component_name != "") {
    base::FilePath version_hint_path(GetVersionFilePath(component_path));
    if (!base::ReadFileToStringWithMaxSize(version_hint_path, &version,
                                           kMaximumFilesize)) {
      LOG(ERROR) << "Could not read current version of component ["
                 << component_name << "]";
      return false;
    }
    versioned_path = GetVersionedPath(component_path, version);
  } else {
    // If the component has not been moved into its versioned path yet, there is
    // no versioned path, and so the caller passes an empty |component_name|,
    // telling this function to work with the |component_path| directly.
    versioned_path = component_path;
  }

  std::string manifest_str;
  std::string manifest_sig;
  if (!base::ReadFileToStringWithMaxSize(GetManifestPath(versioned_path),
                                         &manifest_str, kMaximumFilesize)) {
    LOG(ERROR) << "Could not read manifest file.";
    return false;
  }
  if (!base::ReadFileToStringWithMaxSize(GetSignaturePath(versioned_path),
                                         &manifest_sig, kMaximumFilesize)) {
    LOG(ERROR) << "Could not read signature file.";
    return false;
  }

  if (!VerifyAndParseManifest(manifest_str, manifest_sig, manifest)) {
    LOG(ERROR) << "Could not verify and parse manifest file.";
    return false;
  }

  if (component_name != "" && manifest->version != version) {
    LOG(ERROR) << "Manifest version does not match hint file version.";
    return false;
  }

  if (manifest_out) manifest_out->assign(manifest_str);
  if (sig) sig->assign(manifest_sig);

  return true;
}

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
  base::FilePath fingerprint_path(GetFingerprintPath(src));
  if (base::PathExists(fingerprint_path)) {
    std::string fingerprint_contents;
    if (!base::ReadFileToStringWithMaxSize(
            fingerprint_path, &fingerprint_contents, kMaximumFilesize)) {
      return false;
    }

    if (!IsValidFingerprintFile(fingerprint_contents)) return false;

    if (!WriteFileToDisk(GetFingerprintPath(dest), fingerprint_contents)) {
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

  if (!ReadFileWithCallback(
          &file, [&sha256, &out_file](const char* buf, int length) {
            sha256->Update(buf, length);
            return out_file.WriteAtCurrentPos(buf, length) == length;
          })) {
    LOG(ERROR) << "Failed to read image file.";
    return false;
  }

  std::vector<uint8_t> file_hash(crypto::kSHA256Length);
  sha256->Finish(file_hash.data(), file_hash.size());

  if (expected_hash != file_hash) {
    LOG(ERROR) << "Image is corrupt or modified.";
    return false;
  }
  return true;
}

bool ImageLoaderImpl::VerifyAndParseManifest(
    const std::string& manifest_contents, const std::string& signature,
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

  std::string manifest_str;
  std::string manifest_sig;
  Manifest manifest;
  if (!GetAndVerifyManifest("", component_path, &manifest, &manifest_str,
                            &manifest_sig)) {
    return false;
  }

  if (!WriteFileToDisk(GetManifestPath(destination_folder), manifest_str) ||
      !WriteFileToDisk(GetSignaturePath(destination_folder), manifest_sig)) {
    LOG(ERROR) << "Could not write manifest and signature to disk.";
    return false;
  }

  if (manifest.version != version) {
    LOG(ERROR) << "The client provided a different component version than the "
                  "manifest.";
    return false;
  }

  base::FilePath params_src(GetParamsPath(component_path));
  base::FilePath params_dest(GetParamsPath(destination_folder));
  if (!CopyAndHashFile(params_src, params_dest, manifest.params_sha256)) {
    LOG(ERROR) << "Could not copy params file.";
    return false;
  }

  base::FilePath image_src(GetImagePath(component_path));
  base::FilePath image_dest(GetImagePath(destination_folder));
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

  if (!verifier.VerifyInit(crypto::SignatureVerifier::ECDSA_SHA256,
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

bool ImageLoaderImpl::LoadComponentHelper(const std::string& name,
                                          const Manifest& manifest,
                                          const base::FilePath& mount_point) {
  base::FilePath component_path(GetComponentPath(config_.storage_dir, name));
  if (!base::PathExists(component_path)) {
    LOG(ERROR) << "No valid component [" << name << "] on disk.";
    return false;
  }
  base::FilePath versioned_path(
      GetVersionedPath(component_path, manifest.version));

  // Now read the parameters in and verify the hash.
  std::string params;
  if (!GetAndVerifyParams(GetParamsPath(versioned_path), manifest.params_sha256,
                          &params)) {
    LOG(ERROR) << "Could not read and verify dm-verity parameters.";
    return false;
  }

  // Before we mount the image, check the hash as a sanity check.
  base::ScopedFD image_fd;
  base::FilePath image_path(GetImagePath(versioned_path));
  if (!GetAndVerifyImage(image_path, manifest.image_sha256, &image_fd)) {
    LOG(ERROR) << "Failed to load and verify the disk image.";
    return false;
  }

  bool already_mounted = false;
  if (!CreateMountPointIfNeeded(mount_point, &already_mounted)) return false;
  if (!already_mounted) {
    // The mount point is not yet taken, so go ahead.
    if (!config_.loop_mounter->Mount(image_fd, mount_point)) {
      LOG(ERROR) << "Failed to mount image.";
      return false;
    }
  }
  return true;
}

bool ImageLoaderImpl::LoadComponent(const std::string& name,
                                    const std::string& mount_point_str) {
  Manifest manifest;
  if (!GetManifestForComponent(name, &manifest)) return false;

  base::FilePath mount_point(mount_point_str);
  return LoadComponentHelper(name, manifest, mount_point);
}

std::string ImageLoaderImpl::LoadComponent(const std::string& name) {
  Manifest manifest;
  if (!GetManifestForComponent(name, &manifest)) return kBadResult;

  base::FilePath mount_point(
      GetMountPoint(config_.mount_path, name, manifest.version));
  return LoadComponentHelper(name, manifest, mount_point) ? mount_point.value()
                                                          : kBadResult;
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

  base::FilePath component_root(GetComponentPath(components_dir, name));

  std::string current_version_hint;
  base::FilePath version_hint_path(GetVersionFilePath(component_root));
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
  base::FilePath version_path(GetVersionedPath(component_root, version));
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
    base::DeleteFile(GetVersionedPath(component_root, current_version_hint),
                     true);
  }

  return true;
}

std::string ImageLoaderImpl::GetComponentVersion(const std::string& name) {
  base::FilePath component_path(GetComponentPath(config_.storage_dir, name));
  if (!base::PathExists(component_path)) {
    LOG(ERROR) << "No valid component [" << name << "] on disk.";
    return kBadResult;
  }

  Manifest manifest;
  if (!GetAndVerifyManifest(name, component_path, &manifest, nullptr, nullptr))
    return kBadResult;

  return manifest.version;
}

}  // namespace imageloader
