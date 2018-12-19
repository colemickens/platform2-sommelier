// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/diagnosticsd/bind_utils.h"
#include "diagnostics/diagnosticsd/diagnosticsd_ec_event_service.h"
#include "diagnostics/diagnosticsd/ec_constants.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace diagnostics {

namespace {

class MockDiagnosticsdEcEventServiceDelegate
    : public DiagnosticsdEcEventService::Delegate {
 public:
  MOCK_METHOD1(SendEcEventToDiagnosticsProcessor,
               void(const DiagnosticsdEcEventService::EcEvent& ec_event));
};

class DiagnosticsdEcEventServiceTest : public testing::Test {
 protected:
  DiagnosticsdEcEventServiceTest() : service_(&delegate_) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    service()->set_root_dir_for_testing(temp_dir_.GetPath());
    service()->set_event_fd_events_for_testing(POLLIN);
  }

  void TearDown() override {
    base::RunLoop run_loop;
    service_.Shutdown(run_loop.QuitClosure());
    run_loop.Run();
  }

  void CreateSysfsFile() {
    base::FilePath file_path = sysfs_file_path();
    ASSERT_TRUE(base::CreateDirectory(file_path.DirName()));
    ASSERT_EQ(mkfifo(file_path.value().c_str(), 0600), 0);
  }

  base::FilePath sysfs_file_path() {
    return temp_dir_.GetPath().Append(kEcEventSysfsPath);
  }

  // Must be open only after |service_->Start()| call. Otherwise, it will
  // block thread.
  void InitFifoWriteEnd() {
    ASSERT_EQ(fifo_write_end_.get(), -1);
    fifo_write_end_.reset(open(sysfs_file_path().value().c_str(), O_WRONLY));
    ASSERT_NE(fifo_write_end_.get(), -1);
  }

  void WriteEventToSysfsFile(
      const DiagnosticsdEcEventService::EcEvent& ec_event,
      const base::RepeatingClosure& callback) {
    ASSERT_EQ(write(fifo_write_end_.get(), &ec_event, sizeof(ec_event)),
              sizeof(ec_event));

    EXPECT_CALL(delegate_, SendEcEventToDiagnosticsProcessor(ec_event))
        .WillOnce(Invoke(
            [callback](const DiagnosticsdEcEventService::EcEvent& ec_event) {
              callback.Run();
            }));
  }

  MockDiagnosticsdEcEventServiceDelegate* delegate() { return &delegate_; }

  DiagnosticsdEcEventService* service() { return &service_; }

 private:
  base::MessageLoop message_loop_;
  StrictMock<MockDiagnosticsdEcEventServiceDelegate> delegate_;
  DiagnosticsdEcEventService service_;

  base::ScopedTempDir temp_dir_;

  base::ScopedFD fifo_write_end_;
};

}  // namespace

TEST_F(DiagnosticsdEcEventServiceTest, Start) {
  CreateSysfsFile();
  ASSERT_TRUE(service()->Start());
}

TEST_F(DiagnosticsdEcEventServiceTest, StartFailure) {
  ASSERT_FALSE(service()->Start());
}

TEST_F(DiagnosticsdEcEventServiceTest, ReadEvent) {
  CreateSysfsFile();
  ASSERT_TRUE(service()->Start());

  InitFifoWriteEnd();

  base::RunLoop run_loop;
  const uint16_t data[6] = {0xaaaa, 0xbbbb, 0xcccc, 0xdddd, 0xeeee, 0xffff};
  WriteEventToSysfsFile(
      DiagnosticsdEcEventService::EcEvent(0x8888, 0x9999, data),
      run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(DiagnosticsdEcEventServiceTest, ReadManyEvent) {
  CreateSysfsFile();
  ASSERT_TRUE(service()->Start());

  InitFifoWriteEnd();

  base::RunLoop run_loop;
  base::RepeatingClosure callback = BarrierClosure(
      2 /* num_closures */, run_loop.QuitClosure() /* done closure */);
  const uint16_t data1[6] = {0xaaaa, 0xbbbb, 0xcccc, 0xdddd, 0xeeee, 0xffff};
  WriteEventToSysfsFile(
      DiagnosticsdEcEventService::EcEvent(0x8888, 0x9999, data1), callback);
  const uint16_t data2[6] = {0x0000, 0x1111, 0x2222, 0x3333, 0x4444, 0x5555};
  WriteEventToSysfsFile(
      DiagnosticsdEcEventService::EcEvent(0x6666, 0x7777, data2), callback);
  run_loop.Run();
}

}  // namespace diagnostics
