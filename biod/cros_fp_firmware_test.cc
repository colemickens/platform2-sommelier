// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/cros_fp_firmware.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/memory_mapped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <base/rand_util.h>
#include <fmap.h>
#include <gtest/gtest.h>

namespace {

constexpr int kTestImageBaseAddr = 0x8000000;
constexpr int kTestImageSize = 2 * 1024 * 1024;
constexpr char kTestImageFwName[] = "EC_FMAP";
constexpr char kTestImageROIDLabel[] = "RO_FRID";
constexpr char kTestImageRWIDLabel[] = "RW_FWID";
constexpr char kTestImageFileName[] = "nocturne_fp_v2.2.110-b936c0a3c.bin";
constexpr char kTestImageROVersion[] = "nocturne_fp_v2.2.64-58cf5974e";
constexpr char kTestImageRWVersion[] = "nocturne_fp_v2.2.110-b936c0a3c";

class Fmap {
 public:
  Fmap() : fmap_(nullptr) {}
  ~Fmap() { Destroy(); }
  bool Create(uint64_t base, uint32_t size, const char* name) {
    Destroy();
    // fmap_create does not modify name internally
    fmap_ = fmap_create(
        base, size,
        const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(name)));
    return (fmap_ != nullptr);
  }
  bool AppendArea(uint32_t offset,
                  uint32_t size,
                  const char* name,
                  uint16_t flags) {
    CHECK(IsValid());
    return fmap_append_area(&fmap_, offset, size,
                            reinterpret_cast<const uint8_t*>(name), flags) >= 0;
  }
  bool IsValid() { return fmap_ != nullptr; }
  const char* GetData() { return reinterpret_cast<char*>(fmap_); }
  int GetDataLength() { return fmap_size(fmap_); }

 private:
  void Destroy() {
    if (IsValid()) {
      fmap_destroy(fmap_);
    }
  }

  struct fmap* fmap_;

  DISALLOW_COPY_AND_ASSIGN(Fmap);
};

}  // namespace

namespace biod {

class CrosFpFirmwareTest : public ::testing::Test {
 protected:
  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());
    root_path_ = temp_dir_.GetPath();
  }

  void TearDown() override {
    EXPECT_TRUE(base::DeleteFile(temp_dir_.GetPath(), true));
  }

  base::FilePath GetTestTempDir() const { return root_path_; }

  bool CreateFakeImage(const base::FilePath& abspath,
                       const std::string& ro_version,
                       const std::string& rw_version) {
    if (!GetTestTempDir().IsParent(abspath)) {
      LOG(ERROR) << "Asked to PlaceFakeImage outside test environment.";
      return false;
    }

    LOG(INFO) << "Creating fake image at: " << abspath.value();

    // FMAP_STRLEN is the max size of the string including a null character
    if (ro_version.length() >= FMAP_STRLEN) {
      LOG(ERROR) << "Error - ro_version, '" << ro_version
                 << "', is too long. Must be max " << FMAP_STRLEN
                 << " with null terminator.";
      return false;
    }
    if (rw_version.length() >= FMAP_STRLEN) {
      LOG(ERROR) << "Error - rw_version, '" << rw_version
                 << "', is too long. Must be max " << FMAP_STRLEN
                 << " with null terminator.";
      return false;
    }

    base::File file(abspath,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    if (!file.IsValid()) {
      return false;
    }

    std::vector<char> verbuf(FMAP_STRLEN * 2);
    ro_version.copy(&verbuf[0 * FMAP_STRLEN], FMAP_STRLEN - 1);
    rw_version.copy(&verbuf[1 * FMAP_STRLEN], FMAP_STRLEN - 1);

    // place ro and rw versions at the front of the file
    if (file.WriteAtCurrentPos(&verbuf[0], 2 * FMAP_STRLEN) < 0) {
      LOG(ERROR) << "Failed to write version strings into fake image.";
      return false;
    }

    Fmap fmap;
    if (!fmap.Create(kTestImageBaseAddr, kTestImageSize, kTestImageFwName)) {
      LOG(ERROR) << "Failed to allocate fmap struct";
      return false;
    }
    if (!fmap.AppendArea(0 * FMAP_STRLEN, FMAP_STRLEN, kTestImageROIDLabel,
                         FMAP_AREA_RO)) {
      LOG(ERROR) << "Failed to append " << kTestImageROIDLabel << " FW area.";
      return false;
    }
    if (!fmap.AppendArea(1 * FMAP_STRLEN, FMAP_STRLEN, kTestImageRWIDLabel,
                         FMAP_AREA_RO)) {
      LOG(ERROR) << "Failed to append " << kTestImageRWIDLabel << " FW area.";
      return false;
    }
    if (!fmap.IsValid()) {
      LOG(ERROR) << "Fmap data or size are invalid.";
      return false;
    }
    if (file.WriteAtCurrentPos(fmap.GetData(), fmap.GetDataLength()) < 0) {
      LOG(ERROR) << "Failed to write fmap into fake image.";
      return false;
    }

    if (!file.SetLength(kTestImageSize)) {
      LOG(ERROR) << "Failed to elongate fake image to typical size.";
      return false;
    }

    EXPECT_TRUE(base::PathExists(abspath));
    return true;
  }

  void TestExpectFailure(const base::FilePath& image_path,
                         biod::CrosFpFirmware::Status expect_status) {
    biod::CrosFpFirmware fw(image_path);

    EXPECT_STREQ(fw.GetPath().value().c_str(), image_path.value().c_str());
    EXPECT_FALSE(fw.IsValid());
    EXPECT_EQ(fw.GetStatus(), expect_status);
    biod::CrosFpFirmware::ImageVersion fwver = fw.GetVersion();
    EXPECT_STREQ(fwver.ro_version.c_str(), "");
    EXPECT_STREQ(fwver.rw_version.c_str(), "");
    LOG(INFO) << "Passed";
  }

  void TestExpectSuccess(const base::FilePath& image_path,
                         const std::string& expect_ro_version,
                         const std::string& expect_rw_version) {
    auto expect_status = biod::CrosFpFirmware::Status::kOk;

    biod::CrosFpFirmware fw(image_path);

    EXPECT_STREQ(fw.GetPath().value().c_str(), image_path.value().c_str());
    EXPECT_TRUE(fw.IsValid());
    EXPECT_EQ(fw.GetStatus(), expect_status);
    biod::CrosFpFirmware::ImageVersion fwver = fw.GetVersion();
    EXPECT_STREQ(fwver.ro_version.c_str(), expect_ro_version.c_str());
    EXPECT_STREQ(fwver.rw_version.c_str(), expect_rw_version.c_str());
    LOG(INFO) << "Passed";
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath root_path_;

  CrosFpFirmwareTest() = default;
  ~CrosFpFirmwareTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosFpFirmwareTest);
};

// Invalid path - Blank - Fail
TEST_F(CrosFpFirmwareTest, InvalidPathBlank) {
  TestExpectFailure(base::FilePath(""),
                    biod::CrosFpFirmware::Status::kNotFound);
}

// Invalid path - Odd chars - Fail
TEST_F(CrosFpFirmwareTest, InavlidPathOddChars) {
  TestExpectFailure(base::FilePath("--"),
                    biod::CrosFpFirmware::Status::kNotFound);
}

// Invalid path - Given a directory - Fail
TEST_F(CrosFpFirmwareTest, GivenDirectory) {
  TestExpectFailure(GetTestTempDir(), biod::CrosFpFirmware::Status::kNotFound);
}

// File size is 0 - mmap should fail - Fail
TEST_F(CrosFpFirmwareTest, GivenEmptyFile) {
  const auto image_path = GetTestTempDir().Append("empty.txt");
  base::File file(image_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  file.Close();
  EXPECT_TRUE(base::PathExists(image_path));
  TestExpectFailure(image_path, biod::CrosFpFirmware::Status::kOpenError);
}

// File does not contain an FMAP - Fail
TEST_F(CrosFpFirmwareTest, NoFMAP) {
  const auto image_path = GetTestTempDir().Append("nofmap.txt");
  base::File file(image_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  EXPECT_GE(file.WriteAtCurrentPos("a", 1), 1);
  file.Close();
  EXPECT_TRUE(base::PathExists(image_path));
  TestExpectFailure(image_path, biod::CrosFpFirmware::Status::kBadFmap);
}

// FMAP reports size larger than file size - Fail
TEST_F(CrosFpFirmwareTest, FMAPReportsLargerSizeThanFileSize) {
  const auto image_path = GetTestTempDir().Append("fmapreportlargesize.bin");
  base::File file(image_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  Fmap fmap;
  EXPECT_TRUE(fmap.Create(0, kTestImageSize + 1, "FAKE"));
  const char* fdata = fmap.GetData();
  int fsize = fmap.GetDataLength();
  EXPECT_TRUE(fdata != nullptr);
  EXPECT_GE(fsize, 0);
  EXPECT_GT(file.WriteAtCurrentPos(fdata, fsize), 0);
  EXPECT_TRUE(file.SetLength(kTestImageSize));
  file.Close();

  EXPECT_TRUE(base::PathExists(image_path));
  TestExpectFailure(image_path, biod::CrosFpFirmware::Status::kBadFmap);
}

// Good image file - Succeed
TEST_F(CrosFpFirmwareTest, GoodImageFile) {
  base::FilePath image_path = GetTestTempDir().Append(kTestImageFileName);
  EXPECT_TRUE(
      CreateFakeImage(image_path, kTestImageROVersion, kTestImageRWVersion));

  TestExpectSuccess(image_path, kTestImageROVersion, kTestImageRWVersion);
}

// TODO(hesling): Write tests for image files that had malformed and possibly
// dangerous FMAP info
// * Test various phony FMAP signatures
// * Version area size 0
// * Version size reported too large
// * Test many firmware file names

}  // namespace biod
