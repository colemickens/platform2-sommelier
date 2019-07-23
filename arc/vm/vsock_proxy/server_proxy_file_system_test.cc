// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/server_proxy_file_system.h"

#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <base/posix/eintr_wrapper.h>
#include <base/posix/unix_domain_socket_linux.h>
#include <base/run_loop.h>
#include <base/threading/thread.h>
#include <brillo/process.h>
#include <gtest/gtest.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"
#include "arc/vm/vsock_proxy/proxy_base.h"
#include "arc/vm/vsock_proxy/proxy_file_system.h"
#include "arc/vm/vsock_proxy/proxy_service.h"
#include "arc/vm/vsock_proxy/vsock_proxy.h"

namespace arc {
namespace {

// Runs the message loop until the given |fd| gets read ready.
void WaitUntilReadable(int fd) {
  base::RunLoop run_loop;
  auto controller =
      base::FileDescriptorWatcher::WatchReadable(fd, run_loop.QuitClosure());
  run_loop.Run();
}

class FakeProxy : public ProxyBase {
 public:
  FakeProxy(VSockProxy::Type type,
            ProxyFileSystem* proxy_file_system,
            base::ScopedFD socket)
      : vsock_proxy_(std::make_unique<VSockProxy>(
            type, proxy_file_system, std::move(socket))) {}

  ~FakeProxy() override = default;

  bool Initialize() override { return true; }
  VSockProxy* GetVSockProxy() override { return vsock_proxy_.get(); }

 private:
  std::unique_ptr<VSockProxy> vsock_proxy_;

  DISALLOW_COPY_AND_ASSIGN(FakeProxy);
};

class FakeFactory : public ProxyService::ProxyFactory {
 public:
  FakeFactory(VSockProxy::Type type,
              ProxyFileSystem* file_system,
              base::ScopedFD socket)
      : type_(type), file_system_(file_system), socket_(std::move(socket)) {}
  ~FakeFactory() override = default;

  std::unique_ptr<ProxyBase> Create() override {
    return std::make_unique<FakeProxy>(type_, file_system_, std::move(socket_));
  }

 private:
  const VSockProxy::Type type_;
  ProxyFileSystem* const file_system_;
  base::ScopedFD socket_;
  DISALLOW_COPY_AND_ASSIGN(FakeFactory);
};

class ServerProxyFileSystemInitWaiter : public ServerProxyFileSystem::Observer {
 public:
  explicit ServerProxyFileSystemInitWaiter(
      ServerProxyFileSystem* proxy_file_system)
      : proxy_file_system_(proxy_file_system),
        event_(base::WaitableEvent::ResetPolicy::MANUAL,
               base::WaitableEvent::InitialState::NOT_SIGNALED) {
    proxy_file_system_->AddObserver(this);
  }

  ~ServerProxyFileSystemInitWaiter() override {
    proxy_file_system_->RemoveObserver(this);
  }

  void Wait() { event_.Wait(); }

  void OnInit() override { event_.Signal(); }

 private:
  ServerProxyFileSystem* const proxy_file_system_;
  base::WaitableEvent event_;

  DISALLOW_COPY_AND_ASSIGN(ServerProxyFileSystemInitWaiter);
};

class ServerProxyFileSystemTest : public testing::Test {
 public:
  ServerProxyFileSystemTest() = default;
  ~ServerProxyFileSystemTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Start ServerProxy with FUSE file system.
    ASSERT_TRUE(mount_dir_.CreateUniqueTempDir());

    auto fake_vsock_pair = CreateSocketPair();
    ASSERT_TRUE(fake_vsock_pair.has_value());

    file_system_ =
        std::make_unique<ServerProxyFileSystem>(mount_dir_.GetPath());

    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    file_system_thread_ = std::make_unique<base::Thread>("Fuse thread");
    ASSERT_TRUE(file_system_thread_->StartWithOptions(options));
    {
      ServerProxyFileSystemInitWaiter waiter(file_system_.get());
      file_system_thread_->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(base::IgnoreResult(&ServerProxyFileSystem::Run),
                         base::Unretained(file_system_.get()),
                         std::make_unique<FakeFactory>(
                             VSockProxy::Type::SERVER, file_system_.get(),
                             std::move(fake_vsock_pair->first))));
      waiter.Wait();
    }

    // Start client VSockProxy.
    client_vsock_fd_ = fake_vsock_pair->second.get();
    client_proxy_service_ = std::make_unique<ProxyService>(
        std::make_unique<FakeFactory>(VSockProxy::Type::CLIENT, nullptr,
                                      std::move(fake_vsock_pair->second)));
    client_proxy_service_->Start();

    // Register initial socket pairs.
    auto server_socket_pair = CreateSocketPair();
    ASSERT_TRUE(server_socket_pair.has_value());
    auto client_socket_pair = CreateSocketPair();
    ASSERT_TRUE(client_socket_pair.has_value());

    int64_t handle = 0;
    file_system_->RunWithVSockProxyInSyncForTesting(base::BindOnce(
        [](base::ScopedFD socket, int64_t* handle, VSockProxy* proxy) {
          *handle = proxy->RegisterFileDescriptor(
              std::move(socket), arc_proxy::FileDescriptor::SOCKET,
              0 /* handle */);
        },
        std::move(server_socket_pair->first), &handle));
    server_fd_ = std::move(server_socket_pair->second);

    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    client_proxy_service_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WaitableEvent* event, ProxyService* proxy_service,
               base::ScopedFD socket, int64_t handle) {
              proxy_service->proxy()->GetVSockProxy()->RegisterFileDescriptor(
                  std::move(socket), arc_proxy::FileDescriptor::SOCKET, handle);
              event->Signal();
            },
            &event, client_proxy_service_.get(),
            std::move(client_socket_pair->first), handle));
    event.Wait();
    client_fd_ = std::move(client_socket_pair->second);
  }

  void TearDown() override {
    // Shut down gracefully. First close the initial socket connection.
    server_fd_.reset();
    WaitUntilReadable(client_fd_.get());
    base::RunLoop().RunUntilIdle();
    client_fd_.reset();

    // Unmount the fuse file system, which lets fuse main loop exit.
    brillo::ProcessImpl process;
    process.AddArg("/usr/bin/fusermount");
    process.AddArg("-u");
    process.AddArg(mount_dir_.GetPath().value());
    ASSERT_EQ(0, process.Run());

    // Wait for the file system shutdown.
    file_system_thread_.reset();
  }

 protected:
  // Temp directory to contain test files.
  const base::FilePath& GetTempDirPath() const { return temp_dir_.GetPath(); }

  int server_fd() const { return server_fd_.get(); }
  int client_fd() const { return client_fd_.get(); }

  int client_vsock_fd() const { return client_vsock_fd_; }

 private:
  base::MessageLoopForIO message_loop_;
  base::FileDescriptorWatcher watcher_{&message_loop_};

  base::ScopedTempDir temp_dir_;

  // Mount point for ServerProxyFileSystem.
  base::ScopedTempDir mount_dir_;

  // ServerProxyFileSystem to be tested.
  std::unique_ptr<ServerProxyFileSystem> file_system_;

  std::unique_ptr<base::Thread> file_system_thread_;

  // Client side VSockProxy.
  std::unique_ptr<ProxyService> client_proxy_service_;

  // Keep the socket file descriptor used for client side fake vsock.
  // The lifetime of the file descriptor is maintained in VSockProxy.
  int client_vsock_fd_ = -1;

  // Socket file descriptor, which is connected to |client_fd_| via VSockProxy
  // as initial connection.
  base::ScopedFD server_fd_;

  // Socket file descriptor, which is connected to |server_fd_| via VSockProxy.
  base::ScopedFD client_fd_;

  DISALLOW_COPY_AND_ASSIGN(ServerProxyFileSystemTest);
};

TEST_F(ServerProxyFileSystemTest, DISABLED_RegularFileReadTest) {
  constexpr char kContentData[] = "abcdefghijklmnopqrstuvwxyz";
  // Remove trailing '\0'.
  constexpr size_t kContentSize = sizeof(kContentData) - 1;
  const base::FilePath test_path = GetTempDirPath().Append("test.txt");
  ASSERT_EQ(kContentSize,
            base::WriteFile(test_path, kContentData, kContentSize));

  // Send a regular file FD of the file from client to server.
  constexpr char kDummyData[] = "123";
  {
    base::ScopedFD attach_fd(
        HANDLE_EINTR(open(test_path.value().c_str(), O_RDONLY)));
    ASSERT_TRUE(base::UnixDomainSocket::SendMsg(
        client_fd(), kDummyData, sizeof(kDummyData), {attach_fd.get()}));
  }

  // Receive the FD from server proxy.
  WaitUntilReadable(server_fd());
  std::vector<base::ScopedFD> fds;
  char buf[10];
  ASSERT_EQ(sizeof(kDummyData), base::UnixDomainSocket::RecvMsg(
                                    server_fd(), buf, sizeof(buf), &fds));
  ASSERT_EQ(1, fds.size());

  ASSERT_EQ(sizeof(buf), HANDLE_EINTR(read(fds[0].get(), buf, sizeof(buf))));
  EXPECT_EQ("abcdefghij", std::string(buf, sizeof(buf)));
  ASSERT_EQ(sizeof(buf), HANDLE_EINTR(read(fds[0].get(), buf, sizeof(buf))));
  EXPECT_EQ("klmnopqrst", std::string(buf, sizeof(buf)));
  ASSERT_EQ(6, HANDLE_EINTR(read(fds[0].get(), buf, sizeof(buf))));
  EXPECT_EQ("uvwxyz", std::string(buf, 6));
  // Make sure EOF.
  ASSERT_EQ(0, HANDLE_EINTR(read(fds[0].get(), buf, sizeof(buf))));

  // Close the file descriptor.
  fds.clear();

  // Then the close message should be sent to the client proxy.
  WaitUntilReadable(client_vsock_fd());
  base::RunLoop().RunUntilIdle();

  // TODO(hidehiko): Make sure if the file descriptor in VSockProxy is gone,
  // when file descriptor registration part is extracted.
}

}  // namespace
}  // namespace arc
