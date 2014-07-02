// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "gobi_cdma_modem.h"
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
    MOCK_METHOD1(EnumerateDevices,
                 std::vector<DBus::Path> (DBus::Error& error));
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

static const gobi::DeviceElement kDeviceElement = {"/device/node", "keykeykey"};

class GobiModemTest : public ::testing::Test {
 public:
  GobiModemTest() : connection_(DBus::Connection::SystemBus()),
                    path_("/Gobi/Mock/0") {
    server_ = new CromoServer(connection_);
    handler_ = new MockGobiModemHandler(*server_);
  }

  void SetUp() {
    error_.reset(new DBus::Error());
    GobiModem::set_handler(handler_);
  }

  void TearDown() {
  }

  scoped_ptr<GobiModem> modem_;
  DBus::Connection connection_;
  DBus::Path path_;
  scoped_ptr<DBus::Error> error_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GobiModemTest);

  MockGobiModemHandler *handler_;
  CromoServer *server_;
};


class SignalStrengthSdk : public gobi::BootstrapSdk {
 public:
  SignalStrengthSdk() : BootstrapSdk() {
    correct_[gobi::kRfiCdma1xRtt] = -60;
    correct_[gobi::kRfiAmps] = -50;
    correct_[gobi::kRfiUmts] = -70;
  }
  MOCK_METHOD1(GetDataBearerTechnology,
      ULONG(ULONG * pDataBearer));
  MOCK_METHOD1(GetSessionState,
      ULONG(ULONG * pState));
  MOCK_METHOD3(GetSignalStrengths,
               ULONG(ULONG * pArraySizes,
                     INT8 * pSignalStrengths,
                     ULONG * pRadioInterfaces));

  static ULONG interfaces_[];
  static INT8 dbms_[];
  GobiModem::StrengthMap correct_;
};

// The fourth elements of these two arrays should be ignored; the SDK
// call returns a length of 3
ULONG SignalStrengthSdk::interfaces_[] = {gobi::kRfiCdma1xRtt,
                                          gobi::kRfiAmps,
                                          gobi::kRfiUmts,
                                          static_cast<ULONG>(-1)};

INT8 SignalStrengthSdk::dbms_[] = {-60, -50, -70, -20};

static void SetupSignalMocks(SignalStrengthSdk *sdk) {
  EXPECT_CALL(*sdk, GetSignalStrengths(_, _, _)).WillOnce(DoAll(
      SetArgumentPointee<0>(3),  // Caller should ignore 4th elements returned
      SetArrayArgument<1>(sdk->dbms_, sdk->dbms_ + 4),
      SetArrayArgument<2>(sdk->interfaces_, sdk->interfaces_ + 4),
      Return(0)));
}

TEST_F(GobiModemTest, GetSignalStrengthDbmDisconnected) {
  SignalStrengthSdk sdk;
  SetupSignalMocks(&sdk);
  modem_.reset(new GobiCdmaModem(connection_,
                                 path_,
                                 kDeviceElement,
                                 &sdk,
                                 NULL));
  modem_->Init();

  EXPECT_CALL(sdk, GetSessionState(_)).WillOnce(DoAll(
      SetArgumentPointee<0>(gobi::kDisconnected),
      Return(0)));

  int master;
  GobiModem::StrengthMap result;
  modem_->GetSignalStrengthDbm(master, &result, *error_);

  EXPECT_FALSE(error_->is_set());
  EXPECT_EQ(-50, master);  // Largest

  EXPECT_THAT(result, ContainerEq(sdk.correct_));
}


TEST_F(GobiModemTest, GetSignalStrengthDbmConnected) {
  SignalStrengthSdk sdk;
  SetupSignalMocks(&sdk);
  modem_.reset(new GobiCdmaModem(connection_,
                                 path_,
                                 kDeviceElement,
                                 &sdk,
                                 NULL));
  modem_->Init();

  EXPECT_CALL(sdk, GetSessionState(_)).WillOnce(DoAll(
      SetArgumentPointee<0>(gobi::kConnected),
      Return(0)));

  // gobi::kDataBearerHsdpaDlHsupaUl translates to kRfiUmts
  EXPECT_CALL(sdk, GetDataBearerTechnology(_)).WillOnce(DoAll(
      SetArgumentPointee<0>(gobi::kDataBearerHsdpaDlHsupaUl),
      Return(0)));

  int master;
  GobiModem::StrengthMap result;
  modem_->GetSignalStrengthDbm(master, &result, *error_);
  EXPECT_FALSE(error_->is_set());

  // Value for RFI technology corresponding to the DataBearer technology
  EXPECT_EQ(sdk.correct_[gobi::kRfiUmts], master);

  EXPECT_THAT(result, ContainerEq(sdk.correct_));
}

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);

  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  LOG(WARNING) << "Running";
  return RUN_ALL_TESTS();
}
