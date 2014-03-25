// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lorgnette/manager.h"

#include <map>
#include <string>

#include <base/memory/scoped_ptr.h>
#include <base/stl_util.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <chromeos/process_mock.h>
#include <gtest/gtest.h>

using file_util::ScopedFD;
using std::map;
using std::string;
using testing::_;
using testing::InSequence;
using testing::Return;

namespace lorgnette {

class ManagerTest : public testing::Test {
 public:
  ManagerTest()
     : input_pipe_fd_(kInputPipeFd),
       output_pipe_fd_(kOutputPipeFd),
       input_scoped_fd_(&input_pipe_fd_),
       output_scoped_fd_(&output_pipe_fd_),
       manager_(base::Callback<void()>()) {}

  virtual void TearDown() {
    // The fds that we have handed to these ScopedFD are not real, so we
    // must prevent our scoped fds from calling close() on them.
    int *fd_ptr;
    fd_ptr = input_scoped_fd_.release();
    CHECK(fd_ptr == NULL || fd_ptr == &input_pipe_fd_);
    fd_ptr = output_scoped_fd_.release();
    CHECK(fd_ptr == NULL || fd_ptr == &output_pipe_fd_);
  }
 protected:
  static const char kDeviceName[];
  static const char kMode[];
  static const int kInputPipeFd;
  static const int kOutputFd;
  static const int kOutputPipeFd;
  static const int kResolution;

  static void RunListScannersProcess(int fd, chromeos::Process *process) {
    Manager::RunListScannersProcess(fd, process);
  }

  static void RunScanImageProcess(
      const string &device_name,
      int out_fd,
      file_util::ScopedFD *input_scoped_fd,
      file_util::ScopedFD *output_scoped_fd,
      const map<string, ::DBus::Variant> &scan_properties,
      chromeos::Process *scan_process,
      chromeos::Process *convert_process,
      ::DBus::Error *error) {
    Manager::RunScanImageProcess(device_name,
                                 out_fd,
                                 input_scoped_fd,
                                 output_scoped_fd,
                                 scan_properties,
                                 scan_process,
                                 convert_process,
                                 error);
  }

  static void ExpectStartScan(
      const char *mode,
      int resolution,
      chromeos::ProcessMock *scan_process,
      chromeos::ProcessMock *convert_process) {
    EXPECT_CALL(*scan_process, AddArg(GetScanImagePath()));
    EXPECT_CALL(*scan_process, AddArg("-d"));
    EXPECT_CALL(*scan_process, AddArg(kDeviceName));
    if (mode) {
      EXPECT_CALL(*scan_process, AddArg("--mode"));
      EXPECT_CALL(*scan_process, AddArg(mode));
    }
    if (resolution) {
      const string kResolutionString(base::IntToString(resolution));
      EXPECT_CALL(*scan_process, AddArg("--resolution"));
      EXPECT_CALL(*scan_process, AddArg(kResolutionString));
    }
    EXPECT_CALL(*scan_process, BindFd(kOutputPipeFd, STDOUT_FILENO));
    EXPECT_CALL(*convert_process, AddArg(GetScanConverterPath()));
    EXPECT_CALL(*convert_process, BindFd(kInputPipeFd, STDIN_FILENO));
    EXPECT_CALL(*convert_process, BindFd(kOutputFd, STDOUT_FILENO));
    EXPECT_CALL(*convert_process, Start());
    EXPECT_CALL(*scan_process, Start());
  }

  static Manager::ScannerInfo ScannerInfoFromString(
      const std::string &scanner_info_string) {
    return Manager::ScannerInfoFromString(scanner_info_string);
  }

  Manager::DBusAdaptor *GetDBusAdaptor() {
    return manager_.dbus_adaptor_.get();
  }
  static std::string GetScanConverterPath() {
    return Manager::kScanConverterPath;
  }
  static std::string GetScanImagePath() { return Manager::kScanImagePath; }
  static std::string GetScanImageFromattedDeviceListCmd() {
    return Manager::kScanImageFormattedDeviceListCmd;
  }

  int input_pipe_fd_;
  int output_pipe_fd_;
  ScopedFD input_scoped_fd_;
  ScopedFD output_scoped_fd_;
  Manager manager_;
};

const int ManagerTest::kInputPipeFd = 123;
const int ManagerTest::kOutputFd = 456;
const int ManagerTest::kOutputPipeFd = 789;
const char ManagerTest::kDeviceName[] = "scanner";
const int ManagerTest::kResolution = 300;
const char ManagerTest::kMode[] = "Color";

TEST_F(ManagerTest, Construct) {
  EXPECT_EQ(NULL, GetDBusAdaptor());
}

TEST_F(ManagerTest, RunListScannersProcess) {
  chromeos::ProcessMock process;
  const int kFd = 123;
  InSequence seq;
  EXPECT_CALL(process, AddArg(GetScanImagePath()));
  EXPECT_CALL(process, AddArg(GetScanImageFromattedDeviceListCmd()));
  EXPECT_CALL(process, BindFd(kFd, STDOUT_FILENO));
  EXPECT_CALL(process, Run());
  RunListScannersProcess(kFd, &process);
}

TEST_F(ManagerTest, RunScanImageProcessSuccess) {
  map<string, ::DBus::Variant> props;
  props["Mode"].writer().append_string(kMode);
  props["Resolution"].writer().append_uint32(kResolution);
  chromeos::ProcessMock scan_process;
  chromeos::ProcessMock convert_process;
  InSequence seq;
  ExpectStartScan(kMode,
                  kResolution,
                  &scan_process,
                  &convert_process);
  EXPECT_CALL(scan_process, Wait()).WillOnce(Return(0));
  EXPECT_CALL(convert_process, Wait()).WillOnce(Return(0));
  DBus::Error error;
  RunScanImageProcess(kDeviceName,
                      kOutputFd,
                      &input_scoped_fd_,
                      &output_scoped_fd_,
                      props,
                      &scan_process,
                      &convert_process,
                      &error);
  EXPECT_EQ(NULL, input_scoped_fd_.get());
  EXPECT_EQ(NULL, output_scoped_fd_.get());
  EXPECT_FALSE(error.is_set());
}

TEST_F(ManagerTest, RunScanImageProcessInvalidArgument) {
  map<string, ::DBus::Variant> props;
  const char kInvalidArgument[] = "InvalidArgument";
  props[kInvalidArgument].writer().append_string("");
  chromeos::ProcessMock scan_process;
  chromeos::ProcessMock convert_process;
  // For "scanimage", "-d", "<device name>".
  EXPECT_CALL(scan_process, AddArg(_)).Times(3);
  EXPECT_CALL(convert_process, AddArg(_)).Times(0);
  EXPECT_CALL(convert_process, Start()).Times(0);
  EXPECT_CALL(scan_process, Start()).Times(0);
  DBus::Error error;
  RunScanImageProcess("", 0, NULL, NULL, props, &scan_process,
                      &convert_process, &error);

  // Expect that the pipe fds have not been released.
  EXPECT_EQ(&input_pipe_fd_, input_scoped_fd_.get());
  EXPECT_EQ(&output_pipe_fd_, output_scoped_fd_.get());

  EXPECT_TRUE(error.is_set());
  EXPECT_TRUE(StartsWithASCII(error.message(),
                              base::StringPrintf("Invalid scan parameter %s",
                                                 kInvalidArgument),
                              false));
}

TEST_F(ManagerTest, RunScanImageInvalidModeArgument) {
  map<string, ::DBus::Variant> props;
  const char kBadMode[] = "Raytrace";
  props["Mode"].writer().append_string(kBadMode);
  chromeos::ProcessMock scan_process;
  chromeos::ProcessMock convert_process;
  // For "scanimage", "-d", "<device name>".
  EXPECT_CALL(scan_process, AddArg(_)).Times(3);
  EXPECT_CALL(convert_process, AddArg(_)).Times(0);
  EXPECT_CALL(convert_process, Start()).Times(0);
  EXPECT_CALL(scan_process, Start()).Times(0);
  DBus::Error error;
  RunScanImageProcess(kDeviceName,
                      kOutputFd,
                      &input_scoped_fd_,
                      &output_scoped_fd_,
                      props,
                      &scan_process,
                      &convert_process,
                      &error);

  // Expect that the pipe fds have not been released.
  EXPECT_EQ(&input_pipe_fd_, input_scoped_fd_.get());
  EXPECT_EQ(&output_pipe_fd_, output_scoped_fd_.get());

  EXPECT_TRUE(error.is_set());
  EXPECT_TRUE(StartsWithASCII(error.message(),
                              base::StringPrintf("Invalid mode parameter %s",
                                                 kBadMode),
                              false));
}

TEST_F(ManagerTest, RunScanImageProcessCaptureFailure) {
  map<string, ::DBus::Variant> props;
  props["Mode"].writer().append_string(kMode);
  props["Resolution"].writer().append_uint32(kResolution);
  chromeos::ProcessMock scan_process;
  chromeos::ProcessMock convert_process;
  InSequence seq;
  ExpectStartScan(kMode,
                  kResolution,
                  &scan_process,
                  &convert_process);
  const int kErrorResult = 999;
  EXPECT_CALL(scan_process, Wait()).WillOnce(Return(kErrorResult));
  EXPECT_CALL(convert_process, Kill(SIGKILL, 1));
  EXPECT_CALL(convert_process, Wait()).Times(0);
  DBus::Error error;
  RunScanImageProcess(kDeviceName,
                      kOutputFd,
                      &input_scoped_fd_,
                      &output_scoped_fd_,
                      props,
                      &scan_process,
                      &convert_process,
                      &error);
  EXPECT_EQ(NULL, input_scoped_fd_.get());
  EXPECT_EQ(NULL, output_scoped_fd_.get());
  EXPECT_TRUE(error.is_set());
  EXPECT_STREQ(
      base::StringPrintf("Scan process exited with result %d",
                         kErrorResult).c_str(),
      error.message());
}

TEST_F(ManagerTest, RunScanImageProcessConvertFailure) {
  map<string, ::DBus::Variant> props;
  props["Mode"].writer().append_string(kMode);
  props["Resolution"].writer().append_uint32(kResolution);
  chromeos::ProcessMock scan_process;
  chromeos::ProcessMock convert_process;
  InSequence seq;
  ExpectStartScan(kMode,
                  kResolution,
                  &scan_process,
                  &convert_process);
  EXPECT_CALL(scan_process, Wait()).WillOnce(Return(0));
  const int kErrorResult = 111;
  EXPECT_CALL(convert_process, Wait()).WillOnce(Return(kErrorResult));
  DBus::Error error;
  RunScanImageProcess(kDeviceName,
                      kOutputFd,
                      &input_scoped_fd_,
                      &output_scoped_fd_,
                      props,
                      &scan_process,
                      &convert_process,
                      &error);
  EXPECT_EQ(NULL, input_scoped_fd_.get());
  EXPECT_EQ(NULL, output_scoped_fd_.get());
  EXPECT_TRUE(error.is_set());
  EXPECT_STREQ(
      base::StringPrintf("Image converter process failed with result %d",
                         kErrorResult).c_str(),
      error.message());
}

TEST_F(ManagerTest, ScannerInfoFromString) {
  EXPECT_TRUE(ScannerInfoFromString("").empty());
  EXPECT_TRUE(ScannerInfoFromString("one").empty());
  EXPECT_TRUE(ScannerInfoFromString("one%two").empty());
  EXPECT_TRUE(ScannerInfoFromString("one%two%three").empty());
  const char kDevice0[] = "device0";
  const char kDevice1[] = "device1";
  const char kManufacturer0[] = "rayban";
  const char kManufacturer1[] = "oakley";
  const char kModel0[] = "model0";
  const char kModel1[] = "model1";
  const char kType0[] = "type0";
  const char kType1[] = "type1";
  const string kInputString(base::StringPrintf(
      "one\n"
      "%s%%%s%%%s%%%s\n"
      "one%%two\n"
      "%s%%%s%%%s%%%s\n"
      "one%%two%%three\n"
      "one%%two%%three%%four%%five\n",
      kDevice0, kManufacturer0, kModel0, kType0,
      kDevice1, kManufacturer1, kModel1, kType1));
  Manager::ScannerInfo scan_info = ScannerInfoFromString(kInputString);
  EXPECT_EQ(2, scan_info.size());
  EXPECT_TRUE(ContainsKey(scan_info, kDevice0));
  EXPECT_EQ(3, scan_info[kDevice0].size());
  EXPECT_STREQ(kManufacturer0, scan_info[kDevice0]["Manufacturer"].c_str());
  EXPECT_STREQ(kModel0, scan_info[kDevice0]["Model"].c_str());
  EXPECT_STREQ(kType0, scan_info[kDevice0]["Type"].c_str());

  EXPECT_TRUE(ContainsKey(scan_info, kDevice1));
  EXPECT_EQ(3, scan_info[kDevice1].size());
  EXPECT_STREQ(kManufacturer1, scan_info[kDevice1]["Manufacturer"].c_str());
  EXPECT_STREQ(kModel1, scan_info[kDevice1]["Model"].c_str());
  EXPECT_STREQ(kType1, scan_info[kDevice1]["Type"].c_str());
}

}  // namespace lorgnette
