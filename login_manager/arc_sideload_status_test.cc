// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/arc_sideload_status.h"

#include <memory>

#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bootlockbox/proto_bindings/boot_lockbox_rpc.pb.h"
#include "login_manager/dbus_test_util.h"
#include "login_manager/mock_object_proxy.h"

using ::testing::_;

namespace login_manager {

ACTION_TEMPLATE(RunCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  return ::testing::get<k>(args).Run(p0);
}

#define EXPECT_DBUS_CALL_THEN_CALLBACK(method_call, response)  \
  EXPECT_CALL(*boot_lockbox_proxy_,                            \
              CallMethod(DBusMethodCallEq(method_call), _, _)) \
      .WillOnce(RunCallback<2>(response));

void EnableCallbackAdaptor(bool* succeeded,
                           char** error,
                           bool s,
                           const char* e) {
  *succeeded = s;
  *error = const_cast<char*>(e);
}

void QueryCallbackAdaptor(bool* enabled, bool e) {
  *enabled = e;
}

class ArcSideloadStatusTest : public ::testing::Test {
 public:
  ArcSideloadStatusTest()
      : boot_lockbox_proxy_(new MockObjectProxy()),
        bootlockbox_read_method_call_(cryptohome::kBootLockboxInterface,
                                      cryptohome::kBootLockboxReadBootLockbox),
        bootlockbox_store_method_call_(
            cryptohome::kBootLockboxInterface,
            cryptohome::kBootLockboxStoreBootLockbox) {}

  ~ArcSideloadStatusTest() override {}

  void SetUp() override {
    arc_sideload_status_ =
        std::make_unique<ArcSideloadStatus>(boot_lockbox_proxy_.get());
  }

 protected:
  bool ParseBoolResponse(dbus::Response* response) {
    bool value;
    dbus::MessageReader reader(response);
    EXPECT_TRUE(reader.PopBool(&value));
    EXPECT_FALSE(reader.HasMoreData());
    return value;
  }

  void ExpectBootLockboxServiceToBeAvailable(bool available) {
    EXPECT_CALL(*boot_lockbox_proxy_, WaitForServiceToBeAvailable(_))
        .WillOnce(RunCallback<0>(available));
  }

  void PretendInitialized() {
    arc_sideload_status_->OverrideAdbSideloadStatusTestOnly(
        true /* not really used */);
  }

  // Returns a valid query response containing the given value in bootlockbox.
  std::unique_ptr<dbus::Response> CreateValidQueryResponse(bool enabled) {
    auto bootlockbox_response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(bootlockbox_response.get());
    cryptohome::BootLockboxBaseReply reply;
    reply.MutableExtension(cryptohome::ReadBootLockboxReply::reply)
        ->set_data(enabled ? "1" : "0");
    EXPECT_TRUE(writer.AppendProtoAsArrayOfBytes(reply));
    return bootlockbox_response;
  }

  // Returns a valid response containing a bootlockbox error.
  std::unique_ptr<dbus::Response> CreateResponseWithBootLockboxError(
      cryptohome::BootLockboxErrorCode error_code) {
    auto bootlockbox_response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(bootlockbox_response.get());
    cryptohome::BootLockboxBaseReply base_reply;
    base_reply.set_error(error_code);
    EXPECT_TRUE(writer.AppendProtoAsArrayOfBytes(base_reply));
    return bootlockbox_response;
  }

  static ArcSideloadStatus::QueryAdbSideloadCallback CaptureQueryCallback(
      bool* enabled) {
    return base::Bind(&QueryCallbackAdaptor, enabled);
  }

  static ArcSideloadStatus::EnableAdbSideloadCallback CaptureEnableCallback(
      bool* enabled, char** error) {
    return base::Bind(&EnableCallbackAdaptor, enabled, error);
  }

  scoped_refptr<MockObjectProxy> boot_lockbox_proxy_;
  std::unique_ptr<ArcSideloadStatus> arc_sideload_status_;
  dbus::MethodCall bootlockbox_read_method_call_;
  dbus::MethodCall bootlockbox_store_method_call_;
};

TEST_F(ArcSideloadStatusTest, IsAdbSideloadAllowed_Default) {
  EXPECT_FALSE(arc_sideload_status_->IsAdbSideloadAllowed());
}

TEST_F(ArcSideloadStatusTest, Initialize_ServiceNotAvailable) {
  ExpectBootLockboxServiceToBeAvailable(false);
  arc_sideload_status_->Initialize();
  EXPECT_FALSE(arc_sideload_status_->IsAdbSideloadAllowed());
  // Expect nothing else.
}

TEST_F(ArcSideloadStatusTest, InitializeThenQueryAdbSideload) {
  // Setup
  ExpectBootLockboxServiceToBeAvailable(true);
  auto bootlockbox_response = CreateValidQueryResponse(true);
  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_read_method_call_,
                                 bootlockbox_response.get());

  // Action
  bool enabled;
  arc_sideload_status_->Initialize();
  arc_sideload_status_->QueryAdbSideload(CaptureQueryCallback(&enabled));

  // Verify
  EXPECT_TRUE(enabled);
  EXPECT_TRUE(arc_sideload_status_->IsAdbSideloadAllowed());
}

TEST_F(ArcSideloadStatusTest, QueryAdbSideloadThenInitialize) {
  // Setup
  ExpectBootLockboxServiceToBeAvailable(true);
  auto bootlockbox_response = CreateValidQueryResponse(true);
  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_read_method_call_,
                                 bootlockbox_response.get());

  // Action
  bool enabled;
  arc_sideload_status_->QueryAdbSideload(CaptureQueryCallback(&enabled));
  arc_sideload_status_->Initialize();

  // Verify
  EXPECT_TRUE(enabled);
  EXPECT_TRUE(arc_sideload_status_->IsAdbSideloadAllowed());
}

TEST_F(ArcSideloadStatusTest, QueryAdbSideloadThenBadInitialize) {
  // Setup
  ExpectBootLockboxServiceToBeAvailable(false);

  // Action
  bool enabled;
  arc_sideload_status_->QueryAdbSideload(
      base::Bind(&QueryCallbackAdaptor, &enabled));
  arc_sideload_status_->Initialize();

  // Verify
  EXPECT_FALSE(enabled);
  EXPECT_FALSE(arc_sideload_status_->IsAdbSideloadAllowed());
}

TEST_F(ArcSideloadStatusTest, MultipleQueryAdbSideloadThenInitialize) {
  // Setup
  ExpectBootLockboxServiceToBeAvailable(true);
  auto bootlockbox_response = CreateValidQueryResponse(true);
  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_read_method_call_,
                                 bootlockbox_response.get());

  // Action
  bool enabled1;
  bool enabled2;
  arc_sideload_status_->QueryAdbSideload(CaptureQueryCallback(&enabled1));
  arc_sideload_status_->QueryAdbSideload(CaptureQueryCallback(&enabled2));
  arc_sideload_status_->Initialize();

  // Verify
  EXPECT_TRUE(enabled1);
  EXPECT_TRUE(enabled2);
  EXPECT_TRUE(arc_sideload_status_->IsAdbSideloadAllowed());
}

TEST_F(ArcSideloadStatusTest, InitializeThenQueryAdbSideload_NullResponse) {
  // Setup
  ExpectBootLockboxServiceToBeAvailable(true);
  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_read_method_call_, nullptr);

  // Action
  bool enabled;
  arc_sideload_status_->Initialize();
  arc_sideload_status_->QueryAdbSideload(CaptureQueryCallback(&enabled));

  // Verify
  EXPECT_FALSE(enabled);
  EXPECT_FALSE(arc_sideload_status_->IsAdbSideloadAllowed());
}

TEST_F(ArcSideloadStatusTest, InitializeThenQueryAdbSideload_BadFormat) {
  // Setup
  ExpectBootLockboxServiceToBeAvailable(true);
  auto bootlockbox_response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(bootlockbox_response.get());
  writer.AppendString("garbage");
  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_read_method_call_,
                                 bootlockbox_response.get());

  // Action
  bool enabled;
  arc_sideload_status_->Initialize();
  arc_sideload_status_->QueryAdbSideload(CaptureQueryCallback(&enabled));

  // Verify
  EXPECT_FALSE(enabled);
  EXPECT_FALSE(arc_sideload_status_->IsAdbSideloadAllowed());
}

TEST_F(ArcSideloadStatusTest, InitializeThenQueryAdbSideload_MissingKey) {
  // Setup
  ExpectBootLockboxServiceToBeAvailable(true);
  auto bootlockbox_response = CreateResponseWithBootLockboxError(
      cryptohome::BOOTLOCKBOX_ERROR_MISSING_KEY);
  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_read_method_call_,
                                 bootlockbox_response.get());

  // Action
  bool enabled;
  arc_sideload_status_->Initialize();
  arc_sideload_status_->QueryAdbSideload(CaptureQueryCallback(&enabled));

  // Verify
  EXPECT_FALSE(enabled);
  EXPECT_FALSE(arc_sideload_status_->IsAdbSideloadAllowed());
}

TEST_F(ArcSideloadStatusTest, EnableAdbSideload_Uninitialized) {
  // Action
  bool succeeded = false;
  char* error;
  arc_sideload_status_->EnableAdbSideload(
      CaptureEnableCallback(&succeeded, &error));

  // Verify
  EXPECT_FALSE(succeeded);
  EXPECT_NE(error, nullptr);
}

TEST_F(ArcSideloadStatusTest, EnableAdbSideload_NullResponse) {
  PretendInitialized();

  // Setup
  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_store_method_call_, nullptr);

  // Action
  bool succeeded = false;
  char* error;
  arc_sideload_status_->EnableAdbSideload(
      CaptureEnableCallback(&succeeded, &error));

  // Verify
  EXPECT_FALSE(succeeded);
  EXPECT_NE(error, nullptr);
}

TEST_F(ArcSideloadStatusTest, EnableAdbSideload_BadFormat) {
  PretendInitialized();

  // Setup
  auto bootlockbox_response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(bootlockbox_response.get());
  writer.AppendString("garbage");
  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_store_method_call_,
                                 bootlockbox_response.get());

  // Action
  bool succeeded = false;
  char* error;
  arc_sideload_status_->EnableAdbSideload(
      CaptureEnableCallback(&succeeded, &error));

  // Verify
  EXPECT_FALSE(succeeded);
  EXPECT_NE(error, nullptr);
}

TEST_F(ArcSideloadStatusTest, EnableAdbSideload_BootLockboxError) {
  PretendInitialized();

  // Setup
  auto bootlockbox_response = CreateResponseWithBootLockboxError(
      cryptohome::BootLockboxErrorCode::BOOTLOCKBOX_ERROR_TPM_COMM_ERROR);
  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_store_method_call_,
                                 bootlockbox_response.get());

  // Action
  bool succeeded = false;
  char* error;
  arc_sideload_status_->EnableAdbSideload(
      CaptureEnableCallback(&succeeded, &error));

  // Verify
  EXPECT_FALSE(succeeded);
  EXPECT_EQ(error, nullptr);
}

TEST_F(ArcSideloadStatusTest, EnableAdbSideload_AlreadyLogin) {
  PretendInitialized();

  // Setup
  // When bootlockbox is finalized (after any user login), store operation will
  // fail with BOOTLOCKBOX_ERROR_CANNOT_STORE.
  auto bootlockbox_response = CreateResponseWithBootLockboxError(
      cryptohome::BootLockboxErrorCode::BOOTLOCKBOX_ERROR_CANNOT_STORE);
  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_store_method_call_,
                                 bootlockbox_response.get());

  // Action
  bool succeeded = false;
  char* error;
  arc_sideload_status_->EnableAdbSideload(
      CaptureEnableCallback(&succeeded, &error));

  // Verify
  EXPECT_FALSE(succeeded);
  EXPECT_EQ(error, nullptr);
}

TEST_F(ArcSideloadStatusTest, EnableAdbSideload_Success) {
  PretendInitialized();

  // Setup: 1st call to enable
  auto bootlockbox_enable_response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(bootlockbox_enable_response.get());
  cryptohome::BootLockboxBaseReply base_reply;
  ASSERT_TRUE(writer.AppendProtoAsArrayOfBytes(base_reply));

  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_store_method_call_,
                                 bootlockbox_enable_response.get());

  // Setup: 2nd call to query
  // NB: This needs to happen to ensure querying from the source of truth.
  auto bootlockbox_query_response = CreateValidQueryResponse(true);
  EXPECT_DBUS_CALL_THEN_CALLBACK(&bootlockbox_read_method_call_,
                                 bootlockbox_query_response.get());

  // Action
  bool succeeded = false;
  char* error;
  arc_sideload_status_->EnableAdbSideload(
      CaptureEnableCallback(&succeeded, &error));

  // Verify
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(error, nullptr);
}

#undef EXPECT_DBUS_CALL_THEN_CALLBACK

}  // namespace login_manager
