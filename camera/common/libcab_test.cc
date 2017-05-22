// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <semaphore.h>

#include <fcntl.h>
#include <sys/mman.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "gtest/gtest.h"

#include "arc/camera_algorithm_bridge.h"

// This class helps to forward the callback to test cases because
// CameraAlgorithmBridge accepts initialization and callback registration once
// and only once.
class CallbackSwitcher : public camera_algorithm_callback_ops_t {
 public:
  static CallbackSwitcher* GetInstance() {
    static CallbackSwitcher switcher;
    return &switcher;
  }

  void RegisterCallback(base::Callback<int32_t(int32_t)> callback) {
    callback_ = callback;
  }

 private:
  CallbackSwitcher() {
    CallbackSwitcher::return_callback =
        CallbackSwitcher::ReturnCallbackForwarder;
  }

  static int32_t ReturnCallbackForwarder(
      const camera_algorithm_callback_ops_t* callback_ops,
      int32_t buffer_handle) {
    if (!callback_ops) {
      return -EINVAL;
    }
    auto s = const_cast<CallbackSwitcher*>(
        static_cast<const CallbackSwitcher*>(callback_ops));
    return s->callback_.Run(buffer_handle);
  }

  base::Callback<int32_t(int32_t)> callback_;
};

// This test should be run against the fake libcam_algo.so created with
// fake_libcam_algo.cc.
class CameraAlgorithmBridgeFixture : public testing::Test {
 public:
  const size_t kShmBufferSize = 2048;

  CameraAlgorithmBridgeFixture()
      : bridge_(arc::CameraAlgorithmBridge::GetInstance()) {
    CallbackSwitcher::GetInstance()->RegisterCallback(base::Bind(
        &CameraAlgorithmBridgeFixture::ReturnCallback, base::Unretained(this)));
    sem_init(&return_sem_, 0, 0);
  }

  ~CameraAlgorithmBridgeFixture() { sem_destroy(&return_sem_); }

 protected:
  arc::CameraAlgorithmBridge* bridge_;

  sem_t return_sem_;

 private:
  int32_t ReturnCallback(int32_t buffer_handle) {
    sem_post(&return_sem_);
    return 0;
  }

  DISALLOW_COPY_AND_ASSIGN(CameraAlgorithmBridgeFixture);
};

TEST_F(CameraAlgorithmBridgeFixture, BasicOperation) {
  int fd = shm_open("/myshm", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  ASSERT_LE(0, fd) << "Failed to create shared memory";
  ASSERT_LE(0, fcntl(fd, F_GETFD));
  ASSERT_EQ(0, ftruncate(fd, kShmBufferSize));
  int32_t handle = bridge_->RegisterBuffer(fd);
  ASSERT_LE(0, handle) << "Handle should be of positive value";
  ASSERT_LE(0, fcntl(fd, F_GETFD));
  std::vector<uint8_t> req_header(8);
  EXPECT_EQ(0, bridge_->Request(req_header, handle));
  struct timespec timeout = {};
  clock_gettime(CLOCK_REALTIME, &timeout);
  timeout.tv_sec += 1;
  ASSERT_EQ(0, sem_timedwait(&return_sem_, &timeout));
  std::vector<int32_t> handles({handle});
  bridge_->DeregisterBuffers(handles);
  close(fd);
  shm_unlink("/myshm");
}

TEST_F(CameraAlgorithmBridgeFixture, InvalidFdOrHandle) {
  int32_t handle = bridge_->RegisterBuffer(-1);
  ASSERT_GT(0, handle) << "Registering invalid fd should have failed";

  int fd = shm_open("/myshm", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  ASSERT_LE(0, fd) << "Failed to create shared memory";
  ASSERT_EQ(0, ftruncate(fd, kShmBufferSize));
  handle = bridge_->RegisterBuffer(fd);
  ASSERT_LE(0, handle) << "Handle should be of positive value";
  std::vector<uint8_t> req_header(8);
  EXPECT_NE(0, bridge_->Request(req_header, handle - 1));
  EXPECT_NE(0, bridge_->Request(req_header, handle + 1));
  std::vector<int32_t> handles({handle});
  bridge_->DeregisterBuffers(handles);
  close(fd);
  shm_unlink("/myshm");

  ASSERT_GT(0, bridge_->RegisterBuffer(fd))
      << "Registering invalid fd should have failed";
}

TEST_F(CameraAlgorithmBridgeFixture, MultiRequests) {
  const uint32_t kNumberOfFds = 256;
  auto GetShmName = [](uint32_t num) {
    std::stringstream ss;
    ss << num;
    return std::string("/myshm") + ss.str();
  };

  std::vector<int> fds, handles;
  for (uint32_t i = 1; i <= kNumberOfFds; i++) {
    fds.push_back(
        shm_open(GetShmName(i).c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR));
    ASSERT_LE(0, fds.back()) << "Failed to create shared memory";
    ASSERT_EQ(0, ftruncate(fds.back(), kShmBufferSize));
    handles.push_back(bridge_->RegisterBuffer(fds.back()));
    ASSERT_LE(0, handles.back()) << "Handle should be of positive value";
  }
  for (const auto handle : handles) {
    std::vector<uint8_t> req_header(8);
    EXPECT_EQ(0, bridge_->Request(req_header, handle));
  }
  struct timespec timeout = {};
  clock_gettime(CLOCK_REALTIME, &timeout);
  timeout.tv_sec += 1;
  for (size_t i = 0; i < handles.size(); ++i) {
    ASSERT_EQ(0, sem_timedwait(&return_sem_, &timeout));
  }
  bridge_->DeregisterBuffers(handles);
  for (const auto fd : fds) {
    close(fd);
  }
  for (uint32_t i = 1; i <= kNumberOfFds; i++) {
    shm_unlink(GetShmName(i).c_str());
  }
}

int main(int argc, char** argv) {
  static base::AtExitManager exit_manager;

  // Set up logging so we can enable VLOGs with -v / --vmodule.
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  auto bridge = arc::CameraAlgorithmBridge::GetInstance();
  if (bridge->Initialize(CallbackSwitcher::GetInstance()) != 0) {
    ADD_FAILURE() << "Failed to initialize camera algorithm bridge";
    return EXIT_FAILURE;
  }

  // Initialize and run all tests
  ::testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();

  return result;
}
