// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/acpi_wakeup_helper.h"

#include <utility>

#include "gtest/gtest.h"
#include "power_manager/common/test_main_loop_runner.h"

namespace power_manager {
namespace system {

namespace {

const char* const kFileContents[] = {
    // State 0
    "Device\tS-state\t  Status   Sysfs node\n"
    "LID0\t  S4\t*enabled \n"
    "TPAD\t  S3\t*enabled   pnp:00:00\n"
    "TSCR\t  S3\t*enabled   pnp:00:01\n"
    "HDEF\t  S3\t*disabled  pci:0000:00:1b.0\n"
    "EHCI\t  S3\t*disabled\n"
    "XHCI\t  S3\t*enabled   pci:0000:00:14.0\n",

    // State 1: After toggling TSCR
    "Device\tS-state\t  Status   Sysfs node\n"
    "LID0\t  S4\t*enabled \n"
    "TPAD\t  S3\t*enabled   pnp:00:00\n"
    "TSCR\t  S3\t*disabled  pnp:00:01\n"
    "HDEF\t  S3\t*disabled  pci:0000:00:1b.0\n"
    "EHCI\t  S3\t*disabled\n"
    "XHCI\t  S3\t*enabled   pci:0000:00:14.0\n"};

class FakeAcpiWakeupFile : public AcpiWakeupFileInterface {
 public:
  FakeAcpiWakeupFile()
      : contents_(nullptr),
        expected_write_(nullptr),
        contents_after_write_(nullptr) {}

  bool Exists() override { return contents_ != nullptr; }

  bool Read(std::string* contents) override {
    if (!contents_)
      return false;
    *contents = contents_;
    return true;
  }

  bool Write(const std::string& contents) override {
    if (!expected_write_ || contents != std::string(expected_write_)) {
      ADD_FAILURE() << "Unexpected write";
      return false;
    }
    contents_ = contents_after_write_;
    expected_write_ = nullptr;
    contents_after_write_ = nullptr;
    return true;
  }

  void set_contents(const char* contents) {
    contents_ = contents;
    expected_write_ = nullptr;
    contents_after_write_ = nullptr;
  }

  void ExpectWrite(const char* expected_write,
                   const char* contents_after_write) {
    expected_write_ = expected_write;
    contents_after_write_ = contents_after_write;
  }

  void Verify() {
    if (expected_write_) {
      ADD_FAILURE() << "Expected write did not happen";
    }
  }

 private:
  const char* contents_;
  const char* expected_write_;
  const char* contents_after_write_;
};

}  // namespace

class AcpiWakeupHelperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto file = std::make_unique<FakeAcpiWakeupFile>();
    file_ = file.get();
    helper_.set_file_for_testing(std::move(file));
  }

  AcpiWakeupHelper helper_;
  FakeAcpiWakeupFile* file_;
};

TEST_F(AcpiWakeupHelperTest, Get) {
  bool enabled;

  file_->set_contents(kFileContents[0]);

  ASSERT_TRUE(helper_.GetWakeupEnabled("LID0", &enabled));
  EXPECT_TRUE(enabled);

  ASSERT_TRUE(helper_.GetWakeupEnabled("HDEF", &enabled));
  EXPECT_FALSE(enabled);
}

TEST_F(AcpiWakeupHelperTest, SetToSameState) {
  file_->set_contents(kFileContents[0]);
  ASSERT_TRUE(helper_.SetWakeupEnabled("TPAD", true));
}

TEST_F(AcpiWakeupHelperTest, SetToDifferentState) {
  file_->set_contents(kFileContents[0]);
  file_->ExpectWrite("TSCR", kFileContents[1]);
  ASSERT_TRUE(helper_.SetWakeupEnabled("TSCR", false));
  file_->Verify();
}

}  // namespace system
}  // namespace power_manager
