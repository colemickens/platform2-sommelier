// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/proxy_file_system.h"

#include <unistd.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/posix/eintr_wrapper.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/thread.h>
#include <brillo/process.h>
#include <gtest/gtest.h>

#include "arc/vm/vsock_proxy/proxy_base.h"
#include "arc/vm/vsock_proxy/proxy_service.h"

namespace arc {
namespace {

constexpr int64_t kHandle = 123;
constexpr char kTestData[] = "abcdefghijklmnopqrstuvwxyz";

class FakeProxy : public ProxyBase {
 public:
  FakeProxy() = default;
  ~FakeProxy() override = default;
  FakeProxy(const FakeProxy&) = delete;
  FakeProxy& operator=(const FakeProxy&) = delete;

  bool Initialize() override {
    initialized_.Signal();
    return true;
  }

  void WaitUntilInitialized() { initialized_.Wait(); }

 private:
  base::WaitableEvent initialized_{
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};
};

class ProxyFileSystemTest : public testing::Test,
                            public ProxyFileSystem::Delegate {
 public:
  ProxyFileSystemTest() = default;
  ~ProxyFileSystemTest() override = default;
  ProxyFileSystemTest(const ProxyFileSystemTest&) = delete;
  ProxyFileSystemTest& operator=(const ProxyFileSystemTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(mount_dir_.CreateUniqueTempDir());

    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    file_system_thread_ = std::make_unique<base::Thread>("Fuse thread");
    ASSERT_TRUE(file_system_thread_->StartWithOptions(options));

    auto fake_proxy = std::make_unique<FakeProxy>();
    FakeProxy* fake_proxy_ptr = fake_proxy.get();
    file_system_ =
        std::make_unique<ProxyFileSystem>(this, mount_dir_.GetPath());
    file_system_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&ProxyFileSystem::Run),
                       base::Unretained(file_system_.get()),
                       std::make_unique<ProxyService>(std::move(fake_proxy))));
    fake_proxy_ptr->WaitUntilInitialized();
  }

  void TearDown() override {
    // Unmount the fuse file system, which lets fuse main loop exit.
    brillo::ProcessImpl process;
    process.AddArg("/usr/bin/fusermount");
    process.AddArg("-u");
    process.AddArg(mount_dir_.GetPath().value());
    ASSERT_EQ(0, process.Run());

    // Wait for the file system shutdown.
    file_system_thread_.reset();
  }

  // ProxyFileSystem::Delegate overridess:
  void Pread(int64_t handle,
             uint64_t count,
             uint64_t offset,
             PreadCallback callback) override {
    if (handle == kHandle) {
      const std::string data(kTestData);
      offset = std::min(static_cast<uint64_t>(data.size()), offset);
      count = std::min(static_cast<uint64_t>(data.size()) - offset, count);
      std::move(callback).Run(0, data.substr(offset, count));
    } else {
      std::move(callback).Run(EBADF, std::string());
    }
  }
  void Close(int64_t handle) override {
    EXPECT_FALSE(close_was_called_.IsSignaled());
    EXPECT_EQ(kHandle, handle);
    close_was_called_.Signal();
  }
  void Fstat(int64_t handle, FstatCallback callback) override {
    if (handle == kHandle) {
      std::move(callback).Run(0, sizeof(kTestData));
    } else {
      std::move(callback).Run(EBADF, 0);
    }
  }

 protected:
  base::MessageLoopForIO message_loop_;
  base::FileDescriptorWatcher watcher_{&message_loop_};

  // Mount point for ProxyFileSystem.
  base::ScopedTempDir mount_dir_;

  std::unique_ptr<base::Thread> file_system_thread_;

  // ProxyFileSystem to be tested.
  std::unique_ptr<ProxyFileSystem> file_system_;

  base::WaitableEvent close_was_called_{
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};
};

TEST_F(ProxyFileSystemTest, DISABLED_RegularFileReadTest) {
  base::ScopedFD fd = file_system_->RegisterHandle(kHandle);
  char buf[10];
  ASSERT_EQ(sizeof(buf), HANDLE_EINTR(read(fd.get(), buf, sizeof(buf))));
  EXPECT_EQ("abcdefghij", std::string(buf, sizeof(buf)));
  ASSERT_EQ(sizeof(buf), HANDLE_EINTR(read(fd.get(), buf, sizeof(buf))));
  EXPECT_EQ("klmnopqrst", std::string(buf, sizeof(buf)));
  ASSERT_EQ(6, HANDLE_EINTR(read(fd.get(), buf, sizeof(buf))));
  EXPECT_EQ("uvwxyz", std::string(buf, 6));
  // Make sure EOF.
  ASSERT_EQ(0, HANDLE_EINTR(read(fd.get(), buf, sizeof(buf))));

  // Close the file descriptor.
  fd.reset();
  close_was_called_.Wait();
}

}  // namespace
}  // namespace arc
