// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <stdlib.h>

#include <algorithm>
#include <list>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "imageloader.h"

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
  0xe8, 0xed, 0x6a, 0x45, 0x38, 0x53, 0x54, 0xd2, 0xb1, 0x97
};

}  // namespace {}

class ImageLoaderTest : public testing::Test {
 public:
  base::FilePath GetComponentPath() {
    const char* src_dir = getenv("CROS_WORKON_SRCROOT");
    CHECK(src_dir != nullptr);
    base::FilePath component_path(src_dir);
    component_path =
        component_path.Append("src")
            .Append("platform")
            .Append("imageloader")
            .Append("test")
            .Append("22.0.0.158_chromeos_intel64_PepperFlashPlayer");
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

TEST_F(ImageLoaderTest, ECVerify) {
  EXPECT_TRUE(ImageLoader::ECVerify(
      base::StringPiece(kImageLoaderJSON),
      base::StringPiece(kImageLoaderSig, sizeof(kImageLoaderSig))));

  EXPECT_FALSE(ImageLoader::ECVerify(
      base::StringPiece(kImageLoaderJSON),
      base::StringPiece(kImageLoaderBadSig, sizeof(kImageLoaderBadSig))));
}

TEST_F(ImageLoaderTest, ManifestFingerPrint) {
  const std::string valid_manifest =
      "1.3464353b1ed78574e05f3ffe84b52582572b2fe7202f3824a3761e54ace8bb1";
  EXPECT_TRUE(ImageLoader::IsValidFingerprintFile(valid_manifest));

  const std::string invalid_unicode_manifest = "Ё Ђ Ѓ Є Ѕ І Ї Ј Љ ";
  EXPECT_FALSE(ImageLoader::IsValidFingerprintFile(invalid_unicode_manifest));

  EXPECT_FALSE(ImageLoader::IsValidFingerprintFile("\x49\x34\x19-43.*+abc"));
}

TEST_F(ImageLoaderTest, CopyValidComponent) {
  const int image_size = 4096 * 4;
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath& temp_dir = scoped_temp_dir.path();

  std::vector<char> image(image_size, 0xBB);
  base::FilePath component_dest = temp_dir.Append("copied-component");
  ASSERT_TRUE(ImageLoader::CopyComponentDirectory(
      GetComponentPath(), component_dest, "22.0.0.158"));

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

  EXPECT_FALSE(ImageLoader::CopyComponentDirectory(
      bad_component_dir, temp_dir.Append("copied-component"), "22.0.0.158"));
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

  base::FilePath image_dest = temp_dir.Append("image.copied");
  ASSERT_TRUE(ImageLoader::CopyAndHashFile(image_path, image_dest, hash));

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

  EXPECT_FALSE(ImageLoader::CopyAndHashFile(image_src, image_dest, hash));
}

}
