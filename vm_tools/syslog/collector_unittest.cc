// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>
#include <time.h>

#include <list>
#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/location.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/single_thread_task_runner.h>
#include <base/threading/thread.h>
#include <base/threading/thread_task_runner_handle.h>
#include <base/time/time.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <grpc++/grpc++.h>
#include <gtest/gtest.h>

#include "host.grpc.pb.h"  // NOLINT(build/include)
#include "host.pb.h"       // NOLINT(build/include)
#include "vm_tools/syslog/collector.h"

using std::string;

namespace pb = google::protobuf;

namespace vm_tools {
namespace syslog {
namespace {

using HandleLogsCallback = base::Callback<void(std::unique_ptr<LogRequest>)>;

// Test server that just forwards the protobufs it receives to the main thread.
class FakeLogCollectorService final : public vm_tools::LogCollector::Service {
 public:
  FakeLogCollectorService(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      HandleLogsCallback handle_user_logs_cb,
      HandleLogsCallback handle_kernel_logs_cb);
  ~FakeLogCollectorService() override = default;

  // LogCollector::Service overrides.
  grpc::Status CollectUserLogs(grpc::ServerContext* ctx,
                               const vm_tools::LogRequest* request,
                               vm_tools::EmptyMessage* response) override;
  grpc::Status CollectKernelLogs(grpc::ServerContext* ctx,
                                 const vm_tools::LogRequest* request,
                                 vm_tools::EmptyMessage* response) override;

 private:
  // Task runner for the main thread where tasks will get posted.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Task which will get posted to main_task_runner_ whenever this service
  // receives user logs.
  HandleLogsCallback handle_user_logs_cb_;

  // Task which will get posted to main_task_runner_ whenever this service
  // receives kernel logs.
  HandleLogsCallback handle_kernel_logs_cb_;

  DISALLOW_COPY_AND_ASSIGN(FakeLogCollectorService);
};

FakeLogCollectorService::FakeLogCollectorService(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    HandleLogsCallback handle_user_logs_cb,
    HandleLogsCallback handle_kernel_logs_cb)
    : main_task_runner_(main_task_runner),
      handle_user_logs_cb_(handle_user_logs_cb),
      handle_kernel_logs_cb_(handle_kernel_logs_cb) {}

grpc::Status FakeLogCollectorService::CollectUserLogs(
    grpc::ServerContext* ctx,
    const vm_tools::LogRequest* request,
    vm_tools::EmptyMessage* response) {
  // Make a copy of the request and pass it on.
  auto request_copy = std::make_unique<vm_tools::LogRequest>(*request);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(handle_user_logs_cb_, base::Passed(std::move(request_copy))));

  return grpc::Status::OK;
}

grpc::Status FakeLogCollectorService::CollectKernelLogs(
    grpc::ServerContext* ctx,
    const vm_tools::LogRequest* request,
    vm_tools::EmptyMessage* response) {
  // Make a copy of the request and pass it on.
  auto request_copy = std::make_unique<vm_tools::LogRequest>(*request);
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(handle_kernel_logs_cb_,
                            base::Passed(std::move(request_copy))));

  return grpc::Status::OK;
}

void StartFakeLogCollectorService(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    base::FilePath listen_path,
    base::Callback<void(std::shared_ptr<grpc::Server>)> server_cb,
    HandleLogsCallback handle_user_logs_cb,
    HandleLogsCallback handle_kernel_logs_cb) {
  FakeLogCollectorService log_collector(main_task_runner, handle_user_logs_cb,
                                        handle_kernel_logs_cb);

  grpc::ServerBuilder builder;
  builder.AddListeningPort("unix:" + listen_path.value(),
                           grpc::InsecureServerCredentials());
  builder.RegisterService(&log_collector);

  std::shared_ptr<grpc::Server> server(builder.BuildAndStart().release());
  main_task_runner->PostTask(FROM_HERE, base::Bind(server_cb, server));

  if (server) {
    // This will not race with shutdown because the grpc server code includes a
    // check to see if the server has already been shut down (or is shutting
    // down) when Wait is called.
    server->Wait();
  }
}

constexpr base::FilePath::CharType kServerSocket[] =
    FILE_PATH_LITERAL("server");

// Test fixture for actually testing the Collector functionality.
class CollectorTest : public ::testing::Test {
 public:
  CollectorTest();
  ~CollectorTest() override = default;

 protected:
  // ::testing::Test overrides.
  void SetUp() override;
  void TearDown() override;

 private:
  // Posted back to the main thread by the grpc thread after starting the
  // server.
  void ServerStartCallback(base::Closure quit,
                           std::shared_ptr<grpc::Server> server);

  // Posted back to the main thread by the grpc thread when it receives user
  // logs from the Collector.
  void HandleUserLogs(std::unique_ptr<vm_tools::LogRequest> request);

  // Posted back to the main thread by the grpc thread when it receives kernel
  // logs from the Collector.
  void HandleKernelLogs(std::unique_ptr<vm_tools::LogRequest> request);

  // The message loop for the current thread.  Declared here because it must be
  // the last thing to be cleaned up.
  base::MessageLoopForIO message_loop_;

 protected:
  // Actual Collector being tested.
  std::unique_ptr<Collector> collector_;

  // Socket that tests will use to send the server syslog records.
  base::ScopedFD syslog_socket_;

  // Socket for sending kernel records to the server.
  base::ScopedFD kernel_socket_;

  // Fake boot time to use for kernel log messages.
  base::Time boot_time_;

  // LogRequest that is expected from the server for user logs.
  std::list<std::unique_ptr<vm_tools::LogRequest>> expected_user_requests_;

  // LogRequest that is expected from the server for kernel logs.
  std::list<std::unique_ptr<vm_tools::LogRequest>> expected_kernel_requests_;

  // MessageDifferencer used to compare LogRequests.
  std::unique_ptr<pb::util::MessageDifferencer> message_differencer_;

  // Closure that should be called once all expected LogRequests have been
  // received or on failure.
  base::Closure quit_;

  // Set to true to indicate failure.
  bool failed_;
  string failure_reason_;

 private:
  // Temporary directory where we will store our sockets.
  base::ScopedTempDir temp_dir_;

  // The thread on which the server will run.
  base::Thread server_thread_;

  // grpc::Server that will handle the requests.
  std::shared_ptr<grpc::Server> server_;

  base::WeakPtrFactory<CollectorTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CollectorTest);
};

CollectorTest::CollectorTest()
    : failed_(false),
      server_thread_("gRPC LogCollector Thread"),
      weak_factory_(this) {}

void CollectorTest::SetUp() {
  // Create the temporary directory.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  // Start the FakeLogCollectorService on a different thread.
  base::RunLoop run_loop;

  ASSERT_TRUE(server_thread_.Start());
  server_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&StartFakeLogCollectorService,
                 base::ThreadTaskRunnerHandle::Get(),
                 temp_dir_.GetPath().Append(kServerSocket),
                 base::Bind(&CollectorTest::ServerStartCallback,
                            weak_factory_.GetWeakPtr(), run_loop.QuitClosure()),
                 base::Bind(&CollectorTest::HandleUserLogs,
                            weak_factory_.GetWeakPtr()),
                 base::Bind(&CollectorTest::HandleKernelLogs,
                            weak_factory_.GetWeakPtr())));

  run_loop.Run();

  ASSERT_TRUE(server_);

  // Create the sockets.
  int syslog_fds[2], kernel_fds[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0, syslog_fds), 0);
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0,
                       kernel_fds),
            0);

  // Create the stub to the FakeLogCollectorService.
  std::unique_ptr<vm_tools::LogCollector::Stub> stub =
      vm_tools::LogCollector::NewStub(grpc::CreateChannel(
          "unix:" + temp_dir_.GetPath().Append(kServerSocket).value(),
          grpc::InsecureChannelCredentials()));
  ASSERT_TRUE(stub);

  // Use the current time as the boot time.
  boot_time_ = base::Time::Now();

  // Create the syslog server.
  collector_ = Collector::CreateForTesting(base::ScopedFD(syslog_fds[0]),
                                           base::ScopedFD(kernel_fds[0]),
                                           boot_time_, std::move(stub));
  ASSERT_TRUE(collector_);

  // Store the other end of the socket.
  syslog_socket_.reset(syslog_fds[1]);
  kernel_socket_.reset(kernel_fds[1]);
}

void CollectorTest::TearDown() {
  // Do the opposite of SetUp to make sure things get cleaned up in the right
  // order.
  kernel_socket_.reset();
  syslog_socket_.reset();
  collector_.reset();
  server_->Shutdown();
  server_.reset();
  server_thread_.Stop();
}

void CollectorTest::ServerStartCallback(base::Closure quit,
                                        std::shared_ptr<grpc::Server> server) {
  server_.swap(server);
  quit.Run();
}

void CollectorTest::HandleUserLogs(
    std::unique_ptr<vm_tools::LogRequest> request) {
  CHECK(request);
  if (expected_user_requests_.size() == 0) {
    failed_ = true;
    failure_reason_ = "unexpected user logs";
    quit_.Run();
    return;
  }

  auto expected_request = std::move(expected_user_requests_.front());
  expected_user_requests_.pop_front();

  if (!message_differencer_->Compare(*expected_request, *request)) {
    failed_ = true;
    failure_reason_ = "mismatched user logs";
    quit_.Run();
    return;
  }

  if (expected_user_requests_.size() == 0 &&
      expected_kernel_requests_.size() == 0) {
    quit_.Run();
  }
}

void CollectorTest::HandleKernelLogs(
    std::unique_ptr<vm_tools::LogRequest> request) {
  CHECK(request);
  if (expected_kernel_requests_.size() == 0) {
    failed_ = true;
    failure_reason_ = "unexpected kernel logs";
    quit_.Run();
    return;
  }

  auto expected_request = std::move(expected_kernel_requests_.front());
  expected_kernel_requests_.pop_front();

  if (!message_differencer_->Compare(*expected_request, *request)) {
    failed_ = true;
    failure_reason_ = "mismatched kernel logs";
    quit_.Run();
    return;
  }

  if (expected_kernel_requests_.size() == 0 &&
      expected_user_requests_.size() == 0) {
    quit_.Run();
  }
}

struct TestUserRecord {
  const char* buf;
  struct tm tm;
  vm_tools::LogSeverity severity;
  size_t content_offset;
};

struct TestKernelRecord {
  const char* buf;
  int64_t micros;
  vm_tools::LogSeverity severity;
  size_t content_offset;
};

}  // namespace

// Tests that the end-to-end flow works correctly.  We are not testing for
// poorly-formatted log recrods here because those are tested in the parser
// unittests.
TEST_F(CollectorTest, EndToEnd) {
  // Basic examples from RFC3164.
  const TestUserRecord user_tests[] = {
      {
          .buf = "<34>Oct 11 22:14:15 mymachine su: 'su root' failed for "
                 "lonvick on /dev/pts/8",
          // clang-format off
          .tm = {
              .tm_sec = 15,
              .tm_min = 14,
              .tm_hour = 22,
              .tm_mday = 11,
              .tm_mon = 9,
          },
          // clang-format on
          .severity = vm_tools::CRITICAL,
          .content_offset = 19,
      },
      {
          .buf = "<165>Aug 24 05:34:00 CST 1987 mymachine myproc[10]: %% It's "
                 "time to make the do-nuts.  %%  Ingredients: Mix=OK, Jelly=OK "
                 "# Devices: Mixer=OK, Jelly_Injector=OK, Frier=OK # "
                 "Transport: Conveyer1=OK, Conveyer2=OK # %%",
          // clang-format off
          .tm = {
              .tm_sec = 00,
              .tm_min = 34,
              .tm_hour = 5,
              .tm_mday = 24,
              .tm_mon = 7,
          },
          // clang-format on
          .severity = vm_tools::NOTICE,
          .content_offset = 20,
      },
  };

  // Basic examples from /dev/kmsg on a VM instance.
  const TestKernelRecord kernel_tests[] = {
      {
          .buf = "6,214,500157,-;device-mapper: ioctl: 4.34.0-ioctl "
                 "(2015-10-28) initialised: dm-devel@redhat.com",
          .micros = 500157,
          .severity = vm_tools::INFO,
          .content_offset = 15,
      },
      {
          .buf = "5,279,0,-;Kernel command line: noapic noacpi pci=conf1 "
                 "reboot=k panic=1 i8042.direct=1 i8042.dumbkbd=1 "
                 "i8042.nopnp=1 console=ttyS0 earlyprintk=serial i8042.noaux=1 "
                 "root=/dev/vda rw  container_runtime=kvmtool "
                 "gateway=100.115.92.5 ip_addr=100.115.92.6 "
                 "netmask=255.255.255.252",
          .micros = 0,
          .severity = vm_tools::NOTICE,
          .content_offset = 10,
      },
      {
          .buf = "2,305,408127,-;virtio-pci 0000:00:04.0: virtio_pci: leaving "
                 "for legacy driver\n SUBSYSTEM=pci\n DEVICE=+pci:0000:00:04.0",
          .micros = 408127,
          .severity = vm_tools::CRITICAL,
          .content_offset = 15,
      },
  };

  // Get the current local time so that we can set the year correctly.
  struct timespec ts;
  ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &ts), 0);
  struct tm current_tm;
  ASSERT_TRUE(localtime_r(&ts.tv_sec, &current_tm));

  // Construct the expected user log protobuf.
  auto user_request = std::make_unique<vm_tools::LogRequest>();
  for (TestUserRecord test : user_tests) {
    vm_tools::LogRecord* record = user_request->add_records();
    record->set_severity(test.severity);
    test.tm.tm_year = current_tm.tm_year;
    record->mutable_timestamp()->set_seconds(timelocal(&test.tm));
    record->mutable_timestamp()->set_nanos(0);
    record->set_content(string(&test.buf[test.content_offset]));

    size_t buf_len = strlen(test.buf);
    ASSERT_EQ(send(syslog_socket_.get(), test.buf, buf_len, MSG_NOSIGNAL),
              buf_len);
  }
  expected_user_requests_.push_back(std::move(user_request));

  // Construct the expected kernel log protobuf.
  auto kernel_request = std::make_unique<vm_tools::LogRequest>();
  for (TestKernelRecord test : kernel_tests) {
    vm_tools::LogRecord* record = kernel_request->add_records();
    record->set_severity(test.severity);
    struct timeval tv =
        (boot_time_ + base::TimeDelta::FromMicroseconds(test.micros))
            .ToTimeVal();
    record->mutable_timestamp()->set_seconds(tv.tv_sec);
    record->mutable_timestamp()->set_nanos(
        tv.tv_usec * base::Time::kNanosecondsPerMicrosecond);

    const char* start = &test.buf[test.content_offset];
    const char* end = strchr(start, '\n');
    if (end == nullptr) {
      // All the tests should be properly terminated since they are statically
      // defined.
      end = strchr(start, '\0');
    }
    record->set_content(start, end - start);

    size_t buf_len = strlen(test.buf);
    ASSERT_EQ(write(kernel_socket_.get(), test.buf, buf_len), buf_len);
  }
  expected_kernel_requests_.push_back(std::move(kernel_request));

  // Construct the MessageDifferencer.
  string differences;
  message_differencer_ = std::make_unique<pb::util::MessageDifferencer>();
  message_differencer_->ReportDifferencesToString(&differences);

  failed_ = false;

  // Now wait for things to go through.
  base::RunLoop run_loop;
  quit_ = run_loop.QuitClosure();

  run_loop.Run();

  EXPECT_FALSE(failed_) << "Failure reason: " << failure_reason_ << "\n"
                        << differences;
}

}  // namespace syslog
}  // namespace vm_tools
