// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloader_impl.h"

#include <dirent.h>
#include <stdlib.h>

#include <algorithm>
#include <list>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <base/numerics/safe_conversions.h>
#include <base/strings/string_number_conversions.h>
#include <base/values.h>
#include <crypto/secure_hash.h>
#include <crypto/sha2.h>

namespace imageloader {

namespace {

// Test data always uses the dev key.
const uint8_t kDevPublicKey[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0x7a, 0xaa, 0x2b, 0xf9, 0x3d, 0x7a, 0xbe, 0x35, 0x9a,
    0xfc, 0x9f, 0x39, 0x2d, 0x2d, 0x37, 0x07, 0xd4, 0x19, 0x67, 0x67, 0x30,
    0xbb, 0x5c, 0x74, 0x22, 0xd5, 0x02, 0x07, 0xaf, 0x6b, 0x12, 0x9d, 0x12,
    0xf0, 0x34, 0xfd, 0x1a, 0x7f, 0x02, 0xd8, 0x46, 0x2b, 0x25, 0xca, 0xa0,
    0x6e, 0x2b, 0x54, 0x41, 0xee, 0x92, 0xa2, 0x0f, 0xa2, 0x2a, 0xc0, 0x30,
    0xa6, 0x8c, 0xd1, 0x16, 0x0a, 0x48, 0xca};

const char kImageLoaderJSON[] =
    "{\"image-sha256-hash\":"
    "\"71D11CA4E2B4A3F5E71D789F0E64116F49BB13DE6A591505CA6404985E13F6EF\","
    "\"version\":\"22.0.0.158\",\"params-sha256-hash\":"
    "\"40608D72852DBD223B159FE149CEEE8F8865D46AB28557C2955BC1F02FFECCA7\","
    "\"manifest-version\":1}";

const char kImageLoaderSig[] = {
    0x30, 0x44, 0x02, 0x20, 0x0a, 0x75, 0x49, 0xaf, 0x01, 0x3b, 0x48, 0x51,
    0x45, 0x74, 0x8b, 0x41, 0x64, 0x21, 0x83, 0xce, 0xf1, 0x78, 0x1d, 0xd0,
    0xa8, 0xd6, 0xae, 0x84, 0xf3, 0xc0, 0x3c, 0x3a, 0xee, 0xb4, 0x35, 0xb7,
    0x02, 0x20, 0x34, 0xeb, 0xdc, 0x68, 0x2d, 0x8b, 0x4f, 0x64, 0x94, 0x64,
    0xa3, 0xd5, 0xde, 0xab, 0xf9, 0xa0, 0xbd, 0xcc, 0xc1, 0x2f, 0x78, 0xd4,
    0xe8, 0xed, 0x6a, 0x45, 0x38, 0x53, 0x54, 0xd2, 0xb1, 0x97};

const char kImageLoaderBadSig[] = {
    0x30, 0x44, 0x02, 0x20, 0x0a, 0x75, 0x49, 0xaf, 0x01, 0x3b, 0x48, 0x51,
    0x45, 0x74, 0x8b, 0x41, 0x64, 0x21, 0x83, 0xce, 0xf1, 0x78, 0x1d, 0xd0,
    0xa8, 0xd6, 0xae, 0x84, 0xf3, 0xc1, 0x3c, 0x3a, 0xee, 0xb4, 0x35, 0xb7,
    0x02, 0x20, 0x34, 0xeb, 0xdc, 0x68, 0x2d, 0x8b, 0x4f, 0x64, 0x94, 0x64,
    0xa3, 0xd5, 0xde, 0xab, 0xf9, 0xa0, 0xbd, 0xcc, 0xc1, 0x2f, 0x78, 0xd4,
    0xe8, 0xed, 0x6a, 0x45, 0x38, 0x53, 0x54, 0xd2, 0xb1, 0x97};

// This is the name of the component used in the test data.
const char kTestComponentName[] = "PepperFlashPlayer";
// This is the version of flash player used in the test data.
const char kTestDataVersion[] = "22.0.0.158";
// This is the version of the updated flash player in the test data.
const char kTestUpdatedVersion[] = "22.0.0.256";

}  // namespace {}

class ImageLoaderTest : public testing::Test {
 public:
  ImageLoaderConfig GetConfig(const char* path) {
    std::vector<uint8_t> key(std::begin(kDevPublicKey),
                             std::end(kDevPublicKey));
    ImageLoaderConfig config(key, path);
    return config;
  }

  base::FilePath GetComponentPath() {
    return GetComponentPath(kTestDataVersion);
  }

  base::FilePath GetComponentPath(const std::string& version) {
    const char* src_dir = getenv("CROS_WORKON_SRCROOT");
    CHECK(src_dir != nullptr);
    base::FilePath component_path(src_dir);
    component_path =
        component_path.Append("src")
            .Append("platform")
            .Append("imageloader")
            .Append("test")
            .Append(version + "_chromeos_intel64_PepperFlashPlayer");
    return component_path;
  }

  void GetFilesInDir(const base::FilePath& dir, std::list<std::string>* files) {
    base::FileEnumerator file_enum(dir, false, base::FileEnumerator::FILES);
    for (base::FilePath name = file_enum.Next(); !name.empty();
         name = file_enum.Next()) {
      files->emplace_back(name.BaseName().value());
    }
  }
};

// Test the RegisterComponent public interface.
TEST_F(ImageLoaderTest, RegisterComponentAndGetVersion) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath& temp_dir = scoped_temp_dir.path();
  // Delete the directory so that the ImageLoader can recreate it with the
  // correct permissions.
  base::DeleteFile(temp_dir, /*recursive=*/true);

  ImageLoaderImpl loader(GetConfig(temp_dir.value().c_str()));
  ASSERT_TRUE(loader.RegisterComponent(kTestComponentName, kTestDataVersion,
                                       GetComponentPath().value()));

  base::FilePath comp_dir = temp_dir.Append(kTestComponentName);
  ASSERT_TRUE(base::DirectoryExists(comp_dir));

  base::FilePath hint_file = comp_dir.Append(kTestComponentName);
  ASSERT_TRUE(base::PathExists(hint_file));

  std::string hint_file_contents;
  ASSERT_TRUE(
      base::ReadFileToStringWithMaxSize(hint_file, &hint_file_contents, 4096));
  EXPECT_EQ(kTestDataVersion, hint_file_contents);

  base::FilePath version_dir = comp_dir.Append(kTestDataVersion);
  ASSERT_TRUE(base::DirectoryExists(version_dir));

  std::list<std::string> files;
  GetFilesInDir(version_dir, &files);
  EXPECT_THAT(files, testing::UnorderedElementsAre(
                         "imageloader.json", "imageloader.sig.1", "params",
                         "image.squash", "manifest.fingerprint"));

  // Reject a component if the version already exists.
  EXPECT_FALSE(loader.RegisterComponent(kTestComponentName, kTestDataVersion,
                                       GetComponentPath().value()));

  EXPECT_EQ(kTestDataVersion, loader.GetComponentVersion(kTestComponentName));

  // Now copy a new version into place.
  EXPECT_TRUE(loader.RegisterComponent(kTestComponentName, kTestUpdatedVersion,
                                       GetComponentPath(kTestUpdatedVersion).value()));

  std::string hint_file_contents2;
  ASSERT_TRUE(
      base::ReadFileToStringWithMaxSize(hint_file, &hint_file_contents2, 4096));
  EXPECT_EQ(kTestUpdatedVersion, hint_file_contents2);

  base::FilePath version_dir2 = comp_dir.Append(kTestUpdatedVersion);
  ASSERT_TRUE(base::DirectoryExists(version_dir2));

  std::list<std::string> files2;
  GetFilesInDir(version_dir2, &files2);
  EXPECT_THAT(files2, testing::UnorderedElementsAre("imageloader.json",
                                                    "imageloader.sig.1",
                                                    "params", "image.squash"));

  EXPECT_EQ(kTestUpdatedVersion, loader.GetComponentVersion(kTestComponentName));

  // Reject rollback to an older version.
  EXPECT_FALSE(loader.RegisterComponent(kTestComponentName, kTestDataVersion,
                                       GetComponentPath().value()));

  EXPECT_EQ(kTestUpdatedVersion, loader.GetComponentVersion(kTestComponentName));
}

TEST_F(ImageLoaderTest, ECVerify) {
  ImageLoaderImpl loader(GetConfig("/nonexistant"));
  EXPECT_TRUE(loader.ECVerify(
      base::StringPiece(kImageLoaderJSON),
      base::StringPiece(kImageLoaderSig, sizeof(kImageLoaderSig))));

  EXPECT_FALSE(loader.ECVerify(
      base::StringPiece(kImageLoaderJSON),
      base::StringPiece(kImageLoaderBadSig, sizeof(kImageLoaderBadSig))));
}

TEST_F(ImageLoaderTest, ManifestFingerPrint) {
  ImageLoaderImpl loader(GetConfig("/nonexistant"));
  const std::string valid_manifest =
      "1.3464353b1ed78574e05f3ffe84b52582572b2fe7202f3824a3761e54ace8bb1";
  EXPECT_TRUE(loader.IsValidFingerprintFile(valid_manifest));

  const std::string invalid_unicode_manifest = "Ё Ђ Ѓ Є Ѕ І Ї Ј Љ ";
  EXPECT_FALSE(loader.IsValidFingerprintFile(invalid_unicode_manifest));

  EXPECT_FALSE(loader.IsValidFingerprintFile("\x49\x34\x19-43.*+abc"));
}

TEST_F(ImageLoaderTest, CopyValidComponent) {
  ImageLoaderImpl loader(GetConfig("/nonexistant"));
  const int image_size = 4096 * 4;
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath& temp_dir = scoped_temp_dir.path();

  std::vector<char> image(image_size, 0xBB);
  base::FilePath component_dest = temp_dir.Append("copied-component");
  ASSERT_TRUE(loader.CopyComponentDirectory(GetComponentPath(), component_dest,
                                            kTestDataVersion));

  // Check that all the files are present, except for the manifest.json which
  // should be discarded.
  std::list<std::string> original_files;
  std::list<std::string> copied_files;
  GetFilesInDir(GetComponentPath(), &original_files);
  GetFilesInDir(component_dest, &copied_files);

  EXPECT_THAT(original_files,
              testing::UnorderedElementsAre(
                  "imageloader.json", "imageloader.sig.1", "manifest.json",
                  "params", "image.squash", "manifest.fingerprint"));
  EXPECT_THAT(copied_files,
              testing::UnorderedElementsAre(
                  "imageloader.json", "imageloader.sig.1", "params",
                  "image.squash", "manifest.fingerprint"));
}

TEST_F(ImageLoaderTest, CopyComponentWithBadManifest) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath& temp_dir = scoped_temp_dir.path();

  base::FilePath bad_component_dir = temp_dir.Append("bad-component");

  ASSERT_TRUE(base::CopyDirectory(GetComponentPath(), bad_component_dir,
                                  true /* recursive */));

  base::FilePath manifest = bad_component_dir.Append("imageloader.json");
  const char data[] = "c";
  ASSERT_TRUE(base::AppendToFile(manifest, data, sizeof(data)));

  ImageLoaderImpl loader(GetConfig("/nonexistant"));
  EXPECT_FALSE(loader.CopyComponentDirectory(
      bad_component_dir, temp_dir.Append("copied-component"), kTestDataVersion));
}

TEST_F(ImageLoaderTest, CopyValidImage) {
  const int image_size = 4096 * 4;

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath& temp_dir = scoped_temp_dir.path();

  base::FilePath image_path = temp_dir.Append("image");
  std::vector<char> image(image_size,
                          0xBB);  // large enough to test streaming read.
  ASSERT_TRUE(base::WriteFile(image_path, image.data(), image.size()) ==
              image_size);

  std::vector<uint8_t> hash(crypto::kSHA256Length);

  std::unique_ptr<crypto::SecureHash> sha256(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  sha256->Update(image.data(), image.size());
  sha256->Finish(hash.data(), hash.size());

  ImageLoaderImpl loader(GetConfig("/nonexistant"));
  base::FilePath image_dest = temp_dir.Append("image.copied");
  ASSERT_TRUE(loader.CopyAndHashFile(image_path, image_dest, hash));

  // Check if the image file actually exists and has the correct contents.
  std::string resulting_image;
  ASSERT_TRUE(base::ReadFileToStringWithMaxSize(image_dest, &resulting_image,
                                                image_size));

  EXPECT_TRUE(memcmp(image.data(), resulting_image.data(), image_size) == 0);
}

TEST_F(ImageLoaderTest, CopyInvalidImage) {
  const int image_size = 4096 * 4;
  // It doesn't matter what the hash is, because this is testing a mismatch.
  std::string hash_str =
      "5342065E5D9889739B281D96FD985270A13F2B68A29DD47142ABFA0C2C659AA1";
  std::vector<uint8_t> hash;
  ASSERT_TRUE(base::HexStringToBytes(hash_str, &hash));

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath& temp_dir = scoped_temp_dir.path();

  base::FilePath image_src = temp_dir.Append("bad_image.squash");
  base::FilePath image_dest = temp_dir.Append("image.squash");

  std::vector<char> file(image_size, 0xAA);
  ASSERT_EQ(image_size, base::WriteFile(image_src, file.data(), image_size));

  ImageLoaderImpl loader(GetConfig("/nonexistant"));
  EXPECT_FALSE(loader.CopyAndHashFile(image_src, image_dest, hash));
}

TEST_F(ImageLoaderTest, ParseManifest) {
  ImageLoaderImpl loader(GetConfig("/nonexistant"));
  ImageLoaderImpl::Manifest manifest;
  ASSERT_TRUE(loader.VerifyAndParseManifest(kImageLoaderJSON, kImageLoaderSig,
                                            &manifest));
  EXPECT_EQ(1, manifest.manifest_version);
  EXPECT_EQ(kTestDataVersion, manifest.version);
  EXPECT_EQ(32, manifest.image_sha256.size());
  EXPECT_EQ(32, manifest.params_sha256.size());

  std::string bad_manifest = "{\"foo\":\"128.0.0.9\"}";
  ImageLoaderImpl::Manifest manifest2;
  EXPECT_FALSE(
      loader.VerifyAndParseManifest(bad_manifest, kImageLoaderSig, &manifest2));

  ImageLoaderImpl::Manifest manifest3;
  EXPECT_FALSE(loader.VerifyAndParseManifest(kImageLoaderJSON,
                                             kImageLoaderBadSig, &manifest3));
}

}   // namespace imageloader
