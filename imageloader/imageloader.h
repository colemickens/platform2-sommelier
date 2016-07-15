// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IMAGELOADER_IMAGELOADER_H_
#define IMAGELOADER_IMAGELOADER_H_

#include <map>
#include <string>
#include <utility>

#include <base/files/file_path.h>
#include <base/gtest_prod_util.h>
#include <dbus-c++/dbus.h>

#include "imageloader-glue.h"

namespace imageloader {

// This is a utility that handles mounting and unmounting of
// verified filesystem images that might include binaries intended
// to be run as read only.
class ImageLoader : org::chromium::ImageLoaderInterface_adaptor,
                    public DBus::ObjectAdaptor {
 public:
  // Instantiate a D-Bus Helper Instance
  explicit ImageLoader(DBus::Connection* conn);

  // Register a component.
  bool RegisterComponent(const std::string& name, const std::string& version,
                         const std::string& component_folder_abs_path,
                         ::DBus::Error& err);

  // Get component version given component name.
  std::string GetComponentVersion(const std::string& name, ::DBus::Error& err);

  // Load the specified component.
  std::string LoadComponent(const std::string& name, ::DBus::Error& err);
  std::string LoadComponentUtil(const std::string& name);

  // Unload the specified component.
  bool UnloadComponent(const std::string& name, ::DBus::Error& err);
  bool UnloadComponentUtil(const std::string& name);

 private:
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

  // Verify the data with the RSA (PKCS #1 v1.5) signature.
  static bool ECVerify(const base::StringPiece data,
                       const base::StringPiece sig);

  // Copy the component directory from a user controlled location to an
  // imageloader controlled location. Do not copy unless it verifies.
  static bool CopyComponentDirectory(const base::FilePath& component_path,
                                     const base::FilePath& destination_folder,
                                     const std::string& version);
  // Check the string contents to see if it matches the format of a
  // manifest.fingerprint file.
  static bool IsValidFingerprintFile(const std::string& contents);
  // Verify the imageloader.json manifest file and parse the file information
  // out of it.
  static bool VerifyAndParseManifest(const std::string& manifest_str,
                                     const std::string& signature,
                                     Manifest* manifest);
  // Copies files over and checks their hash in the process. The copy fails if
  // the hashes do not match.
  static bool CopyAndHashFile(const base::FilePath& src_path,
                              const base::FilePath& dest_path,
                              const std::vector<uint8_t>& known_hash);
  // Check if the client created a manifest.fingerprint, and preserve it.
  static bool CopyFingerprintFile(const base::FilePath& src,
                                  const base::FilePath& dest);

  // "mounts" keeps track of what has been mounted.
  // mounts = (name, (mount_point, device_path))
  std::map<std::string, std::pair<base::FilePath, base::FilePath>> mounts;
  // "reg" keeps track of registered components.
  // reg = (name, (version, fs_image_abs_path))
  std::map<std::string, std::pair<std::string, base::FilePath>> reg;
};

}  // namespace imageloader

#endif  // IMAGELOADER_IMAGELOADER_H_
