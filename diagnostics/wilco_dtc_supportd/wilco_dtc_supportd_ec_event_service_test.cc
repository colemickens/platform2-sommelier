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

#include "diagnostics/wilco_dtc_supportd/bind_utils.h"
#include "diagnostics/wilco_dtc_supportd/ec_constants.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_ec_event_service.h"
#include "mojo/wilco_dtc_supportd.mojom.h"

namespace diagnostics {

namespace {

using testing::_;
using testing::Invoke;
using testing::StrictMock;

using EcEvent = WilcoDtcSupportdEcEventService::EcEvent;
using MojoEvent = chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdEvent;

class MockWilcoDtcSupportdEcEventServiceDelegate
    : public WilcoDtcSupportdEcEventService::Delegate {
 public:
  MOCK_METHOD1(SendGrpcEcEventToWilcoDtc, void(const EcEvent& ec_event));
  MOCK_METHOD1(HandleMojoEvent, void(const MojoEvent& mojo_event));
};

class WilcoDtcSupportdEcEventServiceTest : public testing::Test {
 protected:
  WilcoDtcSupportdEcEventServiceTest() : service_(&delegate_) {}

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

  void CreateEcEventFile() {
    base::FilePath file_path = ec_event_file_path();
    ASSERT_TRUE(base::CreateDirectory(file_path.DirName()));
    ASSERT_EQ(mkfifo(file_path.value().c_str(), 0600), 0);
  }

  base::FilePath ec_event_file_path() {
    return temp_dir_.GetPath().Append(kEcEventFilePath);
  }

  // Must be open only after |service_->Start()| call. Otherwise, it will
  // block thread.
  void InitFifoWriteEnd() {
    ASSERT_EQ(fifo_write_end_.get(), -1);
    fifo_write_end_.reset(open(ec_event_file_path().value().c_str(), O_WRONLY));
    ASSERT_NE(fifo_write_end_.get(), -1);
  }

  void WriteEventToEcEventFile(const EcEvent& ec_event,
                               const base::RepeatingClosure& callback) {
    ASSERT_EQ(write(fifo_write_end_.get(), &ec_event, sizeof(ec_event)),
              sizeof(ec_event));

    EXPECT_CALL(delegate_, SendGrpcEcEventToWilcoDtc(ec_event))
        .WillOnce(
            Invoke([callback](const EcEvent& ec_event) { callback.Run(); }));
  }

  MockWilcoDtcSupportdEcEventServiceDelegate* delegate() { return &delegate_; }

  WilcoDtcSupportdEcEventService* service() { return &service_; }

 private:
  base::MessageLoop message_loop_;
  StrictMock<MockWilcoDtcSupportdEcEventServiceDelegate> delegate_;
  WilcoDtcSupportdEcEventService service_;

  base::ScopedTempDir temp_dir_;

  base::ScopedFD fifo_write_end_;
};

TEST_F(WilcoDtcSupportdEcEventServiceTest, Start) {
  CreateEcEventFile();
  ASSERT_TRUE(service()->Start());
}

TEST_F(WilcoDtcSupportdEcEventServiceTest, StartFailure) {
  ASSERT_FALSE(service()->Start());
}

// Tests for the WilcoDtcSupportdEcEventService class that started successfully.
class StartedWilcoDtcSupportdEcEventServiceTest
    : public WilcoDtcSupportdEcEventServiceTest {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(WilcoDtcSupportdEcEventServiceTest::SetUp());
    CreateEcEventFile();
    ASSERT_TRUE(service()->Start());
    InitFifoWriteEnd();
  }
};

TEST_F(StartedWilcoDtcSupportdEcEventServiceTest, ReadEvent) {
  base::RunLoop run_loop;
  const uint16_t data[] = {0xaaaa, 0xbbbb, 0xcccc, 0xdddd, 0xeeee, 0xffff};
  WriteEventToEcEventFile(
      EcEvent(0x8888, static_cast<EcEvent::Type>(0x9999), data),
      run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(StartedWilcoDtcSupportdEcEventServiceTest, ReadManyEvent) {
  base::RunLoop run_loop;
  base::RepeatingClosure callback = BarrierClosure(
      2 /* num_closures */, run_loop.QuitClosure() /* done closure */);
  const uint16_t data1[] = {0xaaaa, 0xbbbb, 0xcccc, 0xdddd, 0xeeee, 0xffff};
  WriteEventToEcEventFile(
      EcEvent(0x8888, static_cast<EcEvent::Type>(0x9999), data1), callback);
  const uint16_t data2[] = {0x0000, 0x1111, 0x2222, 0x3333, 0x4444, 0x5555};
  WriteEventToEcEventFile(
      EcEvent(0x6666, static_cast<EcEvent::Type>(0x7777), data2), callback);
  run_loop.Run();
}

struct EcEventToMojoEventTestParams {
  EcEvent source_ec_event;
  base::Optional<MojoEvent> expected_mojo_event;
};

class EcEventToMojoEventTest
    : public StartedWilcoDtcSupportdEcEventServiceTest,
      public testing::WithParamInterface<EcEventToMojoEventTestParams> {};

// Tests that HandleEvent() correctly interprets EC events, sometimes
// translating them to Mojo events and forwarding them to |delegate_|.
TEST_P(EcEventToMojoEventTest, SingleEvents) {
  EcEventToMojoEventTestParams test_params = GetParam();
  base::RunLoop run_loop;
  WriteEventToEcEventFile(test_params.source_ec_event, run_loop.QuitClosure());
  if (test_params.expected_mojo_event.has_value()) {
    EXPECT_CALL(*delegate(),
                HandleMojoEvent(test_params.expected_mojo_event.value()));
  }
  run_loop.Run();
}

// A meaningless and meaningful EcEvent::Type
constexpr auto kGarbageType = static_cast<EcEvent::Type>(0xabcd);
constexpr auto kSystemNotifyType = static_cast<EcEvent::Type>(0x0012);

// Payloads for EcEvents that should trigger a Mojo event.
constexpr uint16_t kEcEventPayloadNonWilcoCharger[]{0x0000, 0x0000, 0x0001,
                                                    0x0000, 0x0000, 0x0000};
constexpr uint16_t kEcEventPayloadBatteryAuth[]{0x0003, 0x0000, 0x0001,
                                                0x0000, 0x0000, 0x0000};

// These contain the right EcEvent::SystemNotifySubType to trigger a Mojo event,
// but none of the flags are set, so these should not cause a Mojo event.
constexpr uint16_t kEcEventPayloadAcAdapterNoFlags[]{0x0000, 0x0000, 0x0000,
                                                     0x0000, 0x0000, 0x0000};
constexpr uint16_t kEcEventPayloadChargerNoFlags[]{0x0003, 0x0000, 0x0000,
                                                   0x0000, 0x0000, 0x0000};
constexpr uint16_t kEcEventPayloadUsbCNoFlags[]{0x0008, 0x0000, 0x0000, 0x0000};

// Contains the right flags to trigger MojoEvent::kNonWilcoCharger, but the
// EcEvent::SystemNotifySubType is wrong.
constexpr uint16_t kEcEventPayloadNonWilcoChargerBadSubType[]{
    0xffff, 0x0000, 0x0001, 0x0000, 0x0000, 0x0000};

const EcEventToMojoEventTestParams kEcEventToMojoEventTestParams[] = {
    // Each of these EcEvents should trigger a Mojo event.
    {EcEvent(6, kSystemNotifyType, kEcEventPayloadNonWilcoCharger),
     MojoEvent::kNonWilcoCharger},
    {EcEvent(6, kSystemNotifyType, kEcEventPayloadBatteryAuth),
     MojoEvent::kBatteryAuth},
    // These EcEvents should not trigger any mojo events.
    {EcEvent(6, kGarbageType, kEcEventPayloadNonWilcoCharger), base::nullopt},
    {EcEvent(4, kSystemNotifyType, kEcEventPayloadAcAdapterNoFlags),
     base::nullopt},
    {EcEvent(4, kSystemNotifyType, kEcEventPayloadChargerNoFlags),
     base::nullopt},
    {EcEvent(4, kSystemNotifyType, kEcEventPayloadUsbCNoFlags), base::nullopt},
    {EcEvent(6, kSystemNotifyType, kEcEventPayloadNonWilcoChargerBadSubType),
     base::nullopt},
};

INSTANTIATE_TEST_CASE_P(AllRealAndSomeAlmostMojoEvents,
                        EcEventToMojoEventTest,
                        testing::ValuesIn(kEcEventToMojoEventTestParams));

}  // namespace

}  // namespace diagnostics
