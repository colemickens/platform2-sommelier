// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/scoped_ptr.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "gobi_modem.h"
#include "gobi_modem_handler.h"
#include "mock_gobi_sdk_wrapper.h"

using utilities::DBusPropertyMap;

using ::testing::ContainerEq;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::SetArrayArgument;
using ::testing::StrEq;
using ::testing::_;

class MockGobiModemHandler : public GobiModemHandler {
  public:
    MockGobiModemHandler(CromoServer& server) : GobiModemHandler(server) {}
    MOCK_METHOD1(EnumerateDevices, std::vector<DBus::Path> (DBus::Error& error));
    MOCK_METHOD0(Initialize, bool());
};


// SetQString<argument # for length, argument # for output>
//  To call make the API function OutputAString(unsigned char len, CHAR *text):
//  EXPECT_CALL(sdk_, OutputAString(_, _)).WillOnce(
//      SetQString<0, 1>("value to output")))
ACTION_TEMPLATE(SetQString,
                HAS_2_TEMPLATE_PARAMS(int, k_length, int, k_output),
                AND_1_VALUE_PARAMS(input)) {
  char *output = std::tr1::get<k_output>(args);
  strncpy(output, input, std::tr1::get<k_length>(args));
  output[std::tr1::get<k_length>(args) - 1] = 0;
}

static const DEVICE_ELEMENT kDeviceElement = {"/device/node", "keykeykey"};

class GobiModemTest : public ::testing::Test {
 public:
  GobiModemTest() : connection_(DBus::Connection::SystemBus()),
                    path_("/Gobi/Mock/0") {
    server_ = new CromoServer(connection_);
    handler_ = new MockGobiModemHandler(*server_);
  }

  void SetUp() {
    GobiModem::set_handler(handler_);
    modem_ = new GobiModem(connection_,
                           path_,
                           kDeviceElement,
                           &sdk_);
    error_.reset(new DBus::Error);
  }

  void TearDown() {
    error_.reset(NULL);
    delete modem_;
  }

  void ExpectGetFirmwareInfoWithCarrier(ULONG carrier) {
    EXPECT_CALL(sdk_, GetFirmwareInfo(_, _, _, _, _)).WillOnce(DoAll(
        SetArgumentPointee<0>(1000),
        SetArgumentPointee<1>(1001),
        SetArgumentPointee<2>(carrier),
        SetArgumentPointee<3>(1003),
        SetArgumentPointee<4>(1004),
        Return(0)));
  }

  void ExpectRegisterCallbacks() {
    EXPECT_CALL(sdk_, SetActivationStatusCallback(
        GobiModem::ActivationStatusCallbackTrampoline));
    EXPECT_CALL(sdk_, SetNMEAPlusCallback(
        GobiModem::NmeaPlusCallbackTrampoline));
    EXPECT_CALL(sdk_, SetOMADMStateCallback(
        GobiModem::OmadmStateCallbackTrampoline));
    EXPECT_CALL(sdk_, SetSessionStateCallback(
        GobiModem::SessionStateCallbackTrampoline));
    EXPECT_CALL(sdk_, SetDataBearerCallback(
        GobiModem::DataBearerCallbackTrampoline));
    EXPECT_CALL(sdk_, SetRoamingIndicatorCallback(
        GobiModem::RoamingIndicatorCallbackTrampoline));
    EXPECT_CALL(sdk_, SetSignalStrengthCallback(
        GobiModem::SignalStrengthCallbackTrampoline, 5, _));

  }

  void ExpectLogGobiInformation() {
    EXPECT_CALL(sdk_, GetManufacturer(_, _)).WillOnce(DoAll(
        SetQString<0, 1>("Mock Manufacturer"),
        Return(0)));
    ExpectGetFirmwareInfoWithCarrier(101);
    EXPECT_CALL(sdk_, GetFirmwareRevisions(_, _, _, _, _, _)).WillOnce(DoAll(
        SetQString<0, 1>("AMSS revision"),
        SetQString<2, 3>("BOOT revision"),
        SetQString<4, 5>("PRI revision"),
        Return(0)));
    EXPECT_CALL(sdk_, GetImageStore(_, _)).WillOnce(DoAll(
        SetQString<0, 1>("/Image Store"),
        Return(0)));
    EXPECT_CALL(sdk_, GetSerialNumbers(_, _, _, _, _, _)).WillOnce(DoAll(
        SetQString<0, 1>("MockESN"),
        SetQString<2, 3>("MockIMEI"),
        SetQString<4, 5>("MockMEID"),
        Return(0)));
    EXPECT_CALL(sdk_, GetVoiceNumber(_, _, _, _)).WillOnce(DoAll(
        SetQString<0, 1>("012345"),
        SetQString<2, 3>("567890"),
        Return(0)));
    EXPECT_CALL(sdk_, GetActiveMobileIPProfile(_)).WillOnce(DoAll(
        SetArgumentPointee<0>(1),
        Return(0)));
  }

  GobiModem *modem_;
  gobi::MockSdk sdk_;
  DBus::Connection connection_;
  DBus::Path path_;
  scoped_ptr<DBus::Error> error_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GobiModemTest);

  MockGobiModemHandler *handler_;
  CromoServer *server_;
};

TEST_F(GobiModemTest, EnableFalseToFalse) {
  EXPECT_CALL(sdk_, QCWWANConnect(_, _))
      .Times(0);
  modem_->Enable(false, *error_);
}

TEST_F(GobiModemTest, EnableFalseToTrue) {
  InSequence s;
  EXPECT_CALL(sdk_, QCWWANConnect(
      StrEq(kDeviceElement.deviceNode),
      StrEq(kDeviceElement.deviceKey)));
  ExpectRegisterCallbacks();

  ExpectLogGobiInformation();
  ExpectGetFirmwareInfoWithCarrier(102);

  modem_->Enable(true, *error_);
  ASSERT_FALSE(error_->is_set());
}

TEST_F(GobiModemTest, GetStatusWhenDisabled) {
  DBusPropertyMap map;

  EXPECT_CALL(sdk_, GetActivationState(_)).WillOnce(Return(0));
  ExpectGetFirmwareInfoWithCarrier(102);
  EXPECT_CALL(sdk_, GetSessionState(_)).WillOnce(Return(0));
  EXPECT_CALL(sdk_, GetServingNetwork(_, _, _, _, _, _, _)).WillOnce(Return(0));
  EXPECT_CALL(sdk_, GetSerialNumbers(_, _, _, _, _, _)).WillOnce(Return(1));
  map = modem_->GetStatus(*error_);
  ASSERT_FALSE(error_->is_set());
}

TEST_F(GobiModemTest, GetSignalStrengthDbmDisconnected) {
  ULONG interfaces[] = {1, 0, 9, 3};

  INT8 dbms[] = {-60, -50, -70, -20};

  EXPECT_CALL(sdk_, GetSignalStrengths(_, _, _)).WillOnce(DoAll(
      SetArgumentPointee<0>(3),  // Caller should ignore 4th elements returned
      SetArrayArgument<1>(dbms, dbms + 4),
      SetArrayArgument<2>(interfaces, interfaces + 4),
      Return(0)));

  // Don't do the connected case in this test
  EXPECT_CALL(sdk_, GetSessionState(_)).WillOnce(DoAll(
      SetArgumentPointee<0>(gobi::kDisconnected),
      Return(0)));

  int master;
  GobiModem::StrengthMap result;
  modem_->GetSignalStrengthDbm(master, &result, *error_);

  EXPECT_FALSE(error_->is_set());
  EXPECT_EQ(-50, master);

  GobiModem::StrengthMap correct;
  correct[0] = -50;
  correct[1] = -60;
  correct[9] = -70;
  EXPECT_THAT(result, ContainerEq(correct));
}

TEST_F(GobiModemTest, ResetModem) {
  InSequence s;

  EXPECT_CALL(sdk_, SetPower(gobi::kOffline)).WillOnce(Return(0));
  EXPECT_CALL(sdk_, SetPower(gobi::kReset)).WillOnce(Return(0));

  // Waiting for modem to go down
  EXPECT_CALL(sdk_, QCWWANDisconnect()).WillOnce(Return(0));
  EXPECT_CALL(sdk_, QCWWANConnect(_, _)).WillOnce(Return(0));
  ExpectRegisterCallbacks();
  EXPECT_CALL(sdk_, QCWWANDisconnect()).WillOnce(Return(0));
  EXPECT_CALL(sdk_, QCWWANConnect(_, _)).WillOnce(Return(1));

  // Waiting for modem to come back up
  EXPECT_CALL(sdk_, QCWWANConnect(_, _)).WillOnce(Return(1));
  EXPECT_CALL(sdk_, QCWWANConnect(_, _)).WillOnce(Return(1));
  EXPECT_CALL(sdk_, QCWWANConnect(_, _)).WillOnce(Return(0));
  ExpectRegisterCallbacks();

  modem_->SoftReset(*error_);
  ASSERT_FALSE(error_->is_set());
}

TEST_F(GobiModemTest, PowerCycle) {
  InSequence s;
  EXPECT_CALL(sdk_, SetPower(gobi::kPowerOff));
  modem_->PowerCycle(*error_);
  ASSERT_FALSE(error_->is_set());
}


int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);

  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  LOG(WARNING) << "Running";
  return RUN_ALL_TESTS();
}
