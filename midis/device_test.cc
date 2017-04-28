// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
//

#include <fcntl.h>

#include <base/bind.h>
#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>
#include <base/time/time.h>
#include <brillo/daemons/daemon.h>
#include <brillo/test_helpers.h>
#include <gtest/gtest.h>

#include "midis/device.h"
#include "midis/file_handler.h"
#include "midis/test_helper.h"

namespace midis {

namespace {

const char kFakeName1[] = "Sample MIDI Device - 1";
const int kFakeSysNum1 = 2;
const int kFakeDevNum1 = 0;
const int kFakeSubdevs1 = 1;
const int kFakeFlags1 = 7;

const char kFakeMidiData1[] = "0xDEADBEEF";

// Store the data in a string buffer inside this class.
void PrintReadData(std::string* data, const char* buffer,
                   uint32_t subdevice_id) {
  *data = buffer;
}

}  // namespace

class DeviceTest : public ::testing::Test, public brillo::Daemon {
 protected:
  void SetUp() override {
    CreateNewTempDirectory(base::FilePath::StringType(), &temp_fp_);
  }

  void TearDown() override { base::DeleteFile(temp_fp_, true); }

  base::FilePath temp_fp_;
};

TEST_F(DeviceTest, TestHandleDeviceRead) {
  ASSERT_FALSE(temp_fp_.empty());

  base::FilePath dev_path = CreateFakeTempSubDir(temp_fp_, "dev/snd");
  ASSERT_NE(dev_path.value(), "");

  base::FilePath dev_node_path =
      CreateDevNodeFileName(dev_path, kFakeSysNum1, kFakeDevNum1);

  // Create a fake devnode and allow a poll on it.
  ASSERT_EQ(0, base::WriteFile(dev_node_path, nullptr, 0));
  ASSERT_TRUE(base::SetPosixFilePermissions(
      dev_node_path, S_IRGRP | S_IWGRP | S_IRUSR | S_IWUSR));

  auto dev = base::MakeUnique<Device>(kFakeName1, kFakeSysNum1, kFakeDevNum1,
                                      kFakeSubdevs1, kFakeFlags1);
  Device::SetBaseDirForTesting(temp_fp_);
  dev->StartMonitoring();

  // Now we cancel the watcher task, since we want to check the function call
  // manually.
  const auto& subd_handler = dev->handlers_.begin();
  auto& fhandler = subd_handler->second;
  std::string data;
  fhandler->SetDeviceDataCbForTesting(base::Bind(&PrintReadData, &data));
  fhandler->StopMonitoring();

  base::WriteFile(dev_node_path, kFakeMidiData1, sizeof(kFakeMidiData1));
  fhandler->HandleDeviceRead(subd_handler->first);
  EXPECT_EQ(data, kFakeMidiData1);
}

}  // namespace midis

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
