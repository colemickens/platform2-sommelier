// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IMAGELOADER_IMAGELOADER_UTILITY_H_
#define IMAGELOADER_IMAGELOADER_UTILITY_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/gtest_prod_util.h>
#include <base/macros.h>

namespace imageloader {

struct ImageLoaderConfig {
  ImageLoaderConfig(const std::vector<uint8_t> key, const char* path)
      : key(key), storage_dir(path) {}
  std::vector<uint8_t> key;
  base::FilePath storage_dir;
};

class ImageLoaderImpl {
 public:
  // Instantiate an object with a configuration object.
  explicit ImageLoaderImpl(const ImageLoaderConfig& config) : config_(config) {}

  // Register a component.
  bool RegisterComponent(const std::string& name, const std::string& version,
                         const std::string& component_folder_abs_path);

  // Get component version given component name.
  std::string GetComponentVersion(const std::string& name);

  // Load the specified component.
  std::string LoadComponent(const std::string& name);

 private:
  // This is a parsed version of the imageloader.json manifest.
  struct Manifest {
    int manifest_version;
    std::vector<uint8_t> image_sha256;
    std::vector<uint8_t> params_sha256;
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

  // Do the work to verify and mount components.
  std::string LoadComponentUtil(const std::string& name);

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

  // The configuration traits.
  ImageLoaderConfig config_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoaderImpl);
};

}  // namespace imageloader

#endif  // IMAGELOADER_IMAGELOADER_UTILITY_H_
