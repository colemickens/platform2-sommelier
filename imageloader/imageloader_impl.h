// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IMAGELOADER_IMAGELOADER_IMPL_H_
#define IMAGELOADER_IMAGELOADER_IMPL_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/gtest_prod_util.h>
#include <base/macros.h>

#include "verity_mounter.h"

namespace imageloader {

struct ImageLoaderConfig {
  ImageLoaderConfig(const std::vector<uint8_t> key, const char* storage_path,
                    const char* mount_path, std::unique_ptr<VerityMounter> ops)
      : key(key),
        storage_dir(storage_path),
        mount_path(mount_path),
        verity_mounter(std::move(ops)) {}

  std::vector<uint8_t> key;
  base::FilePath storage_dir;
  base::FilePath mount_path;
  std::unique_ptr<VerityMounter> verity_mounter;
};



class ImageLoaderImpl {
 public:
  // Instantiate an object with a configuration object.
  explicit ImageLoaderImpl(ImageLoaderConfig config)
      : config_(std::move(config)) {}

  // Register a component.
  bool RegisterComponent(const std::string& name, const std::string& version,
                         const std::string& component_folder_abs_path);

  // Get component version given component name.
  std::string GetComponentVersion(const std::string& name);

  // Load the specified component.
  std::string LoadComponent(const std::string& name);

  // Load the specified component at a set mount point.
  bool LoadComponent(const std::string& name, const std::string& mount_point);

 private:
  // This is a parsed version of the imageloader.json manifest.
  struct Manifest {
    int manifest_version;
    std::vector<uint8_t> image_sha256;
    std::vector<uint8_t> table_sha256;
    std::string version;
  };

  FRIEND_TEST_ALL_PREFIXES(ImageLoaderTest, ECVerify);
  FRIEND_TEST_ALL_PREFIXES(ImageLoaderTest, ManifestFingerPrint);
  FRIEND_TEST_ALL_PREFIXES(ImageLoaderTest, CopyValidComponent);
  FRIEND_TEST_ALL_PREFIXES(ImageLoaderTest, CopyComponentWithBadManifest);
  FRIEND_TEST_ALL_PREFIXES(ImageLoaderTest, CopyValidImage);
  FRIEND_TEST_ALL_PREFIXES(ImageLoaderTest, CopyInvalidImage);
  FRIEND_TEST_ALL_PREFIXES(ImageLoaderTest, CopyInvalidHash);
  FRIEND_TEST_ALL_PREFIXES(ImageLoaderTest, ParseManifest);
  FRIEND_TEST_ALL_PREFIXES(ImageLoaderTest, MountValidImage);

  // Verify the data with the RSA (PKCS #1 v1.5) signature.
  bool ECVerify(const base::StringPiece data, const base::StringPiece sig);

  // Copy the component directory from a user controlled location to an
  // imageloader controlled location. Do not copy unless it verifies.
  bool CopyComponentDirectory(const base::FilePath& component_path,
                              const base::FilePath& destination_folder,
                              const std::string& version);

  // Check the string contents to see if it matches the format of a
  // manifest.fingerprint file.
  bool IsValidFingerprintFile(const std::string& contents);

  // Verify the imageloader.json manifest file and parse the file information
  // out of it.
  bool VerifyAndParseManifest(const std::string& manifest_str,
                              const std::string& signature, Manifest* manifest);

  // Copies files over and checks their hash in the process. The copy fails if
  // the hashes do not match.
  bool CopyAndHashFile(const base::FilePath& src_path,
                       const base::FilePath& dest_path,
                       const std::vector<uint8_t>& known_hash);

  // Check if the client created a manifest.fingerprint, and preserve it.
  bool CopyFingerprintFile(const base::FilePath& src,
                           const base::FilePath& dest);

  // if |manifest| or |sig| are not null, they are set to the manifest contents
  // and the signature contents.
  bool GetAndVerifyManifest(const std::string& component_name,
                            const base::FilePath& component_path,
                            Manifest* manifest, std::string* manifest_str,
                            std::string* manifest_sig);

  // This performs the actual working of mounting the component. It must be
  // passed a valid |manifest| argument and |mount_point| path.
  bool LoadComponentHelper(const std::string& component_name,
                           const Manifest& manifest,
                           const base::FilePath& mount_point);

  // Looks up the component path for |name| and returns a verified manifest.
  bool GetManifestForComponent(const std::string& name, Manifest* manifest);

  // The configuration traits.
  ImageLoaderConfig config_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoaderImpl);
};

}  // namespace imageloader

#endif  // IMAGELOADER_IMAGELOADER_IMPL_H_
