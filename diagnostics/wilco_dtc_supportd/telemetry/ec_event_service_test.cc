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

#include "diagnostics/common/bind_utils.h"
#include "diagnostics/wilco_dtc_supportd/ec_constants.h"
#include "diagnostics/wilco_dtc_supportd/telemetry/ec_event_service.h"
#include "mojo/wilco_dtc_supportd.mojom.h"

namespace diagnostics {

namespace {

using testing::_;
using testing::Invoke;
using testing::StrictMock;

using EcEvent = EcEventService::EcEvent;
using EcEventType = EcEventService::Observer::EcEventType;
using MojoEvent = chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdEvent;

class MockEcEventServiceObserver : public EcEventService::Observer {
 public:
  MOCK_METHOD(void, OnEcEvent, (const EcEvent&, EcEventType), (override));
};

class EcEventServiceTest : public testing::Test {
 protected:
  EcEventServiceTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    service()->set_root_dir_for_testing(temp_dir_.GetPath());
    service()->set_event_fd_events_for_testing(POLLIN);
    service()->AddObserver(&observer_);
    EXPECT_TRUE(service()->HasObserver(&observer_));
  }

  void TearDown() override {
    service()->RemoveObserver(&observer_);
    EXPECT_FALSE(service()->HasObserver(&observer_));
    base::RunLoop run_loop;
    service_.ShutDown(run_loop.QuitClosure());
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

  // Must be open only after |service_.Start()| call. Otherwise, it will
  // block thread.
  void InitFifoWriteEnd() {
    ASSERT_EQ(fifo_write_end_.get(), -1);
    fifo_write_end_.reset(open(ec_event_file_path().value().c_str(), O_WRONLY));
    ASSERT_NE(fifo_write_end_.get(), -1);
  }

  void EmitEcEventAndSetObserverExpectations(
      const EcEvent& ec_event,
      EcEventType type,
      const base::RepeatingClosure& callback) {
    ASSERT_EQ(write(fifo_write_end_.get(), &ec_event, sizeof(ec_event)),
              sizeof(ec_event));

    EXPECT_CALL(observer_, OnEcEvent(ec_event, type))
        .WillOnce(Invoke([callback](const EcEvent& ec_event, EcEventType type) {
          callback.Run();
        }));
  }

  EcEventService* service() { return &service_; }

 private:
  base::MessageLoop message_loop_;
  StrictMock<MockEcEventServiceObserver> observer_;
  EcEventService service_;

  base::ScopedTempDir temp_dir_;

  base::ScopedFD fifo_write_end_;
};

TEST_F(EcEventServiceTest, Start) {
  CreateEcEventFile();
  ASSERT_TRUE(service()->Start());
}

TEST_F(EcEventServiceTest, StartFailure) {
  ASSERT_FALSE(service()->Start());
}

// Tests for the EcEventService class that started successfully.
class StartedEcEventServiceTest : public EcEventServiceTest {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(EcEventServiceTest::SetUp());
    CreateEcEventFile();
    ASSERT_TRUE(service()->Start());
    InitFifoWriteEnd();
  }
};

TEST_F(StartedEcEventServiceTest, ReadEvent) {
  base::RunLoop run_loop;
  const uint16_t data[] = {0xaaaa, 0xbbbb, 0xcccc, 0xdddd, 0xeeee, 0xffff};
  EmitEcEventAndSetObserverExpectations(
      EcEvent(0x8888, static_cast<EcEvent::Type>(0x9999), data),
      EcEventType::kNonSysNotification, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(StartedEcEventServiceTest, ReadManyEvent) {
  base::RunLoop run_loop;
  base::RepeatingClosure callback = BarrierClosure(
      2 /* num_closures */, run_loop.QuitClosure() /* done closure */);
  const uint16_t data1[] = {0xaaaa, 0xbbbb, 0xcccc, 0xdddd, 0xeeee, 0xffff};
  EmitEcEventAndSetObserverExpectations(
      EcEvent(0x8888, static_cast<EcEvent::Type>(0x9999), data1),
      EcEventType::kNonSysNotification, callback);
  const uint16_t data2[] = {0x0000, 0x1111, 0x2222, 0x3333, 0x4444, 0x5555};
  EmitEcEventAndSetObserverExpectations(
      EcEvent(0x6666, static_cast<EcEvent::Type>(0x7777), data2),
      EcEventType::kNonSysNotification, callback);
  run_loop.Run();
}

struct EcEventToEcEventTypeTestParams {
  EcEvent source_ec_event;
  EcEventType expected_event_type;
};

class ParsedEcEventStartedEcEventServiceTest
    : public StartedEcEventServiceTest,
      public testing::WithParamInterface<EcEventToEcEventTypeTestParams> {};

// Tests that OnEventAvailable() correctly parses the EC events into the
// corresponding EcEventTypes and are received by the |observers_.OnEcEvent()|
TEST_P(ParsedEcEventStartedEcEventServiceTest, SingleEvents) {
  EcEventToEcEventTypeTestParams test_params = GetParam();
  base::RunLoop run_loop;
  EmitEcEventAndSetObserverExpectations(test_params.source_ec_event,
                                        test_params.expected_event_type,
                                        run_loop.QuitClosure());
  run_loop.Run();
}

// A meaningless and meaningful EcEvent::Type
constexpr auto kGarbageType = static_cast<EcEvent::Type>(0xabcd);
constexpr auto kSystemNotifyType = static_cast<EcEvent::Type>(0x0012);

constexpr uint16_t kEcEventPayloadNonWilcoCharger[]{0x0000, 0x0000, 0x0001,
                                                    0x0000, 0x0000, 0x0000};
constexpr uint16_t kEcEventPayloadBatteryAuth[]{0x0003, 0x0000, 0x0001,
                                                0x0000, 0x0000, 0x0000};
constexpr uint16_t kEcEventPayloadDockDisplay[]{0x0008, 0x0200, 0x0000, 0x0000};
constexpr uint16_t kEcEventPayloadDockThunderbolt[]{0x0008, 0x0000, 0x0000,
                                                    0x0100};
constexpr uint16_t kEcEventPayloadIncompatibleDock[]{0x0008, 0x0000, 0x0000,
                                                     0x1000};
constexpr uint16_t kEcEventPayloadDockError[]{0x0008, 0x0000, 0x0000, 0x8000};

constexpr uint16_t kEcEventPayloadAcAdapterNoFlags[]{0x0000, 0x0000, 0x0000,
                                                     0x0000, 0x0000, 0x0000};
constexpr uint16_t kEcEventPayloadChargerNoFlags[]{0x0003, 0x0000, 0x0000,
                                                   0x0000, 0x0000, 0x0000};
constexpr uint16_t kEcEventPayloadUsbCNoFlags[]{0x0008, 0x0000, 0x0000, 0x0000};

constexpr uint16_t kEcEventPayloadNonWilcoChargerBadSubType[]{
    0xffff, 0x0000, 0x0001, 0x0000, 0x0000, 0x0000};

const EcEventToEcEventTypeTestParams kEcEventToMojoEventTestParams[] = {
    {EcEvent(6, kSystemNotifyType, kEcEventPayloadNonWilcoCharger),
     EcEventType::kNonWilcoCharger},
    {EcEvent(6, kSystemNotifyType, kEcEventPayloadBatteryAuth),
     EcEventType::kBatteryAuth},
    {EcEvent(4, kSystemNotifyType, kEcEventPayloadDockDisplay),
     EcEventType::kDockDisplay},
    {EcEvent(4, kSystemNotifyType, kEcEventPayloadDockThunderbolt),
     EcEventType::kDockThunderbolt},
    {EcEvent(4, kSystemNotifyType, kEcEventPayloadIncompatibleDock),
     EcEventType::kIncompatibleDock},
    {EcEvent(4, kSystemNotifyType, kEcEventPayloadDockError),
     EcEventType::kDockError},
    {EcEvent(6, kGarbageType, kEcEventPayloadNonWilcoCharger),
     EcEventType::kNonSysNotification},
    {EcEvent(4, kSystemNotifyType, kEcEventPayloadAcAdapterNoFlags),
     EcEventType::kSysNotification},
    {EcEvent(4, kSystemNotifyType, kEcEventPayloadChargerNoFlags),
     EcEventType::kSysNotification},
    {EcEvent(4, kSystemNotifyType, kEcEventPayloadUsbCNoFlags),
     EcEventType::kSysNotification},
    {EcEvent(6, kSystemNotifyType, kEcEventPayloadNonWilcoChargerBadSubType),
     EcEventType::kSysNotification},
};

INSTANTIATE_TEST_CASE_P(AllRealAndSomeAlmostEcEvents,
                        ParsedEcEventStartedEcEventServiceTest,
                        testing::ValuesIn(kEcEventToMojoEventTestParams));

}  // namespace

}  // namespace diagnostics
